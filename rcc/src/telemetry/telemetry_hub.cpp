#include "rcc/telemetry/telemetry_hub.hpp"

#include <chrono>

namespace rcc::telemetry {

namespace tags {
constexpr auto READY = "rcc.ready";
constexpr auto RADIO_STATE = "rcc.radio.state";
constexpr auto CHANNEL_CHANGED = "rcc.radio.channel";
constexpr auto POWER_CHANGED = "rcc.radio.power";
}  // namespace tags

TelemetryHub::TelemetryHub(asio::io_context& io, const config::TelemetryConfig& config)
    : ring_buffer_(config.event_buffer_size,
                   std::chrono::hours(config.event_retention_hours)),
      event_bus_(io, ring_buffer_) {}

void TelemetryHub::publish_ready(const std::string& container_id) {
    nlohmann::json payload = {
        {"containerId", container_id},
        {"status", "ready"}
    };
    publish(tags::READY, std::move(payload));
}

void TelemetryHub::publish_radio_state(const std::string& radio_id, const common::RadioState& state) {
    nlohmann::json payload = {
        {"radioId", radio_id},
        {"status", common::to_string(state.status)}
    };
    if (state.channel_index) {
        payload["channelIndex"] = *state.channel_index;
    }
    if (state.power_watts) {
        payload["powerWatts"] = *state.power_watts;
    }
    publish(tags::RADIO_STATE, std::move(payload));
}

void TelemetryHub::publish_channel_changed(const std::string& radio_id, int channel_index, double frequency_mhz) {
    nlohmann::json payload = {
        {"radioId", radio_id},
        {"channelIndex", channel_index},
        {"frequencyMHz", frequency_mhz}
    };
    publish(tags::CHANNEL_CHANGED, std::move(payload));
}

void TelemetryHub::publish_power_changed(const std::string& radio_id, double watts) {
    nlohmann::json payload = {
        {"radioId", radio_id},
        {"powerWatts", watts}
    };
    publish(tags::POWER_CHANGED, std::move(payload));
}

void TelemetryHub::publish(const std::string& tag, nlohmann::json payload) {
    event_bus_.publish(tag, std::move(payload));
}

}  // namespace rcc::telemetry

#include "rcc/telemetry/telemetry_hub.hpp"

#include <dts/common/telemetry/event.hpp>
#include <dts/common/core/timestamp.hpp>
#include <dts/common/core/correlation_id.hpp>

namespace rcc::telemetry {

TelemetryHub::TelemetryHub(dts::common::telemetry::EventBus& bus)
    : bus_(bus) {}

void TelemetryHub::publish(std::string_view eventType, nlohmann::json payload) {
    dts::common::telemetry::Event event;
    event.tag = std::string{eventType};
    event.correlationId = dts::common::core::generateCorrelationId();
    event.timestamp = dts::common::core::utc_now_iso8601_ms();
    event.payload = std::move(payload);
    bus_.publish(std::move(event));
}

void TelemetryHub::publishReady(const std::string& containerId) {
    nlohmann::json payload{
        {"containerId", containerId},
        {"status", "ready"}
    };
    publish("rcc.ready", std::move(payload));
}

void TelemetryHub::publishState(const nlohmann::json& statePayload) {
    publish("rcc.state", statePayload);
}

void TelemetryHub::publishFault(const nlohmann::json& faultPayload) {
    publish("rcc.fault", faultPayload);
}

}  // namespace rcc::telemetry

#include "rcc/telemetry/telemetry_hub.hpp"

#include "rcc/config/config_manager.hpp"

#include <dts/common/rest/rate_limiter.hpp>
#include <dts/common/security/bearer_validator.hpp>
#include <dts/common/telemetry/event_bus.hpp>
#include <dts/common/telemetry/ring_buffer.hpp>
#include <dts/common/telemetry/sse_server.hpp>

#include <asio/io_context.hpp>
#include <asio/ip/address.hpp>
#include <asio/ip/tcp.hpp>
#include <nlohmann/json.hpp>

#include <iostream>
#include <memory>
#include <utility>

namespace rcc::telemetry {

class TelemetryHub::Impl {
public:
    Impl(asio::io_context& io, const config::ContainerConfig& config)
        : io_{io},
          config_{config},
          ringBuffer_{config.telemetry.eventBufferSize, config.telemetry.eventRetention},
          eventBus_{io_, ringBuffer_},
          bearerValidator_{std::make_unique<dts::common::security::BearerValidator>(
              config.auth.jwtSecret)},
          rateLimiter_{std::make_unique<dts::common::rest::RateLimiter>(
              std::chrono::minutes(1), config.telemetry.maxSseClients)},
          sseServer_{std::make_shared<dts::common::telemetry::SSEServer>(
              io_,
              asio::ip::tcp::endpoint(asio::ip::make_address(config.network.listenAddress),
                                      config.network.port),
              eventBus_,
              *bearerValidator_,
              config.telemetry.maxSseClients,
              config.telemetry.clientIdleTimeout,
              *rateLimiter_,
              false)} {}

    void start() {
        if (sseServer_) {
            sseServer_->start();
        }
        publishReady();
    }

    void stop() {
        if (sseServer_) {
            sseServer_->stop();
        }
        eventBus_.stop();
    }

    dts::common::telemetry::EventBus& eventBus() noexcept {
        return eventBus_;
    }

    void publishReady() {
        nlohmann::json payload{
            {"event", "ready"},
            {"containerId", config_.containerId},
            {"deployment", config_.deployment},
        };
        eventBus_.publish("rcc.ready", std::move(payload));
    }

    void publishEvent(const std::string& type, const std::string& payload) {
        nlohmann::json data;
        try {
            data = nlohmann::json::parse(payload);
        } catch (const std::exception&) {
            data = nlohmann::json{{"payload", payload}};
        }
        eventBus_.publish(type, std::move(data));
    }

private:
    asio::io_context& io_;
    const config::ContainerConfig& config_;
    dts::common::telemetry::RingBuffer ringBuffer_;
    dts::common::telemetry::EventBus eventBus_;
    std::unique_ptr<dts::common::security::BearerValidator> bearerValidator_;
    std::unique_ptr<dts::common::rest::RateLimiter> rateLimiter_;
    std::shared_ptr<dts::common::telemetry::SSEServer> sseServer_;
};

TelemetryHub::TelemetryHub(asio::io_context& io, const config::ContainerConfig& config)
    : impl_{std::make_unique<Impl>(io, config)} {}

TelemetryHub::~TelemetryHub() = default;

void TelemetryHub::start() {
    impl_->start();
}

void TelemetryHub::stop() {
    impl_->stop();
}

dts::common::telemetry::EventBus& TelemetryHub::eventBus() noexcept {
    return impl_->eventBus();
}

void TelemetryHub::publishReady() {
    impl_->publishReady();
}

void TelemetryHub::publishEvent(const std::string& type, const std::string& payload) {
    impl_->publishEvent(type, payload);
}

}  // namespace rcc::telemetry



