#pragma once

#include "sdr/config.hpp"

#include <cstdint>
#include <span>

struct rtlsdr_dev;
using rtlsdr_dev_t = struct rtlsdr_dev;

namespace sdr {

class RtlSdrSource {
public:
    explicit RtlSdrSource(const Config& config);
    ~RtlSdrSource();

    RtlSdrSource(const RtlSdrSource&) = delete;
    RtlSdrSource& operator=(const RtlSdrSource&) = delete;

    std::span<const std::uint8_t> read();

private:
    rtlsdr_dev_t* device_{nullptr};
    // 20,480 complex samples: USB-friendly and divisible by the Fs/4 mixer
    // period and the default 5:1 decimation ratio.
    std::uint8_t buffer_[40'960]{};
};

}  // namespace sdr
