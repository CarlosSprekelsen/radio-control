#include "rcc/auth/authenticator.hpp"

#include <algorithm>

namespace rcc::auth {

Authenticator::Authenticator(const config::SecurityConfig& config) {
    allowed_roles_ = config.allowed_roles;

    if (!config.token_secret.empty()) {
        validator_.emplace(config.token_secret);
    } else {
        allow_unauthenticated_viewer_  = true;
        allow_unauthenticated_control_ = true;
    }

    if (allowed_roles_.empty()) {
        allow_unauthenticated_viewer_ = true;
    } else {
        allow_unauthenticated_viewer_  = is_role_allowed("viewer");
        allow_unauthenticated_control_ = is_role_allowed("controller");
    }
}

AuthResult Authenticator::authorize(const dts::common::rest::HttpRequest& request,
                                    AccessLevel level) const {
    if (!validator_) {
        if (level == AccessLevel::Telemetry && allow_unauthenticated_viewer_) {
            return {.allowed = true, .subject = "anonymous"};
        }
        if (level == AccessLevel::Control && allow_unauthenticated_control_) {
            return {.allowed = true, .subject = "anonymous"};
        }
        return {.allowed = false, .message = "Authentication required"};
    }

    auto authHeader = header_value(request, "authorization");
    if (authHeader.empty()) {
        return {.allowed = false, .message = "Missing Authorization header"};
    }

    const auto info = validator_->validate(std::string(authHeader));
    if (!info.valid) {
        return {.allowed = false, .message = "Invalid bearer token"};
    }

    bool permitted = false;
    if (level == AccessLevel::Telemetry) {
        permitted = dts::common::security::BearerValidator::hasViewerOrHigher(info);
    } else {
        permitted = dts::common::security::BearerValidator::hasOperatorOrHigher(info);
    }

    if (!permitted) {
        return {.allowed = false, .message = "Insufficient scope"};
    }

    std::string_view requiredRole =
        (level == AccessLevel::Telemetry) ? "viewer" : "controller";
    if (!allowed_roles_.empty() && !is_role_allowed(requiredRole)) {
        return {.allowed = false, .message = "Role not permitted by configuration"};
    }

    return {.allowed = true, .subject = info.subject, .scope = info.scope};
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
