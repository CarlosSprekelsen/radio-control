#include "rcc/auth/authenticator.hpp"

#include <algorithm>

namespace rcc::auth {

Authenticator::Authenticator(const config::SecurityConfig& config) {
    allowed_roles_ = config.allowed_roles;

    if (!config.token_secret.empty()) {
        validator_.emplace(config.token_secret);
    } else if (config.allow_unauthenticated_dev_access) {
        if (allowed_roles_.empty()) {
            allow_unauthenticated_viewer_ = true;
            allow_unauthenticated_control_ = true;
        } else {
            allow_unauthenticated_viewer_ = is_role_allowed("viewer");
            allow_unauthenticated_control_ = is_role_allowed("controller");
        }
    }
}

AuthResult Authenticator::authorize(const dts::common::rest::HttpRequest& request,
                                    AccessLevel level) const {
    if (!validator_) {
        if (level == AccessLevel::Telemetry && allow_unauthenticated_viewer_) {
            return {true, "anonymous", {}, {}};
        }
        if (level == AccessLevel::Control && allow_unauthenticated_control_) {
            return {true, "anonymous", {}, {}};
        }
        return {false,
                {},
                "Authentication required",
                {},
                401,
                "UNAUTHORIZED"};
    }

    auto authHeader = header_value(request, "authorization");
    if (authHeader.empty()) {
        return {false,
                {},
                "Missing Authorization header",
                {},
                401,
                "UNAUTHORIZED"};
    }

    const auto info = validator_->validate(std::string(authHeader));
    if (!info.valid) {
        return {false,
                {},
                "Invalid bearer token",
                {},
                401,
                "UNAUTHORIZED"};
    }

    bool permitted = false;
    if (level == AccessLevel::Telemetry) {
        permitted = dts::common::security::BearerValidator::hasViewerOrHigher(info);
    } else {
        permitted = dts::common::security::BearerValidator::hasOperatorOrHigher(info);
    }

    if (!permitted) {
        return {false,
                {},
                "Insufficient scope",
                {},
                403,
                "FORBIDDEN"};
    }

    std::string_view requiredRole =
        (level == AccessLevel::Telemetry) ? "viewer" : "controller";
    if (!allowed_roles_.empty() && !is_role_allowed(requiredRole)) {
        return {false,
                {},
                "Role not permitted by configuration",
                {},
                403,
                "FORBIDDEN"};
    }

    return {true, info.subject, {}, info.scope};
}

bool Authenticator::is_role_allowed(std::string_view role) const {
    if (allowed_roles_.empty()) {
        return true;
    }
    return std::find(allowed_roles_.begin(), allowed_roles_.end(), role) !=
           allowed_roles_.end();
}

std::string_view Authenticator::header_value(
    const dts::common::rest::HttpRequest& request, std::string_view key) {
    auto it = request.headers.find(std::string(key));
    if (it == request.headers.end()) {
        return {};
    }
    return it->second;
}

}  // namespace rcc::auth
