#include "rcc/telemetry/telemetry_hub.hpp"

#include <dts/common/rest/rate_limiter.hpp>
#include <dts/common/security/bearer_validator.hpp>
#include <dts/common/telemetry/event_bus.hpp>
#include <dts/common/telemetry/ring_buffer.hpp>
#include <dts/common/telemetry/sse_server.hpp>

#include <asio/ip/address.hpp>
#include <asio/ip/tcp.hpp>
#include <nlohmann/json.hpp>

#include <memory>
#include <string>
#include <utility>

namespace rcc::telemetry {

class TelemetryHub::Impl {
public:
    Impl(asio::io_context& io, const config::Config& cfg)
        : io_{io},
          containerId_{cfg.container.container_id},
          deployment_{cfg.container.deployment},
          ringBuffer_{cfg.telemetry.event_buffer_size,
                      cfg.telemetry.event_retention},
          eventBus_{io_, ringBuffer_},
          bearerValidator_{std::make_unique<dts::common::security::BearerValidator>(
              cfg.security.token_secret)},
          rateLimiter_{std::make_unique<dts::common::rest::RateLimiter>(
              std::chrono::minutes{1}, cfg.telemetry.max_sse_clients)},
          sseServer_{std::make_shared<dts::common::telemetry::SSEServer>(
              io_,
              makeSseEndpoint(cfg),
              eventBus_,
              *bearerValidator_,
              cfg.telemetry.max_sse_clients,
              cfg.telemetry.client_idle_timeout,
              *rateLimiter_,
              /*enableCors=*/true,
              /*expectedPath=*/"/api/v1/telemetry")} {}

    void start() {
        sseServer_->start();
    }

    void stop() {
        sseServer_->stop();
        eventBus_.stop();
    }

    bool awaitListening(std::chrono::milliseconds timeout) {
        return sseServer_->awaitListening(timeout);
    }

    void publishEvent(const std::string& tag, nlohmann::json payload) {
        eventBus_.publish(tag, std::move(payload));
    }

private:
    static asio::ip::tcp::endpoint makeSseEndpoint(const config::Config& cfg) {
        uint16_t port = cfg.telemetry.sse_port > 0
                            ? cfg.telemetry.sse_port
                            : static_cast<uint16_t>(cfg.network.command_port + 1);
        return {asio::ip::make_address(cfg.network.bind_address), port};
    }

    asio::io_context& io_;
    std::string containerId_;
    std::string deployment_;
    dts::common::telemetry::RingBuffer ringBuffer_;
    dts::common::telemetry::EventBus eventBus_;
    std::unique_ptr<dts::common::security::BearerValidator> bearerValidator_;
    std::unique_ptr<dts::common::rest::RateLimiter> rateLimiter_;
    std::shared_ptr<dts::common::telemetry::SSEServer> sseServer_;
};

TelemetryHub::TelemetryHub(asio::io_context& io, const config::Config& cfg)
    : impl_{std::make_unique<Impl>(io, cfg)} {}

TelemetryHub::~TelemetryHub() = default;

void TelemetryHub::start() {
    impl_->start();
}

void TelemetryHub::stop() {
    impl_->stop();
}

bool TelemetryHub::awaitListening(std::chrono::milliseconds timeout) {
    return impl_->awaitListening(timeout);
}

void TelemetryHub::publishReady(const std::string& containerId,
                                const std::string& deployment) {
    impl_->publishEvent("rcc.ready", nlohmann::json{
        {"event",       "ready"},
        {"containerId", containerId},
        {"deployment",  deployment}
    });
}

void TelemetryHub::publishRadioState(const std::string& radioId,
                                     const std::string& status,
                                     int channelIndex, double powerWatts) {
    impl_->publishEvent("rcc.radio.state", nlohmann::json{
        {"radioId",      radioId},
        {"status",       status},
        {"channelIndex", channelIndex},
        {"powerWatts",   powerWatts}
    });
}

void TelemetryHub::publishChannelChanged(const std::string& radioId,
                                         int channelIndex, double frequencyMHz) {
    impl_->publishEvent("rcc.radio.channel", nlohmann::json{
        {"radioId",      radioId},
        {"channelIndex", channelIndex},
        {"frequencyMHz", frequencyMHz}
    });
}

void TelemetryHub::publishPowerChanged(const std::string& radioId, double watts) {
    impl_->publishEvent("rcc.radio.power", nlohmann::json{
        {"radioId",    radioId},
        {"powerWatts", watts}
    });
}

void TelemetryHub::publishFault(const std::string& radioId,
                                const std::string& reason) {
    impl_->publishEvent("rcc.fault", nlohmann::json{
        {"radioId", radioId},
        {"reason",  reason}
    });
}

void TelemetryHub::publishEvent(const std::string& tag, nlohmann::json payload) {
    impl_->publishEvent(tag, std::move(payload));
}

}  // namespace rcc::telemetry
