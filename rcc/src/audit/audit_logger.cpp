#include "rcc/audit/audit_logger.hpp"

#include <dts/common/core/logging.hpp>
#include <nlohmann/json.hpp>
#include <sstream>

namespace rcc::audit {

void AuditLogger::record(const AuditRecord& record) const {
    nlohmann::json payload = {
        {"actor",      record.actor},
        {"action",     record.action},
        {"radioId",    record.radio_id},
        {"result",     common::to_string(record.result)},
        {"message",    record.message},
        {"parameters", record.parameters}
    };

    dts::common::core::getLogger().info("[AUDIT] " + payload.dump());
}

}  // namespace rcc::audit
