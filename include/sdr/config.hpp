#pragma once

#include <cstdint>
#include <filesystem>
#include <stdexcept>
#include <string>

namespace sdr {

enum class Modulation { nfm, am };

struct Config {
    std::uint32_t frequency_hz{};
    Modulation modulation{Modulation::nfm};
    double squelch_dbfs{-35.0};
    double silence_seconds{10.0};
    std::filesystem::path output_directory{"recordings"};
    int device_index{0};
    int ppm{0};
    double gain_db{-1.0};
    std::uint32_t iq_sample_rate{240'000};
    std::uint32_t audio_sample_rate{48'000};
    int mp3_bitrate_kbps{64};
    bool level_meter{false};
    double notch_frequency_hz{0.0};
    double notch_width_hz{120.0};
};

class UsageRequested final : public std::exception {
public:
    const char* what() const noexcept override { return "usage requested"; }
};

Config parse_arguments(int argc, char** argv);
std::string usage(const char* program);

}  // namespace sdr
