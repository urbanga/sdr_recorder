#include "sdr/demodulator.hpp"

#include <algorithm>
#include <cmath>
#include <complex>
#include <limits>
#include <stdexcept>

namespace sdr {
namespace {

std::int16_t to_pcm(float value) {
    value = std::clamp(value, -1.0F, 1.0F);
    return static_cast<std::int16_t>(std::lround(value * 32767.0F));
}

std::vector<float> lowpass_filter(std::size_t tap_count, double cutoff_hz,
                                  double sample_rate) {
    constexpr double pi = 3.14159265358979323846;
    constexpr double two_pi = 2.0 * pi;
    const double normalized_cutoff = cutoff_hz / sample_rate;
    const auto midpoint = static_cast<double>(tap_count - 1) / 2.0;
    std::vector<float> coefficients(tap_count);
    double coefficient_sum = 0.0;
    for (std::size_t n = 0; n < tap_count; ++n) {
        const double offset = static_cast<double>(n) - midpoint;
        const double sinc =
            offset == 0.0
                ? 2.0 * normalized_cutoff
                : std::sin(two_pi * normalized_cutoff * offset) / (pi * offset);
        const double hamming =
            0.54 - 0.46 * std::cos(two_pi * static_cast<double>(n) /
                                   static_cast<double>(tap_count - 1));
        coefficients[n] = static_cast<float>(sinc * hamming);
        coefficient_sum += coefficients[n];
    }
    for (auto& coefficient : coefficients) {
        coefficient = static_cast<float>(coefficient / coefficient_sum);
    }
    return coefficients;
}

}  // namespace

Demodulator::Demodulator(Modulation modulation, std::uint32_t iq_sample_rate,
                         std::uint32_t audio_sample_rate, double notch_frequency_hz,
                         double notch_width_hz)
    : modulation_(modulation) {
    if (audio_sample_rate == 0 || iq_sample_rate % audio_sample_rate != 0) {
        throw std::invalid_argument("IQ sample rate must be an integer multiple of audio rate");
    }
    const std::uint32_t channel_sample_rate =
        modulation == Modulation::wfm ? 240'000U : audio_sample_rate;
    if (iq_sample_rate % channel_sample_rate != 0 ||
        channel_sample_rate % audio_sample_rate != 0) {
        throw std::invalid_argument(
            "Sample rates do not support the required demodulation stages");
    }
    channel_decimation_ = iq_sample_rate / channel_sample_rate;
    audio_decimation_ = channel_sample_rate / audio_sample_rate;

    constexpr double pi = 3.14159265358979323846;
    const double demodulated_period = 1.0 / channel_sample_rate;
    const double deemphasis_time = modulation == Modulation::am ? 0.0 : 50e-6;
    audio_lowpass_alpha_ = deemphasis_time == 0.0
                               ? 1.0F
                               : static_cast<float>(demodulated_period /
                                                    (deemphasis_time +
                                                     demodulated_period));
    const double highpass_hz =
        modulation == Modulation::wfm ? 30.0 : 150.0;
    audio_highpass_decay_ = static_cast<float>(
        std::exp(-2.0 * pi * highpass_hz / audio_sample_rate));
    const double audio_cutoff_hz =
        modulation == Modulation::wfm ? 15'000.0 : 3'400.0;
    const std::size_t audio_filter_taps =
        modulation == Modulation::wfm ? 129U : 101U;
    audio_filter_ =
        lowpass_filter(audio_filter_taps, audio_cutoff_hz, channel_sample_rate);
    audio_history_.resize(audio_filter_.size());

    if (notch_frequency_hz > 0.0) {
        if (notch_frequency_hz >= audio_sample_rate / 2.0 || notch_width_hz <= 0.0) {
            throw std::invalid_argument("Invalid audio notch configuration");
        }
        const double angular_frequency =
            2.0 * pi * notch_frequency_hz / audio_sample_rate;
        const double quality = notch_frequency_hz / notch_width_hz;
        const double alpha = std::sin(angular_frequency) / (2.0 * quality);
        const double a0 = 1.0 + alpha;
        notch_b0_ = static_cast<float>(1.0 / a0);
        notch_b1_ =
            static_cast<float>(-2.0 * std::cos(angular_frequency) / a0);
        notch_b2_ = notch_b0_;
        notch_a1_ = notch_b1_;
        notch_a2_ = static_cast<float>((1.0 - alpha) / a0);
        notch_enabled_ = true;
    }

    // A real channel filter is essential here: after Fs/4 offset tuning the
    // RTL-SDR DC spur sits at Fs/4. A short boxcar would alias part of it into
    // the audio band during decimation (12 kHz with the default rates).
    const double cutoff_hz = modulation == Modulation::wfm
                                 ? 100'000.0
                                 : (modulation == Modulation::nfm ? 12'000.0
                                                                  : 10'000.0);
    channel_filter_ = lowpass_filter(129, cutoff_hz, iq_sample_rate);
    channel_history_.resize(channel_filter_.size());
}

DemodulatedBlock Demodulator::process(std::span<const std::uint8_t> iq) {
    if (iq.size() % 2 != 0) throw std::invalid_argument("IQ data must contain I/Q pairs");

    const std::size_t complex_count = iq.size() / 2;
    std::vector<std::complex<float>> channel;
    channel.reserve(complex_count / channel_decimation_ + 1);

    // The tuner is deliberately placed Fs/4 above the requested channel to
    // avoid its DC spike. Mix that channel back to DC before low-pass decimation.
    for (std::size_t n = 0; n < complex_count; ++n) {
        const float i = (static_cast<float>(iq[2 * n]) - 127.5F) / 127.5F;
        const float q = (static_cast<float>(iq[2 * n + 1]) - 127.5F) / 127.5F;
        const std::complex<float> sample{i, q};
        std::complex<float> mixed;
        switch (oscillator_phase_++ & 3U) {
            case 0: mixed = sample; break;
            case 1: mixed = {-q, i}; break;
            case 2: mixed = -sample; break;
            default: mixed = {q, -i}; break;
        }

        channel_history_[channel_history_position_] = mixed;
        channel_history_position_ =
            (channel_history_position_ + 1) % channel_history_.size();

        if (++decimation_phase_ != channel_decimation_) continue;
        decimation_phase_ = 0;

        std::complex<float> filtered{};
        std::size_t history_index = channel_history_position_;
        for (std::size_t tap = 0; tap < channel_filter_.size(); ++tap) {
            if (history_index == 0) history_index = channel_history_.size();
            --history_index;
            filtered += channel_history_[history_index] * channel_filter_[tap];
        }
        channel.push_back(filtered);
    }

    double power = 0.0;
    for (const auto sample : channel) power += std::norm(sample);
    power = channel.empty() ? 0.0 : power / channel.size();
    const double signal_dbfs =
        10.0 * std::log10(std::max(power / 2.0, std::numeric_limits<double>::min()));

    DemodulatedBlock result;
    result.signal_dbfs = signal_dbfs;
    result.audio.reserve(channel.size() / audio_decimation_ + 1);

    constexpr float pi = 3.14159265358979323846F;
    for (const auto sample : channel) {
        const float avg_i = sample.real();
        const float avg_q = sample.imag();

        float audio = 0.0F;
        if (modulation_ == Modulation::nfm || modulation_ == Modulation::wfm) {
            const float cross = previous_i_ * avg_q - previous_q_ * avg_i;
            const float dot = previous_i_ * avg_i + previous_q_ * avg_q;
            // With 2.5 kHz NFM deviation at 48 kHz the raw discriminator peak
            // is only about 0.1. Restore useful PCM headroom before filtering.
            const float discriminator_gain =
                modulation_ == Modulation::wfm ? 1.4F : 6.0F;
            audio = discriminator_gain * std::atan2(cross, dot) / pi;
            previous_i_ = avg_i;
            previous_q_ = avg_q;
        } else {
            const float magnitude = std::hypot(avg_i, avg_q);
            am_dc_ += 0.001F * (magnitude - am_dc_);
            audio = (magnitude - am_dc_) * 3.0F;
        }

        // FM broadcasting and European NFM both use 50 us de-emphasis.
        audio_lp_ += audio_lowpass_alpha_ * (audio - audio_lp_);

        audio_history_[audio_history_position_] = audio_lp_;
        audio_history_position_ =
            (audio_history_position_ + 1) % audio_history_.size();

        if (++audio_decimation_phase_ != audio_decimation_) continue;
        audio_decimation_phase_ = 0;

        float filtered_audio = 0.0F;
        std::size_t history_index = audio_history_position_;
        for (std::size_t tap = 0; tap < audio_filter_.size(); ++tap) {
            if (history_index == 0) history_index = audio_history_.size();
            --history_index;
            filtered_audio += audio_history_[history_index] * audio_filter_[tap];
        }

        // Remove discriminator DC, CTCSS and low-frequency rumble.
        float output_audio =
            audio_highpass_decay_ *
            (audio_hp_state_ + filtered_audio - audio_hp_previous_input_);
        audio_hp_previous_input_ = filtered_audio;
        audio_hp_state_ = output_audio;

        if (notch_enabled_) {
            const float first_stage =
                notch_b0_ * output_audio + notch_b1_ * notch_x1_ +
                notch_b2_ * notch_x2_ - notch_a1_ * notch_y1_ -
                notch_a2_ * notch_y2_;
            notch_x2_ = notch_x1_;
            notch_x1_ = output_audio;
            notch_y2_ = notch_y1_;
            notch_y1_ = first_stage;

            const float second_stage =
                notch_b0_ * first_stage + notch_b1_ * notch_second_x1_ +
                notch_b2_ * notch_second_x2_ -
                notch_a1_ * notch_second_y1_ -
                notch_a2_ * notch_second_y2_;
            notch_second_x2_ = notch_second_x1_;
            notch_second_x1_ = first_stage;
            notch_second_y2_ = notch_second_y1_;
            notch_second_y1_ = second_stage;
            output_audio = second_stage;
        }
        result.audio.push_back(to_pcm(output_audio));
    }
    return result;
}

}  // namespace sdr
