#include "rcc/config/config_manager.hpp"
#include <gtest/gtest.h>

#include <filesystem>
#include <fstream>
#include <stdexcept>

namespace {

const std::string kMinimalYaml = R"yaml(
container:
  id: "test-rcc"
  deployment: "ci"
  soldier_id: "test-op"

network:
  bind_address: "127.0.0.1"
  command_port: 9090

telemetry:
  sse_port: 9091
  heartbeat_interval_sec: 10
  event_buffer_size: 64
  event_retention_hours: 1
  max_sse_clients: 4
  client_idle_timeout_sec: 30

security:
  token_secret: "unit-test-secret"
  allowed_roles:
    - viewer
    - controller
  token_ttl_sec: 60

timing:
  normal_probe_sec: 10
  recovering_probe_sec: 5
  offline_probe_sec: 20

radios:
  - id: "radio-1"
    adapter: "silvus"
    endpoint: "http://127.0.0.1:19000"
)yaml";

std::filesystem::path writeTmpYaml(const std::string& content) {
    auto path = std::filesystem::temp_directory_path() / "rcc_test_config.yaml";
    std::ofstream f(path);
    f << content;
    return path;
}

}  // namespace

TEST(ConfigManager, LoadsRequiredFields) {
    const auto path = writeTmpYaml(kMinimalYaml);
    rcc::config::ConfigManager mgr(path);
    const auto& cfg = mgr.current();

    EXPECT_EQ(cfg.container.container_id, "test-rcc");
    EXPECT_EQ(cfg.container.deployment,   "ci");
    EXPECT_EQ(cfg.container.soldier_id,   "test-op");
    EXPECT_EQ(cfg.network.command_port,   9090);
    EXPECT_EQ(cfg.telemetry.sse_port,     9091);
    EXPECT_EQ(cfg.telemetry.event_buffer_size, 64u);
    EXPECT_EQ(cfg.security.token_secret,  "unit-test-secret");
    ASSERT_EQ(cfg.radios.size(),          1u);
    EXPECT_EQ(cfg.radios[0].id,           "radio-1");
    EXPECT_EQ(cfg.radios[0].adapter,      "silvus");
}

TEST(ConfigManager, DefaultNetworkValues) {
    const std::string yaml = R"yaml(
container:
  id: "defaults-test"
security:
  token_secret: "s"
)yaml";
    const auto path = writeTmpYaml(yaml);
    rcc::config::ConfigManager mgr(path);
    const auto& cfg = mgr.current();

    EXPECT_EQ(cfg.network.bind_address, "0.0.0.0");
    EXPECT_EQ(cfg.network.command_port, 8080);
    EXPECT_EQ(cfg.telemetry.max_sse_clients, 8u);
}

TEST(ConfigManager, ThrowsOnMissingFile) {
    EXPECT_THROW(
        rcc::config::ConfigManager mgr("/tmp/does_not_exist_rcc_xyzzy.yaml"),
        std::runtime_error);
}

TEST(ConfigManager, ThrowsOnMissingSecuritySection) {
    const std::string yaml = R"yaml(
container:
  id: "no-security"
)yaml";
    EXPECT_THROW(
        rcc::config::ConfigManager mgr(writeTmpYaml(yaml)),
        std::runtime_error);
}

TEST(ConfigManager, ReloadUpdatesConfig) {
    auto path = writeTmpYaml(kMinimalYaml);
    rcc::config::ConfigManager mgr(path);
    EXPECT_EQ(mgr.current().container.container_id, "test-rcc");

    // Overwrite with different id
    {
        std::ofstream f(path);
        f << R"yaml(
container:
  id: "reloaded"
security:
  token_secret: "s"
)yaml";
    }
    mgr.reload();
    EXPECT_EQ(mgr.current().container.container_id, "reloaded");
}

TEST(ConfigManager, TimingDefaults) {
    const auto path = writeTmpYaml(kMinimalYaml);
    rcc::config::ConfigManager mgr(path);
    const auto& t = mgr.current().timing;
    EXPECT_EQ(t.normal_probe,     std::chrono::seconds{10});
    EXPECT_EQ(t.recovering_probe, std::chrono::seconds{5});
    EXPECT_EQ(t.offline_probe,    std::chrono::seconds{20});
}
