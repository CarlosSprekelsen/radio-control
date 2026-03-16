#pragma once

#include "rcc/common/types.hpp"
#include <nlohmann/json.hpp>
#include <string>

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
