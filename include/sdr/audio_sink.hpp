#pragma once

#include <cstdint>
#include <memory>
#include <span>

namespace sdr {

class AudioSink {
public:
    virtual ~AudioSink() = default;
    virtual void write(std::span<const std::int16_t> samples) = 0;
    virtual void close() = 0;
};

class AudioSinkFactory {
public:
    virtual ~AudioSinkFactory() = default;
    virtual std::unique_ptr<AudioSink> create() = 0;
};

}  // namespace sdr
