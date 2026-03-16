#pragma once

#include "rcc/config/types.hpp"
#include <dts/common/rest/http_parser.hpp>
#include <dts/common/security/bearer_validator.hpp>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace rcc::auth {

enum class AccessLevel {
    Telemetry,
    Control
};

struct AuthResult {
    bool allowed{false};
    std::string subject;
    std::string message;
    dts::common::security::Scope scope{dts::common::security::Scope::Viewer};
};

class Authenticator {
public:
    explicit Authenticator(const config::SecurityConfig& config);

    AuthResult authorize(const dts::common::rest::HttpRequest& request,
                         AccessLevel level) const;

private:
    std::optional<dts::common::security::BearerValidator> validator_;
    bool allow_unauthenticated_viewer_{false};
    bool allow_unauthenticated_control_{false};
    std::vector<std::string> allowed_roles_;

    bool is_role_allowed(std::string_view role) const;
    static std::string_view header_value(
        const dts::common::rest::HttpRequest& request, std::string_view key);
};

}  // namespace rcc::auth
