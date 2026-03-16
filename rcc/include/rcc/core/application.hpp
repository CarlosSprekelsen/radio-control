#pragma once

#include <memory>
#include <string>

namespace asio {
class io_context;
}

namespace rcc::api {
class ApiGateway;
}

namespace rcc::auth {
class Authenticator;
}

namespace rcc::command {
class Orchestrator;
}

namespace rcc::config {
class ConfigManager;
}

namespace rcc::radio {
class RadioManager;
}

namespace rcc::telemetry {
class TelemetryHub;
}

namespace rcc::audit {
class AuditLogger;
}

namespace rcc::core {

class Application {
public:
    Application();
    ~Application();

    Application(const Application&) = delete;
    Application& operator=(const Application&) = delete;
    Application(Application&&) noexcept = delete;
    Application& operator=(Application&&) noexcept = delete;

    int run(int argc, char* argv[]);

private:
    void initialize(int argc, char* argv[]);
    void start();
    void stop();

    std::unique_ptr<asio::io_context> ioContext_;
    std::unique_ptr<config::ConfigManager> config_;
    std::unique_ptr<auth::Authenticator> authenticator_;
    std::unique_ptr<telemetry::TelemetryHub> telemetry_;
    std::unique_ptr<audit::AuditLogger> auditLogger_;
    std::unique_ptr<radio::RadioManager> radioManager_;
    std::unique_ptr<command::Orchestrator> orchestrator_;
    std::unique_ptr<api::ApiGateway> apiGateway_;

    std::string configPath_;
};

}  // namespace rcc::core


