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
    sdr::Demodulator demodulator(sdr::Modulation::nfm, 240'000, 48'000);
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
    EXPECT_GT(demodulator.process(strong).signal_dbfs,
              demodulator.process(weak).signal_dbfs + 20.0);
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

std::vector<std::uint8_t> modulated_nfm(double audio_frequency_hz) {
    constexpr double pi = 3.14159265358979323846;
    constexpr double iq_rate = 240'000.0;
    constexpr double deviation_hz = 2'500.0;
    std::vector<std::uint8_t> iq;
    iq.reserve(48'000);
    for (int n = 0; n < 24'000; ++n) {
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
    EXPECT_GT(tail_rms(speech), 1'000.0);
    EXPECT_LT(tail_rms(high), tail_rms(speech) * 0.1);
}

TEST(Demodulator, OptionalNotchRejectsPersistentInterferenceTone) {
    sdr::Demodulator unfiltered(sdr::Modulation::nfm, 240'000, 48'000);
    sdr::Demodulator filtered(sdr::Modulation::nfm, 240'000, 48'000, 1'890.0,
                              120.0);
    const auto tone = modulated_nfm(1'890.0);
    const auto original = unfiltered.process(tone).audio;
    const auto notched = filtered.process(tone).audio;
    EXPECT_LT(tail_rms(notched), tail_rms(original) * 0.1);
}

}  // namespace
