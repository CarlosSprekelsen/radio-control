#include "rcc/application.hpp"

#include <dts/common/telemetry/event_bus.hpp>
#include <dts/common/telemetry/ring_buffer.hpp>
#include <dts/common/core/logging.hpp>

#include "rcc/telemetry/telemetry_hub.hpp"

#include <utility>

namespace rcc {

Application::Application(asio::io_context& io, std::filesystem::path configPath)
    : io_(io),
      configManager_(std::move(configPath)) {}

Application::~Application() = default;

void Application::start() {
    initializeTelemetry();
    // TODO: wire API gateway, command orchestrator, adapters, and auth
}

void Application::stop() {
    if (eventBus_) {
        eventBus_->stop();
    }
    io_.stop();
}

void Application::initializeTelemetry() {
    const auto& cfg = configManager_.current();

    ringBuffer_ = std::make_unique<dts::common::telemetry::RingBuffer>(
        cfg.telemetry.event_buffer_size,
        cfg.telemetry.event_retention
    );

    eventBus_ = std::make_unique<dts::common::telemetry::EventBus>(io_, *ringBuffer_);

    telemetryHub_ = std::make_unique<telemetry::TelemetryHub>(*eventBus_);
    telemetryHub_->publishReady(cfg.container.container_id);
}

}  // namespace rcc


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
#include <iostream>

namespace rcc::core {

namespace {
constexpr const char* kDefaultConfigPath = "/etc/rcc/config.yaml";
}  // namespace

Application::Application() = default;

Application::~Application() {
    stop();
}

int Application::run(int argc, char* argv[]) {
    initialize(argc, argv);
    start();

    std::cout << "Radio Control Container (C++20) starting..." << std::endl;
    std::cout << "Version: " << kVersion << " (" << kGitVersion << ")" << std::endl;
    std::cout << "Build Time: " << kBuildTimestamp << std::endl;

    if (!ioContext_) {
        std::cerr << "I/O context not initialized" << std::endl;
        return 1;
    }

    ioContext_->run();
    stop();

    return 0;
}

void Application::initialize(int argc, char* argv[]) {
    configPath_ = (argc > 1) ? std::string{argv[1]} : std::string{kDefaultConfigPath};

    ioContext_ = std::make_unique<asio::io_context>(1);

    // Placeholder initialization; full dependency wiring occurs in later tasks.
    config_ = std::make_unique<config::ConfigManager>(configPath_);
    authenticator_ = std::make_unique<auth::Authenticator>();
    telemetry_ = std::make_unique<telemetry::TelemetryHub>(*ioContext_);
    auditLogger_ = std::make_unique<audit::AuditLogger>();
    radioManager_ = std::make_unique<radio::RadioManager>(*ioContext_, *config_);
    orchestrator_ = std::make_unique<command::Orchestrator>(
        *config_, *radioManager_, *telemetry_, *auditLogger_);
    apiGateway_ = std::make_unique<api::ApiGateway>(
        *ioContext_, *authenticator_, *orchestrator_, *telemetry_);
}

void Application::start() {
    if (telemetry_) {
        telemetry_->start();
    }
    if (radioManager_) {
        radioManager_->start();
    }
    if (apiGateway_) {
        apiGateway_->start();
    }
}

void Application::stop() {
    if (apiGateway_) {
        apiGateway_->stop();
    }
    if (radioManager_) {
        radioManager_->stop();
    }
    if (telemetry_) {
        telemetry_->stop();
    }
}

}  // namespace rcc::core


