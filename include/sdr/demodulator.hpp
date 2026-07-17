#pragma once

#include "sdr/config.hpp"

#include <cstdint>
#include <complex>
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
                std::uint32_t audio_sample_rate, double notch_frequency_hz = 0.0,
                double notch_width_hz = 120.0);
    DemodulatedBlock process(std::span<const std::uint8_t> interleaved_iq);

private:
    Modulation modulation_;
    std::uint32_t decimation_;
    float previous_i_{1.0F};
    float previous_q_{0.0F};
    float am_dc_{0.0F};
    float audio_lp_{0.0F};
    float audio_hp_previous_input_{0.0F};
    float audio_hp_state_{0.0F};
    float audio_lowpass_alpha_{0.0F};
    float audio_highpass_decay_{0.0F};
    std::vector<float> audio_filter_;
    std::vector<float> audio_history_;
    std::size_t audio_history_position_{0};
    bool notch_enabled_{false};
    float notch_b0_{1.0F};
    float notch_b1_{0.0F};
    float notch_b2_{0.0F};
    float notch_a1_{0.0F};
    float notch_a2_{0.0F};
    float notch_x1_{0.0F};
    float notch_x2_{0.0F};
    float notch_y1_{0.0F};
    float notch_y2_{0.0F};
    float notch_second_x1_{0.0F};
    float notch_second_x2_{0.0F};
    float notch_second_y1_{0.0F};
    float notch_second_y2_{0.0F};
    std::uint32_t oscillator_phase_{0};
    std::uint32_t decimation_phase_{0};
    std::vector<float> channel_filter_;
    std::vector<std::complex<float>> channel_history_;
    std::size_t channel_history_position_{0};
};

}  // namespace sdr
