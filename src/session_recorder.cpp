#include "sdr/session_recorder.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <stdexcept>

namespace sdr {
namespace {

void write_silence(AudioSink& sink, std::size_t sample_count) {
    constexpr std::array<std::int16_t, 4096> silence{};
    while (sample_count != 0) {
        const auto chunk_size = std::min(sample_count, silence.size());
        sink.write(std::span<const std::int16_t>{silence.data(), chunk_size});
        sample_count -= chunk_size;
    }
}

}  // namespace

SessionRecorder::SessionRecorder(std::uint32_t sample_rate, double silence_seconds,
                                 AudioSinkFactory& sink_factory,
                                 double max_session_seconds)
    : silence_limit_samples_(
          static_cast<std::size_t>(std::ceil(sample_rate * silence_seconds))),
      max_session_samples_(
          max_session_seconds > 0.0
              ? static_cast<std::size_t>(std::ceil(sample_rate * max_session_seconds))
              : 0),
      sink_factory_(sink_factory) {
    if (sample_rate == 0 || silence_seconds <= 0.0 || max_session_seconds < 0.0) {
        throw std::invalid_argument(
            "Sample rate and silence duration must be positive; max session must not be negative");
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
        if (!sink_) {
            sink_ = sink_factory_.create();
        } else if (silent_samples_ != 0) {
            // The signal resumed before the session boundary, so preserve the
            // natural inter-transmission pause without preserving receiver noise.
            write_silence(*sink_, silent_samples_);
            session_samples_ += silent_samples_;
        }
        silent_samples_ = 0;
        write_active(samples);
    } else if (sink_) {
        silent_samples_ += samples.size();
        const bool silence_boundary = silent_samples_ >= silence_limit_samples_;
        const bool maximum_boundary =
            max_session_samples_ != 0 &&
            session_samples_ + silent_samples_ >= max_session_samples_;
        if (silence_boundary || maximum_boundary) {
            // Silence is only the session boundary. Pending trailing samples
            // are deliberately discarded instead of being stored in the MP3.
            close_session();
        }
    }
}

void SessionRecorder::finish() {
    close_session();
}

void SessionRecorder::close_session() {
    if (sink_) {
        sink_->close();
        sink_.reset();
    }
    silent_samples_ = 0;
    session_samples_ = 0;
}

void SessionRecorder::write_active(std::span<const std::int16_t> samples) {
    std::size_t offset = 0;
    while (offset < samples.size()) {
        if (!sink_) sink_ = sink_factory_.create();

        std::size_t chunk_size = samples.size() - offset;
        if (max_session_samples_ != 0) {
            chunk_size = std::min(chunk_size,
                                  max_session_samples_ - session_samples_);
        }
        sink_->write(samples.subspan(offset, chunk_size));
        offset += chunk_size;
        session_samples_ += chunk_size;

        if (max_session_samples_ != 0 &&
            session_samples_ >= max_session_samples_) {
            close_session();
        }
    }
}

}  // namespace sdr
