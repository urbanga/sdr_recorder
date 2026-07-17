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

}  // namespace

Demodulator::Demodulator(Modulation modulation, std::uint32_t iq_sample_rate,
                         std::uint32_t audio_sample_rate)
    : modulation_(modulation) {
    if (audio_sample_rate == 0 || iq_sample_rate % audio_sample_rate != 0) {
        throw std::invalid_argument("IQ sample rate must be an integer multiple of audio rate");
    }
    decimation_ = iq_sample_rate / audio_sample_rate;
}

DemodulatedBlock Demodulator::process(std::span<const std::uint8_t> iq) {
    if (iq.size() % 2 != 0) throw std::invalid_argument("IQ data must contain I/Q pairs");

    const std::size_t complex_count = iq.size() / 2;
    std::vector<std::complex<float>> channel;
    channel.reserve(complex_count / decimation_);

    // The tuner is deliberately placed Fs/4 above the requested channel to
    // avoid its DC spike. Mix that channel back to DC before low-pass decimation.
    for (std::size_t start = 0; start + decimation_ <= complex_count;
         start += decimation_) {
        std::complex<float> average{};
        for (std::size_t j = 0; j < decimation_; ++j) {
            const float i =
                (static_cast<float>(iq[2 * (start + j)]) - 127.5F) / 127.5F;
            const float q =
                (static_cast<float>(iq[2 * (start + j) + 1]) - 127.5F) / 127.5F;
            const std::complex<float> sample{i, q};
            switch (oscillator_phase_++ & 3U) {
                case 0: average += sample; break;
                case 1: average += std::complex<float>{-q, i}; break;
                case 2: average += -sample; break;
                default: average += std::complex<float>{q, -i}; break;
            }
        }
        channel.push_back(average / static_cast<float>(decimation_));
    }

    double power = 0.0;
    for (const auto sample : channel) power += std::norm(sample);
    power = channel.empty() ? 0.0 : power / channel.size();
    const double signal_dbfs =
        10.0 * std::log10(std::max(power / 2.0, std::numeric_limits<double>::min()));

    DemodulatedBlock result;
    result.signal_dbfs = signal_dbfs;
    result.audio.reserve(channel.size());

    constexpr float pi = 3.14159265358979323846F;
    for (const auto sample : channel) {
        const float avg_i = sample.real();
        const float avg_q = sample.imag();

        float audio = 0.0F;
        if (modulation_ == Modulation::nfm) {
            const float cross = previous_i_ * avg_q - previous_q_ * avg_i;
            const float dot = previous_i_ * avg_i + previous_q_ * avg_q;
            audio = std::atan2(cross, dot) / pi;
            previous_i_ = avg_i;
            previous_q_ = avg_q;
        } else {
            const float magnitude = std::hypot(avg_i, avg_q);
            am_dc_ += 0.001F * (magnitude - am_dc_);
            audio = (magnitude - am_dc_) * 3.0F;
        }

        // A simple speech-band smoothing filter also reduces decimation artifacts.
        audio_lp_ += 0.35F * (audio - audio_lp_);
        result.audio.push_back(to_pcm(audio_lp_));
    }
    return result;
}

}  // namespace sdr
