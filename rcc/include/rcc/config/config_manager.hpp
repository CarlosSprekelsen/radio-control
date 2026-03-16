#pragma once

#include <filesystem>
#include <string>
#include <vector>

namespace rcc::config {

struct NetworkConfig {
    std::string host{"0.0.0.0"};
    uint16_t api_port{8080};
};

struct TelemetryConfig {
    std::size_t event_buffer_size{512};
    int event_retention_hours{24};
    int heartbeat_interval_sec{5};
    std::size_t max_clients{8};
};

struct SecurityConfig {
    std::string token_secret;
    std::vector<std::string> allowed_roles;
};

struct RadioProfile {
    std::string id;
    std::string adapter;
    std::string endpoint;
};

struct Configuration {
    std::string container_id;
    std::string deployment;
    NetworkConfig network;
    TelemetryConfig telemetry;
    SecurityConfig security;
    std::vector<RadioProfile> radios;
};

class ConfigManager {
public:
    explicit ConfigManager(std::filesystem::path path);

    const Configuration& get() const noexcept { return config_; }
    const std::filesystem::path& path() const noexcept { return path_; }

private:
    std::filesystem::path path_;
    Configuration config_;

    static Configuration load_from_file(const std::filesystem::path& path);
};

}  // namespace rcc::config

#pragma once

#include "rcc/config/types.hpp"

#include <filesystem>
#include <mutex>

namespace rcc::config {

class ConfigManager {
public:
    explicit ConfigManager(std::filesystem::path path);

    const Config& current() const;
    void reload();

private:
    Config loadFromFile(const std::filesystem::path& path) const;

    std::filesystem::path path_;
    Config config_;
    mutable std::mutex mutex_;
};

}  // namespace rcc::config

#pragma once

#include <chrono>
#include <cstdint>
#include <filesystem>
#include <string>
#include <string_view>
#include <vector>

namespace rcc::config {

struct NetworkConfig {
    std::string listenAddress{"0.0.0.0"};
    std::uint16_t port{8002};
};

struct AuthConfig {
    bool required{true};
    std::string jwtSecret{"dev-secret"};
    std::string issuer{};
    std::string audience{};
};

struct TelemetryConfig {
    std::size_t eventBufferSize{512};
    std::chrono::hours eventRetention{std::chrono::hours{1}};
    std::size_t maxSseClients{8};
    std::chrono::seconds clientIdleTimeout{std::chrono::seconds{60}};
};

struct ChannelEntry {
    int index{1};
    double frequencyMHz{0.0};
    std::string description;
};

struct RadioConfig {
    std::string id;
    std::string model;
    std::vector<ChannelEntry> channels;
};

struct ContainerConfig {
    std::string containerId{"radio-control-container"};
    std::string deployment{"development"};
    NetworkConfig network{};
    AuthConfig auth{};
    TelemetryConfig telemetry{};
    std::vector<RadioConfig> radios{};
};

class ConfigManager {
public:
    explicit ConfigManager(std::string_view path);

    const ContainerConfig& get() const noexcept;
    const std::filesystem::path& path() const noexcept;

private:
    ContainerConfig loadFromFile(const std::filesystem::path& path) const;

    ContainerConfig config_{};
    std::filesystem::path path_;
};

}  // namespace rcc::config



