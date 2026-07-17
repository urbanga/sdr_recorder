#include "sdr/mp3_sink.hpp"

#include <gtest/gtest.h>

#include <chrono>
#include <cstdint>
#include <filesystem>
#include <vector>

namespace {

TEST(Mp3Sink, EncodesAndFlushesAPlayableSizedFile) {
    const auto unique = std::chrono::steady_clock::now().time_since_epoch().count();
    const auto directory =
        std::filesystem::temp_directory_path() /
        ("sdr_recorder_mp3_test_" + std::to_string(unique));
    std::filesystem::create_directories(directory);

    sdr::Mp3SinkFactory factory(directory, 100'000'000, 48'000, 64);
    auto sink = factory.create();
    std::vector<std::int16_t> audio(48'000);
    for (std::size_t n = 0; n < audio.size(); ++n) {
        audio[n] = (n / 100) % 2 == 0 ? 8'000 : -8'000;
    }
    sink->write(audio);
    sink->close();
    sink.reset();

    std::vector<std::filesystem::path> files;
    for (const auto& entry : std::filesystem::directory_iterator(directory)) {
        files.push_back(entry.path());
    }
    ASSERT_EQ(files.size(), 1U);
    EXPECT_EQ(files[0].extension(), ".mp3");
    EXPECT_GT(std::filesystem::file_size(files[0]), 1'000U);

    std::filesystem::remove_all(directory);
}

}  // namespace
