#include "rcc/config/config_manager.hpp"

#include <yaml-cpp/yaml.h>
#include <stdexcept>

namespace rcc::config {

namespace {

template <typename Duration>
Duration parseDurationSeconds(const YAML::Node& node, const std::string& key, Duration fallback) {
    if (!node[key]) {
        return fallback;
    }
    const auto seconds = node[key].as<int64_t>();
    if (seconds <= 0) {
        throw std::runtime_error("Duration for '" + key + "' must be positive");
    }
    return std::chrono::duration_cast<Duration>(std::chrono::seconds{seconds});
}

Config parseConfig(const YAML::Node& root) {
    Config cfg;

    if (const auto container = root["container"]) {
        cfg.container.container_id = container["id"].as<std::string>("");
        cfg.container.deployment   = container["deployment"].as<std::string>("");
        cfg.container.soldier_id   = container["soldier_id"].as<std::string>("");
    } else {
        throw std::runtime_error("Missing 'container' section");
    }

    if (const auto network = root["network"]) {
        cfg.network.bind_address = network["bind_address"].as<std::string>("0.0.0.0");
        cfg.network.command_port =
            static_cast<uint16_t>(network["command_port"].as<int>(8080));
    }

    if (const auto telemetry = root["telemetry"]) {
        cfg.telemetry.sse_port =
            static_cast<uint16_t>(telemetry["sse_port"].as<int>(0));
        cfg.telemetry.heartbeat_interval = parseDurationSeconds<std::chrono::seconds>(
            telemetry, "heartbeat_interval_sec", std::chrono::seconds{30});
        cfg.telemetry.event_buffer_size =
            telemetry["event_buffer_size"].as<std::size_t>(512);
        cfg.telemetry.event_retention = parseDurationSeconds<std::chrono::hours>(
            telemetry, "event_retention_hours", std::chrono::hours{24});
        cfg.telemetry.max_sse_clients =
            telemetry["max_sse_clients"].as<std::size_t>(8);
        cfg.telemetry.client_idle_timeout = parseDurationSeconds<std::chrono::seconds>(
            telemetry, "client_idle_timeout_sec", std::chrono::seconds{60});
    }

    if (const auto security = root["security"]) {
        cfg.security.token_secret =
            security["token_secret"].as<std::string>("");
        cfg.security.allowed_roles =
            security["allowed_roles"].as<std::vector<std::string>>(
                std::vector<std::string>{});
        cfg.security.token_ttl = parseDurationSeconds<std::chrono::seconds>(
            security, "token_ttl_sec", std::chrono::seconds{300});
    } else {
        throw std::runtime_error("Missing 'security' section");
    }

    if (const auto timing = root["timing"]) {
        cfg.timing.normal_probe = parseDurationSeconds<std::chrono::seconds>(
            timing, "normal_probe_sec", std::chrono::seconds{30});
        cfg.timing.recovering_probe = parseDurationSeconds<std::chrono::seconds>(
            timing, "recovering_probe_sec", std::chrono::seconds{10});
        cfg.timing.offline_probe = parseDurationSeconds<std::chrono::seconds>(
            timing, "offline_probe_sec", std::chrono::seconds{60});
    }

    if (const auto radios = root["radios"]) {
        for (const auto& node : radios) {
            RadioEntry radio;
            radio.id       = node["id"].as<std::string>("");
            radio.adapter  = node["adapter"].as<std::string>("");
            radio.endpoint = node["endpoint"].as<std::string>("");
            if (node["description"]) {
                radio.description = node["description"].as<std::string>();
            }

            if (radio.id.empty() || radio.adapter.empty() || radio.endpoint.empty()) {
                throw std::runtime_error(
                    "Radio entries require 'id', 'adapter', and 'endpoint'");
            }

            cfg.radios.emplace_back(std::move(radio));
        }
    }

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
    if (!std::filesystem::exists(path)) {
        throw std::runtime_error(
            "Configuration file not found: " + path.string());
    }

    YAML::Node root = YAML::LoadFile(path.string());
    if (!root) {
        throw std::runtime_error(
            "Failed to parse configuration file: " + path.string());
    }

    return parseConfig(root);
}

}  // namespace rcc::config
