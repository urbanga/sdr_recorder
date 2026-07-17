#include "sdr/config.hpp"

#include <gtest/gtest.h>

#include <string>
#include <vector>

namespace {

sdr::Config parse(std::vector<std::string> values) {
    std::vector<char*> argv;
    for (auto& value : values) argv.push_back(value.data());
    return sdr::parse_arguments(static_cast<int>(argv.size()), argv.data());
}

TEST(Config, ParsesFrequencySuffixAndSessionOptions) {
    const auto config = parse({"app", "-f", "446.00625M", "--silence", "10",
                               "--squelch-dbfs", "-42.5", "--modulation", "am"});
    EXPECT_EQ(config.frequency_hz, 446'006'250U);
    EXPECT_DOUBLE_EQ(config.silence_seconds, 10.0);
    EXPECT_DOUBLE_EQ(config.squelch_dbfs, -42.5);
    EXPECT_EQ(config.modulation, sdr::Modulation::am);
}

TEST(Config, RequiresFrequency) {
    EXPECT_THROW(parse({"app", "--silence", "5"}), std::invalid_argument);
}

TEST(Config, RejectsUnknownOption) {
    EXPECT_THROW(parse({"app", "-f", "100M", "--magic"}), std::invalid_argument);
}

}  // namespace
