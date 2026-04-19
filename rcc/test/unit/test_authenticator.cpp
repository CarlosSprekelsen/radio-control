#include "rcc/auth/authenticator.hpp"
#include "support/test_utils.hpp"
#include <gtest/gtest.h>

namespace {

rcc::config::SecurityConfig makeConfig(
    const std::string& secret          = "test-secret-key-16bytes",
    std::vector<std::string> roles     = {"viewer", "controller"})
{
    rcc::config::SecurityConfig cfg;
    cfg.token_secret   = secret;
    cfg.allowed_roles  = std::move(roles);
    return cfg;
}

dts::common::rest::HttpRequest makeRequest(const std::string& authHeader) {
    dts::common::rest::HttpRequest req;
    req.method  = "GET";
    req.path    = "/api/v1/radios";
    if (!authHeader.empty()) {
        req.headers["authorization"] = authHeader;
    }
    return req;
}

}  // namespace

TEST(Authenticator, AllowsValidViewerToken) {
    rcc::auth::Authenticator auth(makeConfig());
    const auto token = rcc::test::createTestJWT("viewer");
    const auto req   = makeRequest("Bearer " + token);
    const auto result = auth.authorize(req, rcc::auth::AccessLevel::Telemetry);
    EXPECT_TRUE(result.allowed);
    EXPECT_EQ(result.subject, "test-user");
}

TEST(Authenticator, AllowsValidOperatorForControl) {
    rcc::auth::Authenticator auth(makeConfig());
    const auto token  = rcc::test::createTestJWT("operator");
    const auto result = auth.authorize(
        makeRequest("Bearer " + token), rcc::auth::AccessLevel::Control);
    EXPECT_TRUE(result.allowed);
}

TEST(Authenticator, RejectsViewerForControl) {
    rcc::auth::Authenticator auth(makeConfig());
    const auto token  = rcc::test::createTestJWT("viewer");
    const auto result = auth.authorize(
        makeRequest("Bearer " + token), rcc::auth::AccessLevel::Control);
    EXPECT_FALSE(result.allowed);
    EXPECT_FALSE(result.message.empty());
    EXPECT_EQ(result.http_status, 403);
    EXPECT_EQ(result.error_code, "FORBIDDEN");
}

TEST(Authenticator, RejectsMissingHeader) {
    rcc::auth::Authenticator auth(makeConfig());
    const auto result = auth.authorize(
        makeRequest(""), rcc::auth::AccessLevel::Telemetry);
    EXPECT_FALSE(result.allowed);
    EXPECT_EQ(result.http_status, 401);
    EXPECT_EQ(result.error_code, "UNAUTHORIZED");
}

TEST(Authenticator, RejectsGarbageToken) {
    rcc::auth::Authenticator auth(makeConfig());
    const auto result = auth.authorize(
        makeRequest("Bearer not.a.jwt"), rcc::auth::AccessLevel::Telemetry);
    EXPECT_FALSE(result.allowed);
}

TEST(Authenticator, NoSecretRejectsByDefault) {
    rcc::config::SecurityConfig cfg;
    cfg.token_secret = "";
    rcc::auth::Authenticator auth(cfg);
    const auto result = auth.authorize(
        makeRequest(""), rcc::auth::AccessLevel::Telemetry);
    EXPECT_FALSE(result.allowed);
    EXPECT_EQ(result.http_status, 401);
}

TEST(Authenticator, ExplicitDevOptInAllowsAnonymous) {
    rcc::config::SecurityConfig cfg;
    cfg.token_secret = "";
    cfg.allow_unauthenticated_dev_access = true;
    rcc::auth::Authenticator auth(cfg);
    const auto result = auth.authorize(
        makeRequest(""), rcc::auth::AccessLevel::Telemetry);
    EXPECT_TRUE(result.allowed);
    EXPECT_EQ(result.subject, "anonymous");
}

TEST(Authenticator, AdminTokenGrantsControl) {
    rcc::auth::Authenticator auth(makeConfig());
    const auto token  = rcc::test::createTestJWT("admin");
    const auto result = auth.authorize(
        makeRequest("Bearer " + token), rcc::auth::AccessLevel::Control);
    EXPECT_TRUE(result.allowed);
}

TEST(Authenticator, DevOptInStillRespectsAllowedRoles) {
    rcc::config::SecurityConfig cfg;
    cfg.token_secret = "";
    cfg.allow_unauthenticated_dev_access = true;
    cfg.allowed_roles = {"viewer"};
    rcc::auth::Authenticator auth(cfg);

    const auto telemetry = auth.authorize(
        makeRequest(""), rcc::auth::AccessLevel::Telemetry);
    const auto control = auth.authorize(
        makeRequest(""), rcc::auth::AccessLevel::Control);

    EXPECT_TRUE(telemetry.allowed);
    EXPECT_FALSE(control.allowed);
    EXPECT_EQ(control.http_status, 401);
}
