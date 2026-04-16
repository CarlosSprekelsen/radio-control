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
    if (!radioManager_.set_active_radio(std::string(radioId))) {
        return {CommandResult::Code::NotFound, "Radio not found"};
    }
    return {CommandResult::Code::Ok, "OK"};
}

CommandResult Orchestrator::setPower(std::string_view radioId, double watts) {
    std::cout << "[Orchestrator] setPower(" << radioId << ", " << watts << ") called"
              << std::endl;
    const auto adapter = radioManager_.get_adapter(std::string(radioId));
    if (!adapter) {
        return {CommandResult::Code::NotFound, "Radio not found"};
    }

    const auto caps = adapter->capabilities();
    if (watts < caps.power_range_watts.first || watts > caps.power_range_watts.second) {
        return {CommandResult::Code::InvalidRange,
                "Requested power is outside supported range"};
    }

    const auto result = adapter->set_power(watts);
    if (result.code != common::CommandResultCode::Ok) {
        return {CommandResult::Code::InternalError,
                "Adapter failed to set power"};
    }

    const auto state = adapter->state();
    const double frequencyMhz = state.channel_index.has_value()
        ? caps.supported_frequencies_mhz[static_cast<size_t>(state.channel_index.value()) - 1]
        : 0.0;
    telemetry_.publishPowerChanged(std::string(radioId), watts);
    telemetry_.publishRadioState(std::string(radioId),
                                 common::to_string(state.status),
                                 state.channel_index.value_or(-1),
                                 watts,
                                 frequencyMhz);
    return {CommandResult::Code::Ok, "OK"};
}

CommandResult Orchestrator::setChannel(std::string_view radioId, int channelIndex) {
    std::cout << "[Orchestrator] setChannel(" << radioId << ", " << channelIndex
              << ") called" << std::endl;
    const auto adapter = radioManager_.get_adapter(std::string(radioId));
    if (!adapter) {
        return {CommandResult::Code::NotFound, "Radio not found"};
    }

    const auto caps = adapter->capabilities();
    if (channelIndex < 1 || channelIndex > static_cast<int>(caps.supported_frequencies_mhz.size())) {
        return {CommandResult::Code::InvalidRange,
                "Requested channelIndex is outside supported range"};
    }

    const double frequencyMhz = caps.supported_frequencies_mhz[static_cast<size_t>(channelIndex) - 1];
    const auto result = adapter->set_channel(channelIndex, frequencyMhz);
    if (result.code != common::CommandResultCode::Ok) {
        return {CommandResult::Code::InternalError,
                "Adapter failed to set channel"};
    }

    const auto state = adapter->state();
    const double powerWatts = state.power_watts.value_or(0.0);
    telemetry_.publishChannelChanged(std::string(radioId), channelIndex, frequencyMhz);
    telemetry_.publishRadioState(std::string(radioId),
                                 common::to_string(state.status),
                                 channelIndex,
                                 powerWatts,
                                 frequencyMhz);
    return {CommandResult::Code::Ok, "OK"};
}

}  // namespace rcc::command


