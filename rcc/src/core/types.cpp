#include "rcc/core/types.hpp"

namespace rcc::core {

std::string_view to_string(ErrorCode code) noexcept {
    switch (code) {
        case ErrorCode::Ok:
            return "OK";
        case ErrorCode::BadRequest:
            return "BAD_REQUEST";
        case ErrorCode::InvalidRange:
            return "INVALID_RANGE";
        case ErrorCode::Busy:
            return "BUSY";
        case ErrorCode::Unavailable:
            return "UNAVAILABLE";
        case ErrorCode::Internal:
            return "INTERNAL";
    }

    return "UNKNOWN";
}

}  // namespace rcc::core


