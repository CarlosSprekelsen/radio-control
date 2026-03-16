#include "rcc/api/api_gateway.hpp"

#include "rcc/auth/authenticator.hpp"
#include "rcc/command/orchestrator.hpp"
#include "rcc/telemetry/telemetry_hub.hpp"

#include <asio/io_context.hpp>
#include <iostream>

namespace rcc::api {

class ApiGateway::Impl {
public:
    Impl(asio::io_context& io,
         auth::Authenticator& authenticator,
         command::Orchestrator& orchestrator,
         telemetry::TelemetryHub& telemetry)
        : ioContext_{io},
          authenticator_{authenticator},
          orchestrator_{orchestrator},
          telemetry_{telemetry} {}

    void start() {
        std::cout << "[ApiGateway] start() – HTTP server bootstrap pending" << std::endl;
    }

    void stop() {
        std::cout << "[ApiGateway] stop()" << std::endl;
    }

private:
    asio::io_context& ioContext_;
    auth::Authenticator& authenticator_;
    command::Orchestrator& orchestrator_;
    telemetry::TelemetryHub& telemetry_;
};

ApiGateway::ApiGateway(asio::io_context& io,
                       auth::Authenticator& authenticator,
                       command::Orchestrator& orchestrator,
                       telemetry::TelemetryHub& telemetry)
    : impl_{std::make_unique<Impl>(io, authenticator, orchestrator, telemetry)} {}

ApiGateway::~ApiGateway() = default;

void ApiGateway::start() {
    impl_->start();
}

void ApiGateway::stop() {
    impl_->stop();
}

}  // namespace rcc::api


