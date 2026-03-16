 #pragma once
 
#include "rcc/config/config_manager.hpp"

#include <asio/io_context.hpp>
#include <memory>
#include <string>
#include <filesystem>

namespace dts::common::telemetry {
class RingBuffer;
class EventBus;
}  // namespace dts::common::telemetry

namespace rcc::telemetry {
class TelemetryHub;
}

namespace rcc {

class Application {
public:
    Application(asio::io_context& io, std::filesystem::path configPath);
    ~Application();

    Application(const Application&) = delete;
    Application& operator=(const Application&) = delete;

    void start();
    void stop();

private:
    void initializeTelemetry();

    asio::io_context& io_;
    config::ConfigManager configManager_;

    std::unique_ptr<dts::common::telemetry::RingBuffer> ringBuffer_;
    std::unique_ptr<dts::common::telemetry::EventBus> eventBus_;
    std::unique_ptr<telemetry::TelemetryHub> telemetryHub_;
};

}  // namespace rcc

