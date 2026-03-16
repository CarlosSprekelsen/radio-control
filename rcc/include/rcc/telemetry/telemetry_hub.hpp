#pragma once

#include "rcc/common/types.hpp"
#include "rcc/config/config_manager.hpp"
#include <asio/io_context.hpp>
#include <dts/common/telemetry/event_bus.hpp>
#include <dts/common/telemetry/ring_buffer.hpp>
#include <nlohmann/json.hpp>
#include <memory>
#include <string>

namespace rcc::telemetry {

class TelemetryHub {
public:
    TelemetryHub(asio::io_context& io, const config::TelemetryConfig& config);

    dts::common::telemetry::EventBus& event_bus() noexcept { return event_bus_; }

    void publish_ready(const std::string& container_id);
    void publish_radio_state(const std::string& radio_id, const common::RadioState& state);
    void publish_channel_changed(const std::string& radio_id, int channel_index, double frequency_mhz);
    void publish_power_changed(const std::string& radio_id, double watts);

private:
    dts::common::telemetry::RingBuffer ring_buffer_;
    dts::common::telemetry::EventBus event_bus_;

    void publish(const std::string& tag, nlohmann::json payload);
};

}  // namespace rcc::telemetry

#pragma once

#include <dts/common/telemetry/event_bus.hpp>
#include <nlohmann/json.hpp>

#include <string>
#include <string_view>
#include <memory>

namespace rcc::telemetry {

class TelemetryHub {
public:
    explicit TelemetryHub(dts::common::telemetry::EventBus& bus);

    void publish(std::string_view eventType, nlohmann::json payload);
    void publishReady(const std::string& containerId);
    void publishState(const nlohmann::json& statePayload);
    void publishFault(const nlohmann::json& faultPayload);

private:
    dts::common::telemetry::EventBus& bus_;
};

}  // namespace rcc::telemetry

#pragma once

#include <memory>
#include <string>

namespace asio {
class io_context;
}  // namespace asio

namespace dts::common::telemetry {
class EventBus;
}  // namespace dts::common::telemetry

namespace rcc::config {
struct ContainerConfig;
}  // namespace rcc::config

namespace rcc::telemetry {

class TelemetryHub {
public:
    TelemetryHub(asio::io_context& io, const config::ContainerConfig& config);
    ~TelemetryHub();

    TelemetryHub(const TelemetryHub&) = delete;
    TelemetryHub& operator=(const TelemetryHub&) = delete;
    TelemetryHub(TelemetryHub&&) noexcept = delete;
    TelemetryHub& operator=(TelemetryHub&&) noexcept = delete;

    void start();
    void stop();

    dts::common::telemetry::EventBus& eventBus() noexcept;

    void publishReady();
    void publishEvent(const std::string& type, const std::string& payload);

private:
    class Impl;
    std::unique_ptr<Impl> impl_;
};

}  // namespace rcc::telemetry



