#include "rcc/auth/authenticator.hpp"

#include <algorithm>

namespace rcc::auth {

Authenticator::Authenticator(const config::SecurityConfig& config) {
    allowed_roles_ = config.allowed_roles;

    if (!config.token_secret.empty()) {
        validator_.emplace(config.token_secret);
    } else {
        allow_unauthenticated_viewer_ = true;
        allow_unauthenticated_control_ = true;
    }

    // If roles not configured, default to allowing both
    if (allowed_roles_.empty()) {
        allow_unauthenticated_viewer_ = true;
    } else {
        allow_unauthenticated_viewer_ = is_role_allowed("viewer");
        allow_unauthenticated_control_ = is_role_allowed("controller");
    }
}

AuthResult Authenticator::authorize(const dts::common::rest::HttpRequest& request, AccessLevel level) const {
    if (!validator_) {
        // No validator configured → rely on allow lists
        if (level == AccessLevel::Telemetry && allow_unauthenticated_viewer_) {
            return {.allowed = true, .subject = "anonymous"};
        }
        if (level == AccessLevel::Control && allow_unauthenticated_control_) {
            return {.allowed = true, .subject = "anonymous"};
        }
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

    std::string requiredRole = (level == AccessLevel::Telemetry) ? "viewer" : "controller";
    if (!allowed_roles_.empty() && !is_role_allowed(requiredRole)) {
        return {.allowed = false, .message = "Role not permitted by configuration"};
    }

    return {.allowed = true, .subject = info.subject, .scope = info.scope};
}

bool Authenticator::is_role_allowed(std::string_view role) const {
    if (allowed_roles_.empty()) {
        return true;
    }
    return std::find(allowed_roles_.begin(), allowed_roles_.end(), role) != allowed_roles_.end();
}

std::string_view Authenticator::header_value(const dts::common::rest::HttpRequest& request, std::string_view key) {
    auto it = request.headers.find(std::string(key));
    if (it == request.headers.end()) {
        return {};
    }
    return it->second;
}

}  // namespace rcc::auth

#include "rcc/auth/authenticator.hpp"

namespace rcc::auth {

namespace {

Scope mapScope(dts::common::security::Scope scope) {
    using dtsScope = dts::common::security::Scope;

    switch (scope) {
        case dtsScope::Admin:
            return Scope::Admin;
        case dtsScope::Operator:
            return Scope::Controller;
        case dtsScope::Viewer:
        default:
            return Scope::Viewer;
    }
}

}  // namespace

Authenticator::Authenticator(const config::SecurityConfig& security)
    : validator_(security.token_secret) {}

AuthResult Authenticator::validate(const std::string& authorizationHeader) const {
    const auto info = validator_.validate(authorizationHeader);
    AuthResult result;
    result.granted = info.valid;
    result.scope = mapScope(info.scope);
    result.subject = info.subject;
    return result;
}

}  // namespace rcc::auth

#include "rcc/auth/authenticator.hpp"

#include <algorithm>

namespace rcc::auth {

AuthContext Authenticator::authenticate(std::string_view token) const {
    AuthContext ctx;
    if (!token.empty()) {
        ctx.valid = true;
        ctx.subject = std::string{token};
        ctx.scopes = {"radio:read"};
    }
    return ctx;
}

bool Authenticator::hasScope(const AuthContext& ctx, std::string_view scope) const {
    return ctx.valid &&
           std::find(ctx.scopes.begin(), ctx.scopes.end(), scope) != ctx.scopes.end();
}

}  // namespace rcc::auth


