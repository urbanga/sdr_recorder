#include "sdr/rtl_sdr_source.hpp"

#include <rtl-sdr.h>

#include <cmath>
#include <stdexcept>
#include <string>

namespace sdr {
namespace {

void require_success(int result, const char* operation) {
    if (result < 0) {
        throw std::runtime_error(std::string(operation) +
                                 " failed (librtlsdr error " +
                                 std::to_string(result) + ")");
    }
}

}  // namespace

RtlSdrSource::RtlSdrSource(const Config& config) {
    if (rtlsdr_get_device_count() == 0) {
        throw std::runtime_error("No RTL-SDR device found");
    }
    require_success(rtlsdr_open(&device_, static_cast<std::uint32_t>(config.device_index)),
                    "Opening RTL-SDR");
    try {
        require_success(rtlsdr_set_sample_rate(device_, config.iq_sample_rate),
                        "Setting sample rate");
        const std::uint64_t tuner_frequency =
            static_cast<std::uint64_t>(config.frequency_hz) + config.iq_sample_rate / 4;
        if (tuner_frequency > UINT32_MAX) {
            throw std::invalid_argument("Frequency is too high for offset tuning");
        }
        require_success(rtlsdr_set_center_freq(device_,
                                               static_cast<std::uint32_t>(tuner_frequency)),
                        "Setting frequency");
        // librtlsdr returns -2 when the requested correction is already set.
        // A newly opened device already uses 0 PPM, so avoid the no-op call.
        if (config.ppm != 0) {
            const int correction_result =
                rtlsdr_set_freq_correction(device_, config.ppm);
            if (correction_result != -2) {
                require_success(correction_result, "Setting PPM correction");
            }
        }
        if (config.gain_db < 0.0) {
            require_success(rtlsdr_set_tuner_gain_mode(device_, 0), "Enabling automatic gain");
        } else {
            require_success(rtlsdr_set_tuner_gain_mode(device_, 1), "Enabling manual gain");
            require_success(
                rtlsdr_set_tuner_gain(device_, static_cast<int>(std::lround(config.gain_db * 10))),
                "Setting tuner gain");
        }
        require_success(rtlsdr_reset_buffer(device_), "Resetting RTL-SDR buffer");
    } catch (...) {
        rtlsdr_close(device_);
        device_ = nullptr;
        throw;
    }
}

RtlSdrSource::~RtlSdrSource() {
    if (device_) rtlsdr_close(device_);
}

std::span<const std::uint8_t> RtlSdrSource::read() {
    int bytes_read = 0;
    require_success(rtlsdr_read_sync(device_, buffer_, sizeof(buffer_), &bytes_read),
                    "Reading RTL-SDR");
    if (bytes_read <= 0) throw std::runtime_error("RTL-SDR returned no data");
    return {buffer_, static_cast<std::size_t>(bytes_read)};
}

}  // namespace sdr
