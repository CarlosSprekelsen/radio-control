#pragma once

#include <chrono>
#include <mutex>
#include <string>
#include <vector>
#include <nlohmann/json.hpp>

class SilvusMock {
public:
    SilvusMock();
    nlohmann::json handle_jsonrpc(const nlohmann::json& req);
    std::string handle_jsonrpc_text(const std::string& payload);
    nlohmann::json get_status() const;

private:
    bool is_available() const;
    bool validate_frequency(const std::string& freqStr) const;
    bool frequency_matches_profile(double freq, const std::string& profile) const;
    static double to_mw(int dbm);

    nlohmann::json make_error(int code, const std::string& message, const nlohmann::json& id) const;
    nlohmann::json make_error(int code, const std::string& message, const std::string& data, const nlohmann::json& id) const;
    nlohmann::json make_result(const nlohmann::json& result, const nlohmann::json& id) const;

    mutable std::mutex mutex_;
    std::string current_frequency_;
    int current_power_dBm_;
    int max_link_distance_m_;
    std::string gps_lat_;
    std::string gps_lon_;
    std::string gps_alt_;
    std::string gps_mode_;
    std::string gps_time_;
    std::chrono::steady_clock::time_point blackout_until_;
    std::chrono::seconds soft_boot_duration_;
    std::chrono::seconds power_change_duration_;
    std::chrono::seconds radio_reset_duration_;
    std::vector<nlohmann::json> profiles_;
};
