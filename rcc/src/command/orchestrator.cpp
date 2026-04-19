#include "rcc/command/orchestrator.hpp"

#include "rcc/audit/audit_logger.hpp"
#include "rcc/config/config_manager.hpp"
#include "rcc/radio/radio_manager.hpp"
#include "rcc/telemetry/telemetry_hub.hpp"

#include <iostream>

namespace rcc::command {

namespace {

CommandResult::Code mapAdapterCode(common::CommandResultCode code) {
    switch (code) {
        case common::CommandResultCode::Ok:            return CommandResult::Code::Ok;
        case common::CommandResultCode::InvalidRange:  return CommandResult::Code::InvalidRange;
        case common::CommandResultCode::Busy:          return CommandResult::Code::Busy;
        case common::CommandResultCode::Unavailable:   return CommandResult::Code::Unavailable;
        case common::CommandResultCode::InternalError: return CommandResult::Code::InternalError;
    }
    return CommandResult::Code::InternalError;
}

common::CommandResultCode resultToAuditCode(CommandResult::Code code) {
    switch (code) {
        case CommandResult::Code::Ok:            return common::CommandResultCode::Ok;
        case CommandResult::Code::InvalidRange:  return common::CommandResultCode::InvalidRange;
        case CommandResult::Code::Busy:          return common::CommandResultCode::Busy;
        case CommandResult::Code::Unavailable:   return common::CommandResultCode::Unavailable;
        case CommandResult::Code::Unauthorized:  return common::CommandResultCode::InternalError;
        case CommandResult::Code::NotFound:      return common::CommandResultCode::InternalError;
        case CommandResult::Code::InternalError: return common::CommandResultCode::InternalError;
    }
    return common::CommandResultCode::InternalError;
}

}  // namespace

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
    CommandResult result{CommandResult::Code::Ok, "OK"};
    if (!radioManager_.set_active_radio(std::string(radioId))) {
        result = {CommandResult::Code::NotFound, "Radio not found"};
    }
    auditLogger_.record({
        .actor      = "system",
        .action     = "selectRadio",
        .radio_id   = std::string(radioId),
        .parameters = nlohmann::json::object(),
        .result     = resultToAuditCode(result.code),
        .message    = result.message
    });
    return result;
}

CommandResult Orchestrator::setPower(std::string_view radioId, double watts) {
    std::cout << "[Orchestrator] setPower(" << radioId << ", " << watts << ") called"
              << std::endl;
    const auto emitAudit = [&](const CommandResult& r) {
        auditLogger_.record({
            .actor      = "system",
            .action     = "setPower",
            .radio_id   = std::string(radioId),
            .parameters = {{"powerWatts", watts}},
            .result     = resultToAuditCode(r.code),
            .message    = r.message
        });
    };

    const auto adapter = radioManager_.get_adapter(std::string(radioId));
    if (!adapter) {
        CommandResult r{CommandResult::Code::NotFound, "Radio not found"};
        emitAudit(r);
        return r;
    }

    const auto caps = adapter->capabilities();
    if (watts < caps.power_range_watts.first || watts > caps.power_range_watts.second) {
        CommandResult r{CommandResult::Code::InvalidRange,
                        "Requested power is outside supported range"};
        emitAudit(r);
        return r;
    }

    const auto adapterResult = adapter->set_power(watts);
    if (adapterResult.code != common::CommandResultCode::Ok) {
        CommandResult r{mapAdapterCode(adapterResult.code),
                        adapterResult.message.empty() ? "Adapter rejected setPower"
                                                      : adapterResult.message};
        emitAudit(r);
        return r;
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
    CommandResult ok{CommandResult::Code::Ok, "OK"};
    emitAudit(ok);
    return ok;
}

CommandResult Orchestrator::setChannel(std::string_view radioId, int channelIndex) {
    std::cout << "[Orchestrator] setChannel(" << radioId << ", " << channelIndex
              << ") called" << std::endl;
    const auto emitAudit = [&](const CommandResult& r) {
        auditLogger_.record({
            .actor      = "system",
            .action     = "setChannel",
            .radio_id   = std::string(radioId),
            .parameters = {{"channelIndex", channelIndex}},
            .result     = resultToAuditCode(r.code),
            .message    = r.message
        });
    };

    const auto adapter = radioManager_.get_adapter(std::string(radioId));
    if (!adapter) {
        CommandResult r{CommandResult::Code::NotFound, "Radio not found"};
        emitAudit(r);
        return r;
    }

    const auto caps = adapter->capabilities();
    if (channelIndex < 1 || channelIndex > static_cast<int>(caps.supported_frequencies_mhz.size())) {
        CommandResult r{CommandResult::Code::InvalidRange,
                        "Requested channelIndex is outside supported range"};
        emitAudit(r);
        return r;
    }

    const double frequencyMhz = caps.supported_frequencies_mhz[static_cast<size_t>(channelIndex) - 1];
    const auto adapterResult = adapter->set_channel(channelIndex, frequencyMhz);
    if (adapterResult.code != common::CommandResultCode::Ok) {
        CommandResult r{mapAdapterCode(adapterResult.code),
                        adapterResult.message.empty() ? "Adapter rejected setChannel"
                                                      : adapterResult.message};
        emitAudit(r);
        return r;
    }

    const auto state = adapter->state();
    const double powerWatts = state.power_watts.value_or(0.0);
    telemetry_.publishChannelChanged(std::string(radioId), channelIndex, frequencyMhz);
    telemetry_.publishRadioState(std::string(radioId),
                                 common::to_string(state.status),
                                 channelIndex,
                                 powerWatts,
                                 frequencyMhz);
    CommandResult ok{CommandResult::Code::Ok, "OK"};
    emitAudit(ok);
    return ok;
}

}  // namespace rcc::command
