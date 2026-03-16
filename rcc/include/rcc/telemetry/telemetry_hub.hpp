#pragma once

// TelemetryHub wraps dts::common telemetry infrastructure (EventBus, SSEServer,
// RingBuffer, BearerValidator, RateLimiter) behind a Pimpl to keep dts-common
// headers out of the public RCC interface.

#include "rcc/config/types.hpp"
#include <nlohmann/json.hpp>
#include <memory>
#include <string>

namespace asio {
class io_context;
}

namespace rcc::telemetry {

class TelemetryHub {
public:
    TelemetryHub(asio::io_context& io, const config::Config& cfg);
    ~TelemetryHub();

    TelemetryHub(const TelemetryHub&) = delete;
    TelemetryHub& operator=(const TelemetryHub&) = delete;
    TelemetryHub(TelemetryHub&&) noexcept = delete;
    TelemetryHub& operator=(TelemetryHub&&) noexcept = delete;

    void start();
    void stop();

    void publishReady(const std::string& containerId, const std::string& deployment);
    void publishRadioState(const std::string& radioId, const std::string& status,
                           int channelIndex, double powerWatts);
    void publishChannelChanged(const std::string& radioId, int channelIndex,
                               double frequencyMHz);
    void publishPowerChanged(const std::string& radioId, double watts);
    void publishFault(const std::string& radioId, const std::string& reason);
    void publishEvent(const std::string& tag, nlohmann::json payload);

private:
    class Impl;
    std::unique_ptr<Impl> impl_;
};

}  // namespace rcc::telemetry
