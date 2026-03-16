#include "rcc/config/config_manager.hpp"

#include <yaml-cpp/yaml.h>
#include <stdexcept>

namespace rcc::config {

namespace {

SecurityConfig parse_security(const YAML::Node& node) {
    SecurityConfig security;
    if (!node) {
        return security;
    }

    security.token_secret = node["token_secret"] ? node["token_secret"].as<std::string>() : "";
    if (const auto roles = node["allowed_roles"]; roles && roles.IsSequence()) {
        for (const auto& entry : roles) {
            security.allowed_roles.emplace_back(entry.as<std::string>());
        }
    }
    return security;
}

TelemetryConfig parse_telemetry(const YAML::Node& node) {
    TelemetryConfig telemetry;
    if (!node) {
        return telemetry;
    }

    if (const auto value = node["event_buffer_size"]) {
        telemetry.event_buffer_size = value.as<std::size_t>();
    }
    if (const auto value = node["event_retention_hours"]) {
        telemetry.event_retention_hours = value.as<int>();
    }
    if (const auto value = node["heartbeat_interval_sec"]) {
        telemetry.heartbeat_interval_sec = value.as<int>();
    }
    if (const auto value = node["max_clients"]) {
        telemetry.max_clients = value.as<std::size_t>();
    }
    return telemetry;
}

NetworkConfig parse_network(const YAML::Node& node) {
    NetworkConfig network;
    if (!node) {
        return network;
    }

    if (const auto host = node["host"]) {
        network.host = host.as<std::string>();
    }
    if (const auto port = node["api_port"]) {
        network.api_port = port.as<uint16_t>();
    }
    return network;
}

std::vector<RadioProfile> parse_radios(const YAML::Node& node) {
    std::vector<RadioProfile> radios;
    if (!node || !node.IsSequence()) {
        return radios;
    }

    for (const auto& radio_node : node) {
        RadioProfile profile;
        if (const auto id = radio_node["id"]) {
            profile.id = id.as<std::string>();
        }
        if (const auto adapter = radio_node["adapter"]) {
            profile.adapter = adapter.as<std::string>();
        }
        if (const auto endpoint = radio_node["endpoint"]) {
            profile.endpoint = endpoint.as<std::string>();
        }
        radios.emplace_back(std::move(profile));
    }
    return radios;
}

}  // namespace

ConfigManager::ConfigManager(std::filesystem::path path)
    : path_(std::move(path))
    , config_(load_from_file(path_)) {}

Configuration ConfigManager::load_from_file(const std::filesystem::path& path) {
    if (!std::filesystem::exists(path)) {
        throw std::runtime_error("Config file not found: " + path.string());
    }

    const YAML::Node root = YAML::LoadFile(path.string());

    Configuration config;
    if (const auto container = root["container"]) {
        config.container_id = container["id"] ? container["id"].as<std::string>() : "";
        config.deployment = container["deployment"] ? container["deployment"].as<std::string>() : "";
    }

    config.network = parse_network(root["network"]);
    config.telemetry = parse_telemetry(root["telemetry"]);
    config.security = parse_security(root["security"]);
    config.radios = parse_radios(root["radios"]);

    return config;
}

}  // namespace rcc::config

#include "rcc/config/config_manager.hpp"

#include <yaml-cpp/yaml.h>
#include <fstream>
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
    return Duration{std::chrono::seconds{seconds}};
}

Config parseConfig(const YAML::Node& root) {
    Config cfg;

    if (const auto container = root["container"]) {
        cfg.container.container_id = container["id"].as<std::string>("");
        cfg.container.deployment = container["deployment"].as<std::string>("");
        cfg.container.soldier_id = container["soldier_id"].as<std::string>("");
    } else {
        throw std::runtime_error("Missing 'container' section");
    }

    if (const auto network = root["network"]) {
        cfg.network.bind_address = network["bind_address"].as<std::string>("0.0.0.0");
        cfg.network.command_port = static_cast<uint16_t>(network["command_port"].as<int>(8080));
    }

    if (const auto telemetry = root["telemetry"]) {
        cfg.telemetry.sse_port = static_cast<uint16_t>(telemetry["sse_port"].as<int>(cfg.network.command_port));
        cfg.telemetry.heartbeat_interval = parseDurationSeconds<std::chrono::seconds>(
            telemetry, "heartbeat_interval_sec", std::chrono::seconds{30});
        cfg.telemetry.event_buffer_size = telemetry["event_buffer_size"].as<std::size_t>(512);
        cfg.telemetry.event_retention = parseDurationSeconds<std::chrono::hours>(
            telemetry, "event_retention_hours", std::chrono::hours{24});
    }

    if (const auto security = root["security"]) {
        cfg.security.token_secret = security["token_secret"].as<std::string>("");
        cfg.security.allowed_roles = security["allowed_roles"].as<std::vector<std::string>>(std::vector<std::string>{});
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
            radio.id = node["id"].as<std::string>("");
            radio.adapter = node["adapter"].as<std::string>("");
            radio.endpoint = node["endpoint"].as<std::string>("");
            if (node["description"]) {
                radio.description = node["description"].as<std::string>();
            }

            if (radio.id.empty() || radio.adapter.empty() || radio.endpoint.empty()) {
                throw std::runtime_error("Radio entries require 'id', 'adapter', and 'endpoint'");
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
        throw std::runtime_error("Configuration file not found: " + path.string());
    }

    YAML::Node root = YAML::LoadFile(path.string());
    if (!root) {
        throw std::runtime_error("Failed to parse configuration file: " + path.string());
    }

    return parseConfig(root);
}

}  // namespace rcc::config

#include "rcc/config/config_manager.hpp"

#include <yaml-cpp/yaml.h>

#include <cctype>
#include <iomanip>
#include <iostream>
#include <stdexcept>

namespace rcc::config {

namespace {

std::chrono::seconds parseDuration(const std::string& value) {
    if (value.empty()) {
        return std::chrono::seconds{0};
    }

    char suffix = value.back();
    std::string numberPart = value;
    if (!std::isalpha(static_cast<unsigned char>(suffix))) {
        suffix = 's';
    } else {
        numberPart = value.substr(0, value.size() - 1);
    }

    double numeric = std::stod(numberPart);

    switch (suffix) {
        case 's':
        case 'S':
            return std::chrono::seconds{static_cast<int>(numeric)};
        case 'm':
        case 'M':
            return std::chrono::minutes{static_cast<int>(numeric)};
        case 'h':
        case 'H':
            return std::chrono::hours{static_cast<int>(numeric)};
        default:
            throw std::invalid_argument("Unsupported duration suffix: " + std::string{suffix});
    }
}

ChannelEntry parseChannel(const YAML::Node& node) {
    ChannelEntry channel;
    channel.index = node["index"].as<int>();
    channel.frequencyMHz = node["frequency_mhz"].as<double>();
    if (node["description"]) {
        channel.description = node["description"].as<std::string>();
    }
    return channel;
}

RadioConfig parseRadio(const YAML::Node& node) {
    RadioConfig radio;
    radio.id = node["radio_id"].as<std::string>();
    radio.model = node["model"].as<std::string>();

    if (const auto channels = node["channels"]; channels && channels.IsSequence()) {
        for (const auto& entry : channels) {
            radio.channels.push_back(parseChannel(entry));
        }
    }

    return radio;
}

}  // namespace

ConfigManager::ConfigManager(std::string_view path) : path_{path} {
    config_ = loadFromFile(path_);
    std::cout << "[ConfigManager] Loaded config from " << path_.string() << std::endl;
}

const ContainerConfig& ConfigManager::get() const noexcept {
    return config_;
}

const std::filesystem::path& ConfigManager::path() const noexcept {
    return path_;
}

ContainerConfig ConfigManager::loadFromFile(const std::filesystem::path& path) const {
    ContainerConfig cfg{};

    try {
        const YAML::Node root = YAML::LoadFile(path.string());

        if (const auto system = root["system"]) {
            if (system["service_name"]) {
                cfg.containerId = system["service_name"].as<std::string>();
            }
            if (system["listen_address"]) {
                cfg.network.listenAddress = system["listen_address"].as<std::string>();
            }
            if (system["port"]) {
                cfg.network.port = system["port"].as<std::uint16_t>();
            }
            if (system["auth_required"]) {
                cfg.auth.required = system["auth_required"].as<bool>();
            }
        }

        if (const auto auth = root["auth"]; auth && auth.IsMap()) {
            if (auth["jwt_secret"]) {
                cfg.auth.jwtSecret = auth["jwt_secret"].as<std::string>();
            }
            if (auth["issuer"]) {
                cfg.auth.issuer = auth["issuer"].as<std::string>();
            }
            if (auth["audience"]) {
                cfg.auth.audience = auth["audience"].as<std::string>();
            }
        }

        if (const auto timing = root["timing"]; timing && timing.IsMap()) {
            if (const auto telemetry = timing["telemetry"]; telemetry && telemetry.IsMap()) {
                if (telemetry["heartbeat_timeout"]) {
                    cfg.telemetry.clientIdleTimeout =
                        parseDuration(telemetry["heartbeat_timeout"].as<std::string>());
                }
            }
            if (const auto events = timing["events"]; events && events.IsMap()) {
                if (events["buffer_size_per_radio"]) {
                    cfg.telemetry.eventBufferSize = events["buffer_size_per_radio"].as<std::size_t>();
                }
                if (events["buffer_retention"]) {
                    cfg.telemetry.eventRetention =
                        std::chrono::duration_cast<std::chrono::hours>(
                            parseDuration(events["buffer_retention"].as<std::string>()));
                }
            }
        }

        if (const auto telemetry = root["telemetry"]; telemetry && telemetry.IsMap()) {
            if (telemetry["max_clients"]) {
                cfg.telemetry.maxSseClients = telemetry["max_clients"].as<std::size_t>();
            }
            if (telemetry["client_idle_timeout"]) {
                cfg.telemetry.clientIdleTimeout =
                    parseDuration(telemetry["client_idle_timeout"].as<std::string>());
            }
        }

        if (const auto channels = root["channels"]; channels && channels.IsMap()) {
            if (const auto radioChannels = channels["radio_channels"];
                radioChannels && radioChannels.IsMap()) {
                for (const auto& entry : radioChannels) {
                    cfg.radios.push_back(parseRadio(entry.second));
                }
            }
        }

    } catch (const std::exception& e) {
        std::cerr << "[ConfigManager] Failed to parse config: " << e.what()
                  << ". Using defaults." << std::endl;
    }

    return cfg;
}

}  // namespace rcc::config



