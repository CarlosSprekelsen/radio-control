#include "rcc/command/orchestrator.hpp"

#include "rcc/audit/audit_logger.hpp"
#include "rcc/config/config_manager.hpp"
#include "rcc/radio/radio_manager.hpp"
#include "rcc/telemetry/telemetry_hub.hpp"

#include <iostream>

namespace rcc::command {

Orchestrator::Orchestrator(config::ConfigManager& config,
                           radio::RadioManager& radioManager,
                           telemetry::TelemetryHub& telemetry,
                           audit::AuditLogger& auditLogger)
    : config_{config},
      radioManager_{radioManager},
      telemetry_{telemetry},
      auditLogger_{auditLogger} {}

Orchestrator::~Orchestrator() = default;

CommandResult Orchestrator::selectRadio(std::string_view radioId) {
    std::cout << "[Orchestrator] selectRadio(" << radioId << ") called" << std::endl;
    return {CommandResult::Code::Ok, "stub"};
}

CommandResult Orchestrator::setPower(std::string_view radioId, double watts) {
    std::cout << "[Orchestrator] setPower(" << radioId << ", " << watts << ") called"
              << std::endl;
    return {CommandResult::Code::Ok, "stub"};
}

CommandResult Orchestrator::setChannel(std::string_view radioId, int channelIndex) {
    std::cout << "[Orchestrator] setChannel(" << radioId << ", " << channelIndex
              << ") called" << std::endl;
    return {CommandResult::Code::Ok, "stub"};
}

}  // namespace rcc::command


