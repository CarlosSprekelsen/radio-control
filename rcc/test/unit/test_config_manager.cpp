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
  allow_unauthenticated_dev_access: false
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
    EXPECT_FALSE(cfg.security.allow_unauthenticated_dev_access);
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

TEST(ConfigManager, AcceptsSharedDtsCommonStyleKeys) {
    const std::string yaml = R"yaml(
container:
  id: "shared-style"
  deployment: "ci"
network:
  bindAddress: "127.0.0.1"
  apiPort: 19090
telemetry:
  sse_port: 19091
  heartbeatIntervalSec: 11
  eventBufferSize: 128
  eventRetentionHours: 2
  maxConcurrentClients: 6
  clientIdleTimeoutSec: 45
security:
  token_secret: "shared-secret"
  allow_unauthenticated_dev_access: true
)yaml";
    const auto path = writeTmpYaml(yaml);
    rcc::config::ConfigManager mgr(path);
    const auto& cfg = mgr.current();

    EXPECT_EQ(cfg.network.bind_address, "127.0.0.1");
    EXPECT_EQ(cfg.network.command_port, 19090);
    EXPECT_EQ(cfg.telemetry.sse_port, 19091);
    EXPECT_EQ(cfg.telemetry.heartbeat_interval, std::chrono::seconds{11});
    EXPECT_EQ(cfg.telemetry.event_buffer_size, 128u);
    EXPECT_EQ(cfg.telemetry.event_retention, std::chrono::hours{2});
    EXPECT_EQ(cfg.telemetry.max_sse_clients, 6u);
    EXPECT_EQ(cfg.telemetry.client_idle_timeout, std::chrono::seconds{45});
    EXPECT_TRUE(cfg.security.allow_unauthenticated_dev_access);
}

TEST(ConfigManager, LegacyKeysOverrideSharedAliasesAndPreserveHourUnits) {
    const std::string yaml = R"yaml(
container:
  id: "legacy-wins"
network:
  bindAddress: "0.0.0.0"
  apiPort: 18080
  bind_address: "127.0.0.1"
  command_port: 19090
telemetry:
  sse_port: 19091
  heartbeatIntervalSec: 11
  eventBufferSize: 32
  eventRetentionHours: 6
  maxConcurrentClients: 2
  clientIdleTimeoutSec: 12
  heartbeat_interval_sec: 13
  event_buffer_size: 64
  event_retention_hours: 1
  max_sse_clients: 3
  client_idle_timeout_sec: 14
security:
  token_secret: "shared-secret"
)yaml";
    const auto path = writeTmpYaml(yaml);
    rcc::config::ConfigManager mgr(path);
    const auto& cfg = mgr.current();

    EXPECT_EQ(cfg.network.bind_address, "127.0.0.1");
    EXPECT_EQ(cfg.network.command_port, 19090);
    EXPECT_EQ(cfg.telemetry.heartbeat_interval, std::chrono::seconds{13});
    EXPECT_EQ(cfg.telemetry.event_buffer_size, 64u);
    EXPECT_EQ(cfg.telemetry.event_retention, std::chrono::hours{1});
    EXPECT_EQ(cfg.telemetry.max_sse_clients, 3u);
    EXPECT_EQ(cfg.telemetry.client_idle_timeout, std::chrono::seconds{14});
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
