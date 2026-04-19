#include "rcc/telemetry/telemetry_hub.hpp"

#include <dts/common/core/timestamp.hpp>
#include <dts/common/rest/rate_limiter.hpp>
#include <dts/common/security/bearer_validator.hpp>
#include <dts/common/telemetry/event_bus.hpp>
#include <dts/common/telemetry/ring_buffer.hpp>
#include <dts/common/telemetry/sse_server.hpp>

#include <asio/ip/address.hpp>
#include <asio/ip/tcp.hpp>
#include <asio/io_context.hpp>
#include <asio/post.hpp>
#include <asio/strand.hpp>
#include <asio/steady_timer.hpp>
#include <nlohmann/json.hpp>

#include <atomic>
#include <cmath>
#include <cstdint>
#include <future>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <utility>

namespace rcc::telemetry {

namespace {

std::string effective_sse_secret(const config::Config& cfg) {
    if (!cfg.security.token_secret.empty() ||
        cfg.security.allow_unauthenticated_dev_access) {
        return cfg.security.token_secret;
    }

    const auto seed = std::chrono::steady_clock::now().time_since_epoch().count();
    return "__rcc-disabled-auth-" + std::to_string(seed);
}

std::string normalize_status(std::string_view status) {
    if (status == "ready" || status == "discovering" || status == "busy") {
        return "online";
    }
    if (status == "recovering") {
        return "recovering";
    }
    return "offline";
}

nlohmann::json power_dbm_json(double watts) {
    if (watts <= 0.0) {
        return nullptr;
    }
    return std::round(10.0 * std::log10(watts * 1000.0));
}

}  // namespace

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
              effective_sse_secret(cfg))},
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
              /*expectedPath=*/"/api/v1/telemetry")},
          heartbeatStrand_{asio::make_strand(io)},
          heartbeatInterval_{cfg.telemetry.heartbeat_interval},
          heartbeatTimer_{heartbeatStrand_} {}

    void start() {
        sseServer_->start();
        heartbeatStopped_.store(false, std::memory_order_relaxed);
        asio::post(heartbeatStrand_, [this]() { scheduleHeartbeat(); });
    }

    void stop() {
        heartbeatStopped_.store(true, std::memory_order_relaxed);
        cancelHeartbeatTimer();
        sseServer_->stop();
        eventBus_.stop();
    }

    bool awaitListening(std::chrono::milliseconds timeout) {
        return sseServer_->awaitListening(timeout);
    }

    void publishEvent(const std::string& tag, nlohmann::json payload) {
        eventBus_.publish(tag, std::move(payload));
    }

    void rememberReadySnapshot(nlohmann::json snapshot) {
        std::lock_guard<std::mutex> lock(readyMutex_);
        lastReadyPayload_ = std::move(snapshot);
    }

private:
    static asio::ip::tcp::endpoint makeSseEndpoint(const config::Config& cfg) {
        uint16_t port = cfg.telemetry.sse_port > 0
                            ? cfg.telemetry.sse_port
                            : static_cast<uint16_t>(cfg.network.command_port + 1);
        return {asio::ip::make_address(cfg.network.bind_address), port};
    }

    void cancelHeartbeatTimer() {
        auto cancel = [this]() {
            asio::error_code ec;
            heartbeatTimer_.cancel(ec);
        };

        if (heartbeatStrand_.running_in_this_thread() || io_.stopped()) {
            cancel();
            return;
        }

        std::promise<void> cancelled;
        auto future = cancelled.get_future();
        asio::post(heartbeatStrand_, [cancel = std::move(cancel),
                                      done = std::move(cancelled)]() mutable {
            cancel();
            done.set_value();
        });
        future.wait();
    }

    void scheduleHeartbeat() {
        if (heartbeatStopped_.load(std::memory_order_relaxed)) return;
        heartbeatTimer_.expires_after(heartbeatInterval_);
        heartbeatTimer_.async_wait([this](const asio::error_code& ec) {
            if (ec == asio::error::operation_aborted) return;
            if (heartbeatStopped_.load(std::memory_order_relaxed)) return;
            eventBus_.publish("heartbeat", nlohmann::json{
                {"ts", dts::common::core::utc_now_iso8601_ms()}
            });
            // Every kReadyReemitPeriod heartbeats, re-emit the last `ready`
            // snapshot so late-joining SSE clients always find one in the
            // ring buffer. Matches the lrf-control "publish state on every
            // transition" pattern without coupling to radio_manager.
            if (++heartbeatTicks_ % kReadyReemitPeriod == 0) {
                std::optional<nlohmann::json> snap;
                {
                    std::lock_guard<std::mutex> lock(readyMutex_);
                    snap = lastReadyPayload_;
                }
                if (snap) {
                    eventBus_.publish("ready", *snap);
                }
            }
            scheduleHeartbeat();
        });
    }

    static constexpr std::uint64_t kReadyReemitPeriod = 12;  // ~60s at 5s heartbeat

    asio::io_context& io_;
    std::string containerId_;
    std::string deployment_;
    dts::common::telemetry::RingBuffer ringBuffer_;
    dts::common::telemetry::EventBus eventBus_;
    std::unique_ptr<dts::common::security::BearerValidator> bearerValidator_;
    std::unique_ptr<dts::common::rest::RateLimiter> rateLimiter_;
    std::shared_ptr<dts::common::telemetry::SSEServer> sseServer_;
    asio::strand<asio::io_context::executor_type> heartbeatStrand_;
    std::chrono::seconds heartbeatInterval_;
    asio::steady_timer heartbeatTimer_;
    std::atomic<bool> heartbeatStopped_{false};
    std::uint64_t heartbeatTicks_{0};

    std::mutex readyMutex_;
    std::optional<nlohmann::json> lastReadyPayload_;
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

static std::string now_iso8601() {
    using namespace std::chrono;
    const auto now = system_clock::now();
    const auto time = system_clock::to_time_t(now);
    std::tm utc;
    gmtime_r(&time, &utc);
    char buf[32];
    std::strftime(buf, sizeof(buf), "%Y-%m-%dT%H:%M:%SZ", &utc);
    return buf;
}

void TelemetryHub::publishReady(nlohmann::json snapshot) {
    nlohmann::json payload = {{"snapshot", std::move(snapshot)}};
    impl_->rememberReadySnapshot(payload);
    impl_->publishEvent("ready", std::move(payload));
}

void TelemetryHub::publishRadioState(const std::string& radioId,
                                     const std::string& status,
                                     int channelIndex, double powerWatts,
                                     double frequencyMHz) {
    nlohmann::json payload = {
        {"radioId", radioId},
        {"status", normalize_status(status)},
        {"powerDbm", power_dbm_json(powerWatts)},
        {"ts", now_iso8601()}
    };
    if (channelIndex > 0) {
        payload["channelIndex"] = channelIndex;
    } else {
        payload["channelIndex"] = nullptr;
    }
    if (frequencyMHz > 0.0) {
        payload["frequencyMhz"] = frequencyMHz;
    } else {
        payload["frequencyMhz"] = nullptr;
    }
    impl_->publishEvent("state", std::move(payload));
}

void TelemetryHub::publishChannelChanged(const std::string& radioId,
                                         int channelIndex, double frequencyMHz) {
    impl_->publishEvent("channelChanged", nlohmann::json{
        {"radioId",      radioId},
        {"frequencyMhz", frequencyMHz},
        {"channelIndex", channelIndex},
        {"ts",           now_iso8601()}
    });
}

void TelemetryHub::publishPowerChanged(const std::string& radioId, double watts) {
    impl_->publishEvent("powerChanged", nlohmann::json{
        {"radioId",  radioId},
        {"powerDbm", power_dbm_json(watts)},
        {"ts",       now_iso8601()}
    });
}

void TelemetryHub::publishFault(const std::string& radioId,
                                const std::string& code,
                                const std::string& message,
                                int retryMs) {
    impl_->publishEvent("fault", nlohmann::json{
        {"radioId", radioId},
        {"code",    code},
        {"message", message},
        {"details", {{"retryMs", retryMs}}},
        {"ts",      now_iso8601()}
    });
}

void TelemetryHub::publishEvent(const std::string& tag, nlohmann::json payload) {
    impl_->publishEvent(tag, std::move(payload));
}

}  // namespace rcc::telemetry
