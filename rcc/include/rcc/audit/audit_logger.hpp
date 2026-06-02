#pragma once

#include "rcc/common/types.hpp"
#include "rcc/config/types.hpp"
#include <nlohmann/json.hpp>
#include <memory>
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
    AuditLogger();
    explicit AuditLogger(const config::AuditConfig& config);
    ~AuditLogger();

    void record(const AuditRecord& record) const;

private:
    class Impl;
    std::unique_ptr<Impl> impl_;
};

}  // namespace rcc::audit
