#include "sdr/demodulator.hpp"

#include <gtest/gtest.h>

#include <cmath>
#include <cstdint>
#include <numeric>
#include <vector>

namespace {

TEST(Demodulator, ProducesOneAudioSamplePerDecimationGroup) {
    sdr::Demodulator demodulator(sdr::Modulation::nfm, 240'000, 48'000);
    std::vector<std::uint8_t> iq(100, 127);
    const auto result = demodulator.process(iq);
    EXPECT_EQ(result.audio.size(), 10U);
}

TEST(Demodulator, StrongVaryingIqHasHigherLevelThanSmallVaryingIq) {
    sdr::Demodulator weak_demodulator(sdr::Modulation::nfm, 240'000, 48'000);
    sdr::Demodulator strong_demodulator(sdr::Modulation::nfm, 240'000, 48'000);
    std::vector<std::uint8_t> weak;
    std::vector<std::uint8_t> strong;
    for (int n = 0; n < 100; ++n) {
        constexpr int weak_amplitude = 2;
        constexpr int strong_amplitude = 60;
        const int phase = n % 4;
        const int i_sign = phase == 0 ? 1 : (phase == 2 ? -1 : 0);
        const int q_sign = phase == 1 ? -1 : (phase == 3 ? 1 : 0);
        weak.push_back(static_cast<std::uint8_t>(127 + weak_amplitude * i_sign));
        weak.push_back(static_cast<std::uint8_t>(127 + weak_amplitude * q_sign));
        strong.push_back(static_cast<std::uint8_t>(127 + strong_amplitude * i_sign));
        strong.push_back(static_cast<std::uint8_t>(127 + strong_amplitude * q_sign));
    }
    // Demodulator is stateful (FIR history, mixer and decimation phase). Keep
    // the two measurements isolated and evaluate them before the assertion so
    // the test cannot depend on macro argument evaluation order.
    const double weak_level = weak_demodulator.process(weak).signal_dbfs;
    const double strong_level = strong_demodulator.process(strong).signal_dbfs;
    EXPECT_GT(strong_level, weak_level + 20.0);
}

TEST(Demodulator, RejectsIncompleteIqPair) {
    sdr::Demodulator demodulator(sdr::Modulation::am, 240'000, 48'000);
    const std::vector<std::uint8_t> iq{1, 2, 3};
    EXPECT_THROW(demodulator.process(iq), std::invalid_argument);
}

TEST(Demodulator, RejectsTunerDcSpurBeforeDecimation) {
    sdr::Demodulator demodulator(sdr::Modulation::nfm, 240'000, 48'000);
    // A constant raw IQ value models the tuner's zero-frequency DC spur. After
    // Fs/4 mixing it must be removed by the channel filter, not aliased to audio.
    const std::vector<std::uint8_t> tuner_dc(20'000, 200);
    demodulator.process(tuner_dc);
    const auto settled = demodulator.process(tuner_dc);
    EXPECT_LT(settled.signal_dbfs, -60.0);
}

std::vector<std::uint8_t> modulated_fm(double audio_frequency_hz,
                                       double iq_rate,
                                       int sample_count,
                                       double deviation_hz) {
    constexpr double pi = 3.14159265358979323846;
    std::vector<std::uint8_t> iq;
    iq.reserve(static_cast<std::size_t>(sample_count) * 2);
    for (int n = 0; n < sample_count; ++n) {
        const double phase =
            -pi * n / 2.0 +
            (deviation_hz / audio_frequency_hz) *
                std::sin(2.0 * pi * audio_frequency_hz * n / iq_rate);
        iq.push_back(static_cast<std::uint8_t>(
            std::lround(127.5 + 100.0 * std::cos(phase))));
        iq.push_back(static_cast<std::uint8_t>(
            std::lround(127.5 + 100.0 * std::sin(phase))));
    }
    return iq;
}

std::vector<std::uint8_t> modulated_nfm(double audio_frequency_hz) {
    return modulated_fm(audio_frequency_hz, 240'000.0, 24'000, 2'500.0);
}

double tail_rms(const std::vector<std::int16_t>& samples) {
    const auto begin = samples.begin() + static_cast<std::ptrdiff_t>(samples.size() / 2);
    const double square_sum =
        std::accumulate(begin, samples.end(), 0.0, [](double sum, std::int16_t value) {
            return sum + static_cast<double>(value) * value;
        });
    return std::sqrt(square_sum / static_cast<double>(samples.end() - begin));
}

TEST(Demodulator, KeepsSpeechToneAndRejectsHighFrequencyAudio) {
    sdr::Demodulator speech_demodulator(sdr::Modulation::nfm, 240'000, 48'000);
    sdr::Demodulator high_demodulator(sdr::Modulation::nfm, 240'000, 48'000);
    const auto speech =
        speech_demodulator.process(modulated_nfm(1'000.0)).audio;
    const auto high =
        high_demodulator.process(modulated_nfm(10'000.0)).audio;
    const double speech_rms = tail_rms(speech);
    const double high_rms = tail_rms(high);
    EXPECT_GT(speech_rms, 1'000.0);
    EXPECT_LT(high_rms, speech_rms * 0.1);
}

TEST(Demodulator, OptionalNotchRejectsPersistentInterferenceTone) {
    sdr::Demodulator unfiltered(sdr::Modulation::nfm, 240'000, 48'000);
    sdr::Demodulator filtered(sdr::Modulation::nfm, 240'000, 48'000, 1'890.0,
                              120.0);
    const auto tone = modulated_nfm(1'890.0);
    const auto original = unfiltered.process(tone).audio;
    const auto notched = filtered.process(tone).audio;
    const double original_rms = tail_rms(original);
    const double notched_rms = tail_rms(notched);
    EXPECT_LT(notched_rms, original_rms * 0.1);
}

TEST(Demodulator, WideFmPreservesBroadcastBandAudioAtFortyEightKhz) {
    sdr::Demodulator demodulator(sdr::Modulation::wfm, 1'200'000, 48'000);
    const auto iq = modulated_fm(10'000.0, 1'200'000.0, 120'000, 50'000.0);
    const auto result = demodulator.process(iq);

    EXPECT_EQ(result.audio.size(), 4'800U);
    EXPECT_GT(tail_rms(result.audio), 1'000.0);
}

}  // namespace
