#pragma once

#include "sdr/audio_sink.hpp"

#include <cstddef>
#include <cstdint>
#include <memory>
#include <span>

namespace sdr {

class SessionRecorder {
public:
    SessionRecorder(std::uint32_t sample_rate, double silence_seconds,
                    AudioSinkFactory& sink_factory);
    ~SessionRecorder();

    SessionRecorder(const SessionRecorder&) = delete;
    SessionRecorder& operator=(const SessionRecorder&) = delete;

    void process(std::span<const std::int16_t> samples, bool signal_present);
    void finish();
    [[nodiscard]] bool recording() const noexcept { return sink_ != nullptr; }

private:
    std::size_t silence_limit_samples_;
    std::size_t silent_samples_{0};
    AudioSinkFactory& sink_factory_;
    std::unique_ptr<AudioSink> sink_;
};

}  // namespace sdr
