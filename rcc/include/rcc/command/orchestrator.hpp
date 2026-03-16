#pragma once

#include <optional>
#include <string>
#include <string_view>

namespace rcc::config {
class ConfigManager;
class ChannelMapper;
}  // namespace rcc::config

namespace rcc::radio {
class RadioManager;
}  // namespace rcc::radio

namespace rcc::telemetry {
class TelemetryHub;
}  // namespace rcc::telemetry

namespace rcc::audit {
class AuditLogger;
}  // namespace rcc::audit

namespace rcc::command {

struct CommandResult {
    enum class Code {
        Ok,
        InvalidRange,
        Busy,
        Unavailable,
        InternalError,
        Unauthorized
    };

    Code code{Code::InternalError};
    std::string message{};
};

class Orchestrator {
public:
    Orchestrator(config::ConfigManager& config,
                 radio::RadioManager& radioManager,
                 telemetry::TelemetryHub& telemetry,
                 audit::AuditLogger& auditLogger);
    ~Orchestrator();

    CommandResult selectRadio(std::string_view radioId);
    CommandResult setPower(std::string_view radioId, double watts);
    CommandResult setChannel(std::string_view radioId, int channelIndex);

private:
    config::ConfigManager& config_;
    radio::RadioManager& radioManager_;
    telemetry::TelemetryHub& telemetry_;
    audit::AuditLogger& auditLogger_;
};

}  // namespace rcc::command


