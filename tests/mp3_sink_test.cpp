#include "sdr/mp3_sink.hpp"

#include <gtest/gtest.h>

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <stdexcept>
#include <vector>

namespace {

class TemporaryDirectory {
public:
    TemporaryDirectory() {
        const auto unique =
            std::chrono::steady_clock::now().time_since_epoch().count();
        for (int attempt = 0; attempt < 100; ++attempt) {
            const auto candidate =
                std::filesystem::temp_directory_path() /
                ("sdr_recorder_mp3_test_" + std::to_string(unique) + "_" +
                 std::to_string(attempt));
            std::error_code error;
            if (std::filesystem::create_directory(candidate, error)) {
                path_ = candidate;
                return;
            }
            if (error && error != std::errc::file_exists) {
                throw std::filesystem::filesystem_error(
                    "Creating test directory", candidate, error);
            }
        }
        throw std::runtime_error("Could not create a unique MP3 test directory");
    }

    ~TemporaryDirectory() {
        std::error_code ignored;
        std::filesystem::remove_all(path_, ignored);
    }

    TemporaryDirectory(const TemporaryDirectory&) = delete;
    TemporaryDirectory& operator=(const TemporaryDirectory&) = delete;

    [[nodiscard]] const std::filesystem::path& path() const noexcept {
        return path_;
    }

private:
    std::filesystem::path path_;
};

std::size_t mpeg1_layer3_frame_size(const std::vector<unsigned char>& data,
                                    std::size_t offset) {
    if (offset + 4 > data.size()) return 0;
    const auto byte1 = data[offset + 1];
    const auto byte2 = data[offset + 2];
    const bool sync = data[offset] == 0xFF && (byte1 & 0xE0) == 0xE0;
    const bool mpeg1 = (byte1 & 0x18) == 0x18;
    const bool layer_three = (byte1 & 0x06) == 0x02;
    if (!sync || !mpeg1 || !layer_three) return 0;

    constexpr int bitrates_kbps[] = {0,   32,  40,  48,  56,  64,
                                     80,  96,  112, 128, 160, 192,
                                     224, 256, 320, 0};
    constexpr int sample_rates[] = {44'100, 48'000, 32'000, 0};
    const int bitrate = bitrates_kbps[(byte2 >> 4) & 0x0F];
    const int sample_rate = sample_rates[(byte2 >> 2) & 0x03];
    const int padding = (byte2 >> 1) & 0x01;
    if (bitrate == 0 || sample_rate == 0) return 0;
    return static_cast<std::size_t>(144 * bitrate * 1000 / sample_rate + padding);
}

bool starts_with_consecutive_mp3_frames(const std::vector<unsigned char>& data) {
    std::size_t offset = 0;
    if (data.size() >= 10 && data[0] == 'I' && data[1] == 'D' && data[2] == '3') {
        offset = 10 + (static_cast<std::size_t>(data[6] & 0x7F) << 21) +
                 (static_cast<std::size_t>(data[7] & 0x7F) << 14) +
                 (static_cast<std::size_t>(data[8] & 0x7F) << 7) +
                 static_cast<std::size_t>(data[9] & 0x7F);
    }
    for (int frame = 0; frame < 3; ++frame) {
        const auto frame_size = mpeg1_layer3_frame_size(data, offset);
        if (frame_size == 0 || offset + frame_size > data.size()) return false;
        offset += frame_size;
    }
    return true;
}

TEST(Mp3Sink, EncodesAndFlushesConsecutiveMp3Frames) {
    const TemporaryDirectory directory;

    sdr::Mp3SinkFactory factory(directory.path(), 100'000'000, 48'000, 64);
    auto sink = factory.create();
    std::vector<std::int16_t> audio(48'000);
    for (std::size_t n = 0; n < audio.size(); ++n) {
        audio[n] = (n / 100) % 2 == 0 ? 8'000 : -8'000;
    }
    sink->write(audio);
    sink->close();
    sink.reset();

    std::vector<std::filesystem::path> files;
    for (const auto& entry : std::filesystem::directory_iterator(directory.path())) {
        files.push_back(entry.path());
    }
    ASSERT_EQ(files.size(), 1U);
    EXPECT_EQ(files[0].extension(), ".mp3");
    EXPECT_EQ(files[0].filename().string().find("_000.mp3"), std::string::npos);
    EXPECT_GT(std::filesystem::file_size(files[0]), 1'000U);

    std::ifstream encoded_file(files[0], std::ios::binary);
    ASSERT_TRUE(encoded_file.is_open());
    const std::vector<unsigned char> encoded{
        std::istreambuf_iterator<char>{encoded_file},
        std::istreambuf_iterator<char>{}};
    EXPECT_TRUE(starts_with_consecutive_mp3_frames(encoded));
}

TEST(Mp3Sink, UsesUniqueMillisecondTimestampsForConsecutiveFiles) {
    const TemporaryDirectory directory;
    sdr::Mp3SinkFactory factory(directory.path(), 96'600'000, 48'000, 128);

    auto first = factory.create();
    first->close();
    auto second = factory.create();
    second->close();

    std::vector<std::string> names;
    for (const auto& entry : std::filesystem::directory_iterator(directory.path())) {
        names.push_back(entry.path().filename().string());
    }
    std::sort(names.begin(), names.end());
    ASSERT_EQ(names.size(), 2U);
    EXPECT_NE(names[0], names[1]);
    EXPECT_EQ(names[0].find("Hz_000.mp3"), std::string::npos);
    EXPECT_EQ(names[1].find("Hz_001.mp3"), std::string::npos);
    EXPECT_TRUE(names[0].ends_with("_96600000Hz.mp3"));
    EXPECT_TRUE(names[1].ends_with("_96600000Hz.mp3"));
}

}  // namespace
