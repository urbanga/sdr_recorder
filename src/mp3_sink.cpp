#include "sdr/mp3_sink.hpp"

#include <chrono>
#include <cstdio>
#include <filesystem>
#include <iomanip>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

// The small stable libmp3lame C ABI used here is declared locally so the runtime
// library is sufficient; invoking the ffmpeg or lame executables is not required.
extern "C" {
struct lame_global_struct;
using lame_global_flags = struct lame_global_struct;
lame_global_flags* lame_init();
int lame_set_in_samplerate(lame_global_flags*, int);
int lame_set_num_channels(lame_global_flags*, int);
int lame_set_brate(lame_global_flags*, int);
int lame_set_quality(lame_global_flags*, int);
int lame_init_params(lame_global_flags*);
int lame_encode_buffer(lame_global_flags*, const short int[], const short int[], int,
                       unsigned char*, int);
int lame_encode_flush(lame_global_flags*, unsigned char*, int);
int lame_close(lame_global_flags*);
}

namespace sdr {
namespace {

class Mp3Sink final : public AudioSink {
public:
    Mp3Sink(const std::filesystem::path& path, std::uint32_t sample_rate, int bitrate)
        : path_(path), encoder_(lame_init()) {
        if (!encoder_) throw std::runtime_error("Could not initialize MP3 encoder");
        file_ = std::fopen(path.string().c_str(), "wb");
        if (!file_) {
            lame_close(encoder_);
            encoder_ = nullptr;
            throw std::runtime_error("Could not create " + path.string());
        }
        if (lame_set_in_samplerate(encoder_, static_cast<int>(sample_rate)) < 0 ||
            lame_set_num_channels(encoder_, 1) < 0 ||
            lame_set_brate(encoder_, bitrate) < 0 ||
            lame_set_quality(encoder_, 2) < 0 || lame_init_params(encoder_) < 0) {
            cleanup();
            throw std::runtime_error("Could not configure MP3 encoder");
        }
    }

    ~Mp3Sink() override {
        try {
            close();
        } catch (...) {
        }
    }

    void write(std::span<const std::int16_t> samples) override {
        if (!encoder_) throw std::logic_error("Writing to a closed MP3 file");
        if (samples.empty()) return;
        std::vector<unsigned char> mp3(samples.size() * 5 / 4 + 7200);
        const int encoded = lame_encode_buffer(
            encoder_, samples.data(), samples.data(), static_cast<int>(samples.size()),
            mp3.data(), static_cast<int>(mp3.size()));
        if (encoded < 0) throw std::runtime_error("MP3 encoding failed");
        write_bytes(mp3.data(), static_cast<std::size_t>(encoded));
    }

    void close() override {
        if (!encoder_) return;
        std::vector<unsigned char> mp3(7200);
        const int encoded =
            lame_encode_flush(encoder_, mp3.data(), static_cast<int>(mp3.size()));
        if (encoded < 0) {
            cleanup();
            throw std::runtime_error("Flushing MP3 encoder failed");
        }
        write_bytes(mp3.data(), static_cast<std::size_t>(encoded));
        cleanup();
    }

private:
    void write_bytes(const unsigned char* data, std::size_t size) {
        if (size != 0 && std::fwrite(data, 1, size, file_) != size) {
            throw std::runtime_error("Writing " + path_.string() + " failed");
        }
    }

    void cleanup() noexcept {
        if (encoder_) {
            lame_close(encoder_);
            encoder_ = nullptr;
        }
        if (file_) {
            std::fclose(file_);
            file_ = nullptr;
        }
    }

    std::filesystem::path path_;
    lame_global_flags* encoder_{nullptr};
    std::FILE* file_{nullptr};
};

std::string timestamp(std::chrono::system_clock::time_point now) {
    const std::time_t time = std::chrono::system_clock::to_time_t(now);
    std::tm local{};
    localtime_r(&time, &local);
    std::ostringstream value;
    value << std::put_time(&local, "%Y%m%d_%H%M%S");
    const auto milliseconds =
        std::chrono::duration_cast<std::chrono::milliseconds>(
            now.time_since_epoch()) %
        1000;
    value << '_' << std::setw(3) << std::setfill('0') << milliseconds.count();
    return value.str();
}

}  // namespace

Mp3SinkFactory::Mp3SinkFactory(std::filesystem::path output_directory,
                               std::uint32_t frequency_hz, std::uint32_t sample_rate,
                               int bitrate_kbps)
    : output_directory_(std::move(output_directory)),
      frequency_hz_(frequency_hz),
      sample_rate_(sample_rate),
      bitrate_kbps_(bitrate_kbps) {
    std::filesystem::create_directories(output_directory_);
}

std::unique_ptr<AudioSink> Mp3SinkFactory::create() {
    std::filesystem::path path;
    auto start_time = std::chrono::system_clock::now();
    do {
        std::ostringstream name;
        name << timestamp(start_time) << '_' << frequency_hz_ << "Hz.mp3";
        path = output_directory_ / name.str();
        start_time += std::chrono::milliseconds(1);
    } while (std::filesystem::exists(path));
    std::fprintf(stderr, "Recording: %s\n", path.string().c_str());
    return std::make_unique<Mp3Sink>(path, sample_rate_, bitrate_kbps_);
}

}  // namespace sdr
