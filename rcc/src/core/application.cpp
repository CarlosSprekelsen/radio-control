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

#include <iostream>

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

    runner_ = std::make_unique<dts::common::core::ServiceRunner>(
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
}

void Application::start() {
    telemetry_->start();
    radioManager_->start();
    apiGateway_->start();

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
    if (apiGateway_)   { apiGateway_->stop(); }
    if (radioManager_) { radioManager_->stop(); }
    if (telemetry_)    { telemetry_->stop(); }
}

}  // namespace rcc::core
