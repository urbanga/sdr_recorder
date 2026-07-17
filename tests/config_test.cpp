#include "sdr/config.hpp"

#include <gtest/gtest.h>

#include <locale>
#include <string>
#include <vector>

namespace {

sdr::Config parse(std::vector<std::string> values) {
    std::vector<char*> argv;
    for (auto& value : values) argv.push_back(value.data());
    return sdr::parse_arguments(static_cast<int>(argv.size()), argv.data());
}

class CommaDecimalPoint final : public std::numpunct<char> {
protected:
    char do_decimal_point() const override { return ','; }
};

class ScopedGlobalLocale {
public:
    ScopedGlobalLocale()
        : previous_(std::locale()),
          comma_locale_(std::locale::classic(), new CommaDecimalPoint) {
        std::locale::global(comma_locale_);
    }
    ~ScopedGlobalLocale() { std::locale::global(previous_); }

    ScopedGlobalLocale(const ScopedGlobalLocale&) = delete;
    ScopedGlobalLocale& operator=(const ScopedGlobalLocale&) = delete;

private:
    std::locale previous_;
    std::locale comma_locale_;
};

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

TEST(Config, ParsesOptionalAudioNotch) {
    const auto config = parse({"app", "-f", "446M", "--notch-frequency", "1890",
                               "--notch-width", "120"});
    EXPECT_DOUBLE_EQ(config.notch_frequency_hz, 1890.0);
    EXPECT_DOUBLE_EQ(config.notch_width_hz, 120.0);
}

TEST(Config, WideFmSelectsBroadcastIqRateAndParsesMaximumSession) {
    const auto config = parse({"app", "-f", "96.6M", "--modulation", "wfm",
                               "--max-session", "30"});
    EXPECT_EQ(config.modulation, sdr::Modulation::wfm);
    EXPECT_EQ(config.iq_sample_rate, 1'200'000U);
    EXPECT_DOUBLE_EQ(config.max_session_seconds, 30.0);
}

TEST(Config, RejectsNegativeMaximumSession) {
    EXPECT_THROW(parse({"app", "-f", "96.6M", "--max-session", "-1"}),
                 std::invalid_argument);
}

TEST(Config, ParsingIsIndependentOfProcessLocale) {
    const ScopedGlobalLocale comma_locale;
    const auto config = parse({"app", "-f", "446.00625M", "--silence", "10.5"});
    EXPECT_EQ(config.frequency_hz, 446'006'250U);
    EXPECT_DOUBLE_EQ(config.silence_seconds, 10.5);
}

}  // namespace
