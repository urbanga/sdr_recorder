#pragma once

#include "sdr/audio_sink.hpp"

#include <cstdint>
#include <filesystem>
#include <memory>

namespace sdr {

class Mp3SinkFactory final : public AudioSinkFactory {
public:
    Mp3SinkFactory(std::filesystem::path output_directory,
                   std::uint32_t frequency_hz, std::uint32_t sample_rate,
                   int bitrate_kbps);
    std::unique_ptr<AudioSink> create() override;

private:
    std::filesystem::path output_directory_;
    std::uint32_t frequency_hz_;
    std::uint32_t sample_rate_;
    int bitrate_kbps_;
};

}  // namespace sdr
