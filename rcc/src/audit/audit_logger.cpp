#include "rcc/audit/audit_logger.hpp"

#include <dts/common/security/audit_logger.hpp>
#include <nlohmann/json.hpp>
#include <utility>

namespace rcc::audit {

class AuditLogger::Impl {
public:
    explicit Impl(const config::AuditConfig& config)
        : logger(config.file_path,
                 config.enabled,
                 config.rotate_after_bytes,
                 config.rotated_file_count) {}

    dts::common::security::AuditLogger logger;
};

AuditLogger::AuditLogger()
    : AuditLogger(config::AuditConfig{}) {}

AuditLogger::AuditLogger(const config::AuditConfig& config)
    : impl_(std::make_unique<Impl>(config)) {}

AuditLogger::~AuditLogger() = default;

void AuditLogger::record(const AuditRecord& record) const {
    if (!impl_) {
        return;
    }

    nlohmann::json details = {
        {"action",     record.action},
        {"radioId",    record.radio_id},
        {"message",    record.message},
        {"parameters", record.parameters}
    };

    impl_->logger.logOperation(record.actor,
                               record.action,
                               common::to_string(record.result),
                               "",
                               std::move(details));
}

}  // namespace rcc::audit
