#include "rcc/audit/audit_logger.hpp"

#include <sstream>

namespace rcc::audit {

void AuditLogger::record(const AuditRecord& record) const {
    nlohmann::json payload = {
        {"actor", record.actor},
        {"action", record.action},
        {"radioId", record.radio_id},
        {"result", common::to_string(record.result)},
        {"message", record.message},
        {"parameters", record.parameters}
    };

    std::ostringstream oss;
    oss << "[AUDIT] " << payload.dump();
    dts::common::core::getLogger().info(oss.str());
}

}  // namespace rcc::audit

#include "rcc/audit/audit_logger.hpp"

#include <iomanip>
#include <iostream>

namespace rcc::audit {

AuditLogger::AuditLogger() = default;
AuditLogger::~AuditLogger() = default;

void AuditLogger::record(const AuditRecord& record) {
    auto time = std::chrono::system_clock::to_time_t(record.timestamp);
    std::cout << "[Audit] " << std::put_time(std::gmtime(&time), "%FT%TZ")
              << " actor=" << record.actor << " action=" << record.action
              << " radio=" << record.radioId << " result=" << record.result
              << " details=" << record.details << std::endl;
}

}  // namespace rcc::audit


