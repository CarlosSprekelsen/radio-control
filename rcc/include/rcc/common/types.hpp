#pragma once

#include <cstdint>
#include <optional>
#include <string>

namespace rcc::common {

enum class CommandResultCode {
    Ok,
    InvalidRange,
    Busy,
    Unavailable,
    InternalError
};

inline std::string to_string(CommandResultCode code) {
    switch (code) {
        case CommandResultCode::Ok: return "ok";
        case CommandResultCode::InvalidRange: return "invalid_range";
        case CommandResultCode::Busy: return "busy";
        case CommandResultCode::Unavailable: return "unavailable";
        case CommandResultCode::InternalError: return "internal";
    }
    return "internal";
}

struct CommandResult {
    CommandResultCode code{CommandResultCode::Ok};
    std::string message;
    std::optional<std::string> vendor_payload;
};

enum class RadioStatus {
    Offline,
    Discovering,
    Ready,
    Busy,
    Recovering
};

inline std::string to_string(RadioStatus status) {
    switch (status) {
        case RadioStatus::Offline: return "offline";
        case RadioStatus::Discovering: return "discovering";
        case RadioStatus::Ready: return "ready";
        case RadioStatus::Busy: return "busy";
        case RadioStatus::Recovering: return "recovering";
    }
    return "offline";
}

struct RadioState {
    RadioStatus status{RadioStatus::Offline};
    std::optional<int> channel_index;
    std::optional<double> power_watts;
};

}  // namespace rcc::common


