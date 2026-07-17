#include "sdr/session_recorder.hpp"

#include <cmath>
#include <stdexcept>

namespace sdr {

SessionRecorder::SessionRecorder(std::uint32_t sample_rate, double silence_seconds,
                                 AudioSinkFactory& sink_factory)
    : silence_limit_samples_(
          static_cast<std::size_t>(std::ceil(sample_rate * silence_seconds))),
      sink_factory_(sink_factory) {
    if (sample_rate == 0 || silence_seconds <= 0.0) {
        throw std::invalid_argument("Sample rate and silence duration must be positive");
    }
}

SessionRecorder::~SessionRecorder() {
    try {
        finish();
    } catch (...) {
    }
}

void SessionRecorder::process(std::span<const std::int16_t> samples, bool signal_present) {
    if (signal_present) {
        if (!sink_) sink_ = sink_factory_.create();
        silent_samples_ = 0;
    } else if (sink_) {
        silent_samples_ += samples.size();
    }

    if (!sink_) return;
    sink_->write(samples);

    if (!signal_present && silent_samples_ >= silence_limit_samples_) {
        sink_->close();
        sink_.reset();
        silent_samples_ = 0;
    }
}

void SessionRecorder::finish() {
    if (sink_) {
        sink_->close();
        sink_.reset();
        silent_samples_ = 0;
    }
}

}  // namespace sdr
