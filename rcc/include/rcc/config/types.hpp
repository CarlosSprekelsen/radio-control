#pragma once

#include <string>
#include <string_view>
#include <vector>
#include <optional>
#include <chrono>

namespace rcc::config {

struct TelemetryConfig {
    uint16_t sse_port{0};
    std::chrono::seconds heartbeat_interval{std::chrono::seconds{30}};
    std::size_t event_buffer_size{256};
    std::chrono::hours event_retention{std::chrono::hours{24}};
};

struct NetworkConfig {
    std::string bind_address{"0.0.0.0"};
    uint16_t command_port{8080};
};

struct ContainerConfig {
    std::string container_id;
    std::string deployment;
    std::string soldier_id;
};

struct SecurityConfig {
    std::string token_secret;
    std::vector<std::string> allowed_roles;
    std::chrono::seconds token_ttl{std::chrono::seconds{300}};
};

struct RadioEntry {
    std::string id;
    std::string adapter;
    std::string endpoint;
    std::optional<std::string> description;
};

struct TimingProfile {
    std::chrono::seconds normal_probe{std::chrono::seconds{30}};
    std::chrono::seconds recovering_probe{std::chrono::seconds{10}};
    std::chrono::seconds offline_probe{std::chrono::seconds{60}};
};

struct Config {
    ContainerConfig container;
    NetworkConfig network;
    TelemetryConfig telemetry;
    SecurityConfig security;
    TimingProfile timing;
    std::vector<RadioEntry> radios;
};

}  // namespace rcc::config


