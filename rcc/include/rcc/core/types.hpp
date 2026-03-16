#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace rcc::core {

using RadioId = std::string;

enum class ErrorCode {
    Ok,
    BadRequest,
    InvalidRange,
    Busy,
    Unavailable,
    Internal,
};

struct CommandResult {
    ErrorCode code{ErrorCode::Ok};
    std::string message{};
};

struct ChannelRequest {
    std::optional<std::uint32_t> channelIndex{};
    std::optional<double> frequencyMHz{};
    std::string correlationId{};
};

struct PowerRequest {
    std::optional<double> watts{};
    std::optional<std::string> presetName{};
    std::string correlationId{};
};

struct RadioSummary {
    RadioId id{};
    std::string model{};
    std::string state{};
    std::vector<std::string> capabilities{};
};

std::string_view to_string(ErrorCode code) noexcept;

}  // namespace rcc::core


