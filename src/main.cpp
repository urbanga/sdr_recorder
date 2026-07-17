#include "sdr/config.hpp"
#include "sdr/demodulator.hpp"
#include "sdr/mp3_sink.hpp"
#include "sdr/rtl_sdr_source.hpp"
#include "sdr/session_recorder.hpp"

#include <atomic>
#include <chrono>
#include <csignal>
#include <cstdio>
#include <exception>

namespace {

std::atomic_bool running{true};

extern "C" void stop(int) {
    running.store(false);
}

}  // namespace

int main(int argc, char** argv) {
    try {
        const auto config = sdr::parse_arguments(argc, argv);
        std::signal(SIGINT, stop);
        std::signal(SIGTERM, stop);

        sdr::RtlSdrSource source(config);
        sdr::Demodulator demodulator(config.modulation, config.iq_sample_rate,
                                     config.audio_sample_rate,
                                     config.notch_frequency_hz,
                                     config.notch_width_hz);
        sdr::Mp3SinkFactory sink_factory(
            config.output_directory, config.frequency_hz, config.audio_sample_rate,
            config.mp3_bitrate_kbps);
        sdr::SessionRecorder recorder(config.audio_sample_rate, config.silence_seconds,
                                      sink_factory);

        std::fprintf(stderr,
                     "Listening on %u Hz; squelch %.1f dBFS; stop with Ctrl+C\n",
                     config.frequency_hz, config.squelch_dbfs);
        auto next_level_report = std::chrono::steady_clock::now();
        while (running.load()) {
            auto block = demodulator.process(source.read());
            if (config.level_meter &&
                std::chrono::steady_clock::now() >= next_level_report) {
                std::fprintf(stderr, "Signal: %6.1f dBFS %s\n", block.signal_dbfs,
                             block.signal_dbfs >= config.squelch_dbfs ? "OPEN" : "closed");
                next_level_report =
                    std::chrono::steady_clock::now() + std::chrono::seconds(1);
            }
            recorder.process(block.audio, block.signal_dbfs >= config.squelch_dbfs);
        }
        recorder.finish();
        std::fprintf(stderr, "Stopped cleanly\n");
        return 0;
    } catch (const sdr::UsageRequested&) {
        std::fputs(sdr::usage(argv[0]).c_str(), stdout);
        return 0;
    } catch (const std::exception& error) {
        std::fprintf(stderr, "Error: %s\n\n%s", error.what(), sdr::usage(argv[0]).c_str());
        return 1;
    }
}
