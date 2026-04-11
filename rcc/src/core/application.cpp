#include "rcc/core/application.hpp"

#include "rcc/api/api_gateway.hpp"
#include "rcc/audit/audit_logger.hpp"
#include "rcc/auth/authenticator.hpp"
#include "rcc/command/orchestrator.hpp"
#include "rcc/config/config_manager.hpp"
#include "rcc/radio/radio_manager.hpp"
#include "rcc/telemetry/telemetry_hub.hpp"
#include "rcc/version.hpp"

#include "dts/common/core/service_runner.hpp"
#include "dts/common/discovery/discovery.hpp"

#include <iostream>

using ::dts::common::discovery::DiscoveryEndpointMap;
using ::dts::common::discovery::DiscoveryResponder;
using ::dts::common::discovery::DiscoveryResponderConfig;
using ::dts::common::discovery::DiscoveryServiceDescriptor;

namespace rcc::core {

Application::Application() = default;

Application::~Application() {
    stop();
}

int Application::run(int argc, char* argv[]) {
    initialize(argc, argv);

    std::cout << "Radio Control Container (C++20)\n"
              << "  Version: " << rcc::version()
              << " (" << rcc::git_revision() << ")\n"
              << "  Built:   " << rcc::build_timestamp() << "\n\n";

    return runner_->run(
        [this](asio::io_context&) { start(); },
        [this]() { stop(); });
}

void Application::initialize(int argc, char* argv[]) {
    configPath_ = (argc > 1) ? std::string{argv[1]}
                              : std::string{"/etc/rcc/config.yaml"};

    runner_ = std::make_unique<::dts::common::core::ServiceRunner>(
        "radio-control", 1);
    auto& io = runner_->io();

    config_ = std::make_unique<config::ConfigManager>(configPath_);
    const auto& cfg = config_->current();

    authenticator_ = std::make_unique<auth::Authenticator>(cfg.security);
    telemetry_     = std::make_unique<telemetry::TelemetryHub>(io, cfg);
    auditLogger_   = std::make_unique<audit::AuditLogger>();
    radioManager_  = std::make_unique<radio::RadioManager>(io, cfg);

    orchestrator_ = std::make_unique<command::Orchestrator>(
        *config_, *radioManager_, *telemetry_, *auditLogger_);

    apiGateway_ = std::make_unique<api::ApiGateway>(
        io, *authenticator_, *orchestrator_,
        *radioManager_, *telemetry_, cfg.network.command_port,
        cfg.security.token_secret);

    // Service discovery responder
    DiscoveryResponderConfig disc_config;
    disc_config.enabled = cfg.service_discovery.enabled;
    disc_config.port = cfg.service_discovery.port;
    disc_config.startupBurstCount = cfg.service_discovery.startup_burst_count;
    disc_config.startupBurstSpacingMs = cfg.service_discovery.startup_burst_spacing_ms;

    DiscoveryServiceDescriptor disc_descriptor;
    disc_descriptor.service = "radio-service";
    disc_descriptor.version = std::string(rcc::version());
    disc_descriptor.ttl = cfg.service_discovery.ttl;
    disc_descriptor.bindAddress = cfg.service_discovery.bind_address;
    disc_descriptor.interfaceHint = cfg.service_discovery.interface_hint;
    const auto command_port = cfg.network.command_port;
    const auto sse_port = cfg.telemetry.sse_port > 0
                              ? cfg.telemetry.sse_port
                              : static_cast<uint16_t>(command_port + 1);
    disc_descriptor.endpointBuilder =
        [command_port, sse_port](const std::string& host) -> DiscoveryEndpointMap {
      return DiscoveryEndpointMap{
          {"rest", "http://" + host + ":" + std::to_string(command_port)},
          {"sse", "http://" + host + ":" + std::to_string(sse_port) +
                      "/api/v1/events"},
          {"health", "http://" + host + ":" + std::to_string(command_port) +
                         "/api/v1/health"},
      };
    };

    discoveryResponder_ = std::make_unique<DiscoveryResponder>(
        io, disc_config, disc_descriptor);
}

void Application::start() {
    telemetry_->start();
    radioManager_->start();
    apiGateway_->start();

    if (discoveryResponder_) {
        const auto& cfg = config_->current();
        if (cfg.service_discovery.enabled) {
            if (discoveryResponder_->start()) {
                std::cout << "Service discovery responder started on UDP port "
                          << cfg.service_discovery.port << "\n";
            } else {
                std::cerr << "Service discovery failed open; continuing without "
                             "discovery announcements\n";
            }
        }
    }

    const auto& cfg = config_->current();
    telemetry_->publishReady(cfg.container.container_id, cfg.container.deployment);

    std::cout << "Listening on port " << cfg.network.command_port
              << " (SSE on port "
              << (cfg.telemetry.sse_port > 0
                      ? cfg.telemetry.sse_port
                      : static_cast<uint16_t>(cfg.network.command_port + 1))
              << ")\n";
}

void Application::stop() {
    if (discoveryResponder_) { discoveryResponder_->stop(); }
    if (apiGateway_)   { apiGateway_->stop(); }
    if (radioManager_) { radioManager_->stop(); }
    if (telemetry_)    { telemetry_->stop(); }
}

}  // namespace rcc::core
