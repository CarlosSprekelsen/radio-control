#pragma once

#include "rcc/common/types.hpp"
#include <dts/common/core/logging.hpp>
#include <string>
#include <string_view>
#include <nlohmann/json.hpp>

namespace rcc::audit {

struct AuditRecord {
    std::string actor;
    std::string action;
    std::string radio_id;
    nlohmann::json parameters;
    common::CommandResultCode result{common::CommandResultCode::Ok};
    std::string message;
};

class AuditLogger {
public:
    void record(const AuditRecord& record) const;
};

}  // namespace rcc::audit

#pragma once

#include <chrono>
#include <string>

namespace rcc::audit {

struct AuditRecord {
    std::chrono::system_clock::time_point timestamp;
    std::string actor;
    std::string action;
    std::string radioId;
    std::string result;
    std::string details;
};

class AuditLogger {
public:
    AuditLogger();
    ~AuditLogger();

    void record(const AuditRecord& record);
};

}  // namespace rcc::audit


