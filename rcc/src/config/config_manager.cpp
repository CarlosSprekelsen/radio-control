#include "rcc/config/config_manager.hpp"

#include <dts/common/config/sections.hpp>
#include <dts/common/config/yaml.hpp>

#include <stdexcept>

namespace rcc::config {

namespace {

std::string joinPath(std::string_view parent, std::string_view key) {
    if (parent.empty()) {
        return std::string(key);
    }
    return std::string(parent) + "." + std::string(key);
}

template <typename T>
std::optional<T> optionalScalar(const YAML::Node& node,
                                std::string_view key,
                                std::string_view parentPath) {
    return dts::common::config::optionalScalar<T>(node[std::string(key)],
                                                  joinPath(parentPath, key));
}

template <typename Duration>
Duration readPositiveDurationSeconds(const YAML::Node& node,
                                     std::string_view key,
                                     Duration fallback,
                                     std::string_view parentPath) {
    return dts::common::config::readPositiveDurationSeconds(
        node, key, fallback, parentPath);
}

std::chrono::hours readPositiveDurationHours(const YAML::Node& node,
                                             std::string_view key,
                                             std::chrono::hours fallback,
                                             std::string_view parentPath) {
    const auto child = node[std::string(key)];
    if (!child) {
        return fallback;
    }

    const auto hours = dts::common::config::requiredScalar<int64_t>(
        child, joinPath(parentPath, key));
    if (hours <= 0) {
        throw dts::common::config::ConfigError(
            "Duration for '" + joinPath(parentPath, key) + "' must be positive");
    }
    return std::chrono::hours{hours};
}

ContainerInfo loadContainerInfo(const YAML::Node& node) {
    ContainerInfo cfg;
    if (const auto value = optionalScalar<std::string>(node, "id", "container")) {
        cfg.container_id = *value;
    }
    if (const auto value =
            optionalScalar<std::string>(node, "deployment", "container")) {
        cfg.deployment = *value;
    }
    if (const auto value =
            optionalScalar<std::string>(node, "soldier_id", "container")) {
        cfg.soldier_id = *value;
    }
    return cfg;
}

NetworkConfig loadNetworkConfigCompat(const YAML::Node& node) {
    NetworkConfig cfg;

    dts::common::config::NetworkConfig defaults;
    defaults.bindAddress = cfg.bind_address;
    defaults.port = cfg.command_port;

    const auto shared = dts::common::config::loadNetworkConfig(node, defaults);
    cfg.bind_address = shared.bindAddress;
    cfg.command_port = shared.port;

    // Preserve the existing snake_case RCC config contract when both forms
    // are present, while also accepting the shared dts-common key names.
    if (const auto value =
            optionalScalar<std::string>(node, "bind_address", "network")) {
        cfg.bind_address = *value;
    }
    if (const auto value =
            optionalScalar<uint16_t>(node, "command_port", "network")) {
        cfg.command_port = *value;
    }

    return cfg;
}

TelemetryConfig loadTelemetryConfigCompat(const YAML::Node& node) {
    TelemetryConfig cfg;

    dts::common::config::TelemetryConfig defaults;
    defaults.heartbeatIntervalSec =
        static_cast<uint32_t>(cfg.heartbeat_interval.count());
    defaults.eventBufferSize = static_cast<uint32_t>(cfg.event_buffer_size);
    defaults.eventRetentionHours =
        static_cast<uint32_t>(cfg.event_retention.count());
    defaults.maxConcurrentClients =
        static_cast<uint32_t>(cfg.max_sse_clients);
    defaults.clientIdleTimeoutSec =
        static_cast<uint32_t>(cfg.client_idle_timeout.count());

    const auto shared = dts::common::config::loadTelemetryConfig(node, defaults);
    cfg.heartbeat_interval = std::chrono::seconds(shared.heartbeatIntervalSec);
    cfg.event_buffer_size = shared.eventBufferSize;
    cfg.event_retention = std::chrono::hours(shared.eventRetentionHours);
    cfg.max_sse_clients = shared.maxConcurrentClients;
    cfg.client_idle_timeout = std::chrono::seconds(shared.clientIdleTimeoutSec);

    if (const auto value =
            optionalScalar<uint16_t>(node, "sse_port", "telemetry")) {
        cfg.sse_port = *value;
    }
    if (node["heartbeat_interval_sec"]) {
        cfg.heartbeat_interval = readPositiveDurationSeconds(
            node, "heartbeat_interval_sec", cfg.heartbeat_interval, "telemetry");
    }
    if (const auto value =
            optionalScalar<std::size_t>(node, "event_buffer_size", "telemetry")) {
        cfg.event_buffer_size = *value;
    }
    if (node["event_retention_hours"]) {
        cfg.event_retention = readPositiveDurationHours(
            node, "event_retention_hours", cfg.event_retention, "telemetry");
    }
    if (const auto value =
            optionalScalar<std::size_t>(node, "max_sse_clients", "telemetry")) {
        cfg.max_sse_clients = *value;
    }
    if (node["client_idle_timeout_sec"]) {
        cfg.client_idle_timeout = readPositiveDurationSeconds(
            node, "client_idle_timeout_sec", cfg.client_idle_timeout, "telemetry");
    }

    return cfg;
}

SecurityConfig loadSecurityConfig(const YAML::Node& node) {
    SecurityConfig cfg;

    if (const auto value =
            optionalScalar<std::string>(node, "token_secret", "security")) {
        cfg.token_secret = *value;
    }
    if (const auto value = optionalScalar<bool>(
            node, "allow_unauthenticated_dev_access", "security")) {
        cfg.allow_unauthenticated_dev_access = *value;
    }
    if (const auto value = optionalScalar<std::vector<std::string>>(
            node, "allowed_roles", "security")) {
        cfg.allowed_roles = *value;
    }
    cfg.token_ttl = readPositiveDurationSeconds(
        node, "token_ttl_sec", cfg.token_ttl, "security");

    return cfg;
}

TimingProfile loadTimingProfile(const YAML::Node& node) {
    TimingProfile cfg;

    cfg.normal_probe = readPositiveDurationSeconds(
        node, "normal_probe_sec", cfg.normal_probe, "timing");
    cfg.recovering_probe = readPositiveDurationSeconds(
        node, "recovering_probe_sec", cfg.recovering_probe, "timing");
    cfg.offline_probe = readPositiveDurationSeconds(
        node, "offline_probe_sec", cfg.offline_probe, "timing");

    return cfg;
}

std::vector<RadioEntry> loadRadioEntries(const YAML::Node& node) {
    std::vector<RadioEntry> radios;
    if (!node) {
        return radios;
    }

    for (const auto& entryNode : node) {
        RadioEntry radio;
        if (const auto value =
                optionalScalar<std::string>(entryNode, "id", "radios[].id")) {
            radio.id = *value;
        }
        if (const auto value = optionalScalar<std::string>(
                entryNode, "adapter", "radios[].adapter")) {
            radio.adapter = *value;
        }
        if (const auto value = optionalScalar<std::string>(
                entryNode, "endpoint", "radios[].endpoint")) {
            radio.endpoint = *value;
        }
        radio.description = dts::common::config::optionalString(
            entryNode["description"], "radios[].description");

        if (radio.id.empty() || radio.adapter.empty() || radio.endpoint.empty()) {
            throw std::runtime_error(
                "Radio entries require 'id', 'adapter', and 'endpoint'");
        }

        radios.emplace_back(std::move(radio));
    }

    return radios;
}

ServiceDiscoveryConfig loadServiceDiscoveryConfig(const YAML::Node& node) {
    ServiceDiscoveryConfig cfg;
    if (!node) {
        return cfg;
    }
    if (const auto v = optionalScalar<bool>(node, "enabled", "serviceDiscovery")) {
        cfg.enabled = *v;
    }
    if (const auto v = optionalScalar<uint16_t>(node, "port", "serviceDiscovery")) {
        cfg.port = *v;
    }
    if (const auto v = optionalScalar<int>(node, "ttl", "serviceDiscovery")) {
        cfg.ttl = *v;
    }
    if (const auto v = optionalScalar<int>(node, "startupBurstCount", "serviceDiscovery")) {
        cfg.startup_burst_count = *v;
    }
    if (const auto v = optionalScalar<int>(node, "startupBurstSpacingMs", "serviceDiscovery")) {
        cfg.startup_burst_spacing_ms = *v;
    }
    if (const auto v = optionalScalar<std::string>(node, "bindAddress", "serviceDiscovery")) {
        cfg.bind_address = *v;
    }
    cfg.interface_hint = dts::common::config::optionalString(
        node["interfaceHint"], "serviceDiscovery.interfaceHint");
    return cfg;
}

Config parseConfig(const YAML::Node& root) {
    Config cfg;

    const auto container =
        dts::common::config::requireChild(root, "container");
    cfg.container = loadContainerInfo(container);

    if (const auto network = root["network"]) {
        cfg.network = loadNetworkConfigCompat(network);
    }

    if (const auto telemetry = root["telemetry"]) {
        cfg.telemetry = loadTelemetryConfigCompat(telemetry);
    }

    const auto security =
        dts::common::config::requireChild(root, "security");
    cfg.security = loadSecurityConfig(security);

    cfg.service_discovery =
        loadServiceDiscoveryConfig(root["serviceDiscovery"]);

    if (const auto timing = root["timing"]) {
        cfg.timing = loadTimingProfile(timing);
    }

    cfg.radios = loadRadioEntries(root["radios"]);

    return cfg;
}

}  // namespace

ConfigManager::ConfigManager(std::filesystem::path path)
    : path_(std::move(path)),
      config_(loadFromFile(path_)) {}

const Config& ConfigManager::current() const {
    std::scoped_lock lock(mutex_);
    return config_;
}

void ConfigManager::reload() {
    Config updated = loadFromFile(path_);
    std::scoped_lock lock(mutex_);
    config_ = std::move(updated);
}

Config ConfigManager::loadFromFile(const std::filesystem::path& path) const {
    return parseConfig(dts::common::config::YamlDocument::load(path).root());
}

}  // namespace rcc::config
