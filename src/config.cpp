#include "sdr/config.hpp"

#include <charconv>
#include <cmath>
#include <limits>
#include <sstream>
#include <string_view>

namespace sdr {
namespace {

double parse_number(std::string_view text, std::string_view option) {
    std::string value{text};
    std::size_t consumed = 0;
    double result = 0.0;
    try {
        result = std::stod(value, &consumed);
    } catch (const std::exception&) {
        throw std::invalid_argument("Invalid value for " + std::string(option) + ": " + value);
    }
    if (consumed != value.size() || !std::isfinite(result)) {
        throw std::invalid_argument("Invalid value for " + std::string(option) + ": " + value);
    }
    return result;
}

std::uint32_t parse_frequency(std::string_view text) {
    double multiplier = 1.0;
    if (!text.empty()) {
        const char suffix = text.back();
        if (suffix == 'k' || suffix == 'K') {
            multiplier = 1'000.0;
            text.remove_suffix(1);
        } else if (suffix == 'm' || suffix == 'M') {
            multiplier = 1'000'000.0;
            text.remove_suffix(1);
        }
    }
    const double hz = parse_number(text, "--frequency") * multiplier;
    if (hz < 1.0 || hz > std::numeric_limits<std::uint32_t>::max()) {
        throw std::invalid_argument("Frequency is outside the supported range");
    }
    return static_cast<std::uint32_t>(std::llround(hz));
}

std::string_view next_value(int& index, int argc, char** argv) {
    if (++index >= argc) {
        throw std::invalid_argument(std::string("Missing value for ") + argv[index - 1]);
    }
    return argv[index];
}

}  // namespace

Config parse_arguments(int argc, char** argv) {
    Config config;
    bool has_frequency = false;
    for (int i = 1; i < argc; ++i) {
        const std::string_view option{argv[i]};
        if (option == "--help" || option == "-h") {
            throw UsageRequested{};
        } else if (option == "--frequency" || option == "-f") {
            config.frequency_hz = parse_frequency(next_value(i, argc, argv));
            has_frequency = true;
        } else if (option == "--modulation") {
            const auto value = next_value(i, argc, argv);
            if (value == "nfm") config.modulation = Modulation::nfm;
            else if (value == "am") config.modulation = Modulation::am;
            else throw std::invalid_argument("--modulation must be nfm or am");
        } else if (option == "--squelch-dbfs") {
            config.squelch_dbfs = parse_number(next_value(i, argc, argv), option);
        } else if (option == "--silence") {
            config.silence_seconds = parse_number(next_value(i, argc, argv), option);
        } else if (option == "--output") {
            config.output_directory = next_value(i, argc, argv);
        } else if (option == "--device") {
            config.device_index = static_cast<int>(parse_number(next_value(i, argc, argv), option));
        } else if (option == "--ppm") {
            config.ppm = static_cast<int>(parse_number(next_value(i, argc, argv), option));
        } else if (option == "--gain") {
            config.gain_db = parse_number(next_value(i, argc, argv), option);
        } else if (option == "--bitrate") {
            config.mp3_bitrate_kbps =
                static_cast<int>(parse_number(next_value(i, argc, argv), option));
        } else if (option == "--level-meter") {
            config.level_meter = true;
        } else if (option == "--notch-frequency") {
            config.notch_frequency_hz =
                parse_number(next_value(i, argc, argv), option);
        } else if (option == "--notch-width") {
            config.notch_width_hz =
                parse_number(next_value(i, argc, argv), option);
        } else {
            throw std::invalid_argument("Unknown option: " + std::string(option));
        }
    }
    if (!has_frequency) throw std::invalid_argument("--frequency is required");
    if (config.silence_seconds <= 0.0) throw std::invalid_argument("--silence must be positive");
    if (config.squelch_dbfs > 0.0) throw std::invalid_argument("--squelch-dbfs must not exceed 0");
    if (config.device_index < 0) throw std::invalid_argument("--device must not be negative");
    if (config.mp3_bitrate_kbps < 8 || config.mp3_bitrate_kbps > 320) {
        throw std::invalid_argument("--bitrate must be between 8 and 320");
    }
    if (config.notch_frequency_hz < 0.0 ||
        config.notch_frequency_hz >= config.audio_sample_rate / 2.0) {
        throw std::invalid_argument("--notch-frequency must be below the audio Nyquist rate");
    }
    if (config.notch_width_hz <= 0.0) {
        throw std::invalid_argument("--notch-width must be positive");
    }
    return config;
}

std::string usage(const char* program) {
    std::ostringstream out;
    out << "Usage: " << program << " --frequency HZ [options]\n\n"
        << "Options:\n"
        << "  -f, --frequency HZ|k|M  Frequency, e.g. 446.00625M (required)\n"
        << "      --modulation nfm|am Demodulation mode (default: nfm)\n"
        << "      --squelch-dbfs DB    Signal threshold (default: -35)\n"
        << "      --silence SECONDS    Silence required to close a session (default: 10)\n"
        << "      --output DIRECTORY   MP3 destination (default: recordings)\n"
        << "      --device INDEX       RTL-SDR device index (default: 0)\n"
        << "      --gain DB            Manual tuner gain; omit for automatic gain\n"
        << "      --ppm PPM            Frequency correction (default: 0)\n"
        << "      --bitrate KBPS       MP3 bitrate, 8-320 (default: 64)\n"
        << "      --level-meter        Print signal level once per second\n"
        << "      --notch-frequency HZ Remove a persistent audio tone (disabled by default)\n"
        << "      --notch-width HZ     Notch width (default: 120)\n"
        << "  -h, --help               Show this help\n";
    return out.str();
}

}  // namespace sdr
