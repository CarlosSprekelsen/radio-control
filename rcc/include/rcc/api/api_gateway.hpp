#pragma once

#include <chrono>
#include <cstdint>
#include <memory>
#include <string>

namespace asio {
class io_context;
}

namespace rcc::auth {
class Authenticator;
}

namespace rcc::command {
class Orchestrator;
}

namespace rcc::radio {
class RadioManager;
}

namespace rcc::telemetry {
class TelemetryHub;
}

namespace rcc::api {

class ApiGateway {
public:
    ApiGateway(asio::io_context& io,
               auth::Authenticator& authenticator,
               command::Orchestrator& orchestrator,
               radio::RadioManager& radioManager,
               telemetry::TelemetryHub& telemetry,
               uint16_t restPort,
               std::string tokenSecret,
               bool allowUnauthenticatedDevAccess);
    ~ApiGateway();

    ApiGateway(const ApiGateway&) = delete;
    ApiGateway& operator=(const ApiGateway&) = delete;
    ApiGateway(ApiGateway&&) noexcept = delete;
    ApiGateway& operator=(ApiGateway&&) noexcept = delete;

    void start();
    void stop();
    bool awaitListening(std::chrono::milliseconds timeout);

private:
    class Impl;
    std::unique_ptr<Impl> impl_;
};

}  // namespace rcc::api
