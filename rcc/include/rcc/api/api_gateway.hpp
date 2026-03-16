#pragma once

#include <memory>

namespace asio {
class io_context;
}

namespace rcc::auth {
class Authenticator;
}

namespace rcc::command {
class Orchestrator;
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
               telemetry::TelemetryHub& telemetry);
    ~ApiGateway();

    ApiGateway(const ApiGateway&) = delete;
    ApiGateway& operator=(const ApiGateway&) = delete;
    ApiGateway(ApiGateway&&) noexcept = delete;
    ApiGateway& operator=(ApiGateway&&) noexcept = delete;

    void start();
    void stop();

private:
    class Impl;
    std::unique_ptr<Impl> impl_;
};

}  // namespace rcc::api


