#include "rcc/core/application.hpp"

#include "rcc/api/api_gateway.hpp"
#include "rcc/audit/audit_logger.hpp"
#include "rcc/auth/authenticator.hpp"
#include "rcc/command/orchestrator.hpp"
#include "rcc/config/config_manager.hpp"
#include "rcc/radio/radio_manager.hpp"
#include "rcc/telemetry/telemetry_hub.hpp"
#include "rcc/version.hpp"

#include <asio/io_context.hpp>
#include <asio/executor_work_guard.hpp>
#include <csignal>
#include <iostream>

namespace rcc::core {

namespace {

std::atomic<asio::io_context*> g_io_context{nullptr};

void signal_handler(int sig) {
    std::cout << "\nSignal " << sig << " received, shutting down...\n";
    if (auto* io = g_io_context.load()) {
        io->stop();
    }
}

}  // namespace

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

    std::signal(SIGINT,  signal_handler);
    std::signal(SIGTERM, signal_handler);

    start();

    // Prevent io_context::run() from returning prematurely if all async work
    // momentarily drains.  Released before stop() in the shutdown path.
    auto workGuard = asio::make_work_guard(*ioContext_);

    g_io_context.store(ioContext_.get());
    ioContext_->run();
    g_io_context.store(nullptr);

    workGuard.reset();

    stop();
    return 0;
}

void Application::initialize(int argc, char* argv[]) {
    configPath_ = (argc > 1) ? std::string{argv[1]}
                              : std::string{"/etc/rcc/config.yaml"};

    ioContext_ = std::make_unique<asio::io_context>(1);

    config_ = std::make_unique<config::ConfigManager>(configPath_);
    const auto& cfg = config_->current();

    authenticator_ = std::make_unique<auth::Authenticator>(cfg.security);
    telemetry_     = std::make_unique<telemetry::TelemetryHub>(*ioContext_, cfg);
    auditLogger_   = std::make_unique<audit::AuditLogger>();
    radioManager_  = std::make_unique<radio::RadioManager>(*ioContext_, cfg);

    orchestrator_ = std::make_unique<command::Orchestrator>(
        *config_, *radioManager_, *telemetry_, *auditLogger_);

    apiGateway_ = std::make_unique<api::ApiGateway>(
        *ioContext_, *authenticator_, *orchestrator_,
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
