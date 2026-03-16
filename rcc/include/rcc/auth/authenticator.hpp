#pragma once

#include "rcc/config/config_manager.hpp"
#include <dts/common/security/bearer_validator.hpp>
#include <dts/common/rest/http_parser.hpp>
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

    AuthResult authorize(const dts::common::rest::HttpRequest& request, AccessLevel level) const;

private:
    std::optional<dts::common::security::BearerValidator> validator_;
    bool allow_unauthenticated_viewer_{false};
    bool allow_unauthenticated_control_{false};
    std::vector<std::string> allowed_roles_;

    bool is_role_allowed(std::string_view role) const;
    static std::string_view header_value(const dts::common::rest::HttpRequest& request, std::string_view key);
};

}  // namespace rcc::auth

#pragma once

#include "rcc/config/types.hpp"

#include <dts/common/security/bearer_validator.hpp>

#include <mutex>
#include <string>

namespace rcc::auth {

enum class Scope {
    Viewer,
    Controller,
    Admin
};

struct AuthResult {
    bool granted{false};
    Scope scope{Scope::Viewer};
    std::string subject;
};

class Authenticator {
public:
    explicit Authenticator(const config::SecurityConfig& security);

    AuthResult validate(const std::string& authorizationHeader) const;

private:
    dts::common::security::BearerValidator validator_;
};

}  // namespace rcc::auth

#pragma once

#include <string>
#include <string_view>
#include <vector>

namespace rcc::auth {

struct AuthContext {
    bool valid{false};
    std::string subject{};
    std::vector<std::string> scopes{};
};

class Authenticator {
public:
    Authenticator() = default;
    ~Authenticator() = default;

    AuthContext authenticate(std::string_view token) const;
    bool hasScope(const AuthContext& ctx, std::string_view scope) const;
};

}  // namespace rcc::auth


