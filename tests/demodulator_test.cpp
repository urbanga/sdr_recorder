#include "sdr/demodulator.hpp"

#include <gtest/gtest.h>

#include <cmath>
#include <cstdint>
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

}  // namespace
