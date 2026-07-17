#pragma once

#include "sdr/config.hpp"

#include <cstdint>
#include <span>
#include <vector>

namespace sdr {

struct DemodulatedBlock {
    std::vector<std::int16_t> audio;
    double signal_dbfs;
};

class Demodulator {
public:
    Demodulator(Modulation modulation, std::uint32_t iq_sample_rate,
                std::uint32_t audio_sample_rate);
    DemodulatedBlock process(std::span<const std::uint8_t> interleaved_iq);

private:
    Modulation modulation_;
    std::uint32_t decimation_;
    float previous_i_{1.0F};
    float previous_q_{0.0F};
    float am_dc_{0.0F};
    float audio_lp_{0.0F};
    std::uint32_t oscillator_phase_{0};
};

}  // namespace sdr
