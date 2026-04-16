#include "silvus_mock.hpp"

#include <cmath>
#include <iostream>
#include <utility>

using json = nlohmann::json;

SilvusMock::SilvusMock()
    : current_frequency_("4700.0"),
      current_power_dBm_(30),
      max_link_distance_m_(10000),
      gps_lat_("0.0"),
      gps_lon_("0.0"),
      gps_alt_("0.0"),
      gps_mode_("enabled"),
      gps_time_(std::to_string(std::chrono::system_clock::to_time_t(std::chrono::system_clock::now()))),
      blackout_until_(std::chrono::steady_clock::time_point()),
      soft_boot_duration_(30),
      power_change_duration_(5),
      radio_reset_duration_(60) {
    profiles_ = {
        {
            {"frequencies", {"2200:20:2380", "4700"}},
            {"bandwidth", "-1"},
            {"antenna_mask", "15"}
        },
        {
            {"frequencies", {"4420:40:4700"}},
            {"bandwidth", "-1"},
            {"antenna_mask", "3"}
        },
        {
            {"frequencies", {"4700:20:4980"}},
            {"bandwidth", "-1"},
            {"antenna_mask", "12"}
        }
    };
}

static bool almost_equal(double a, double b) {
    return std::fabs(a - b) < 1e-6;
}

bool SilvusMock::is_available() const {
    return std::chrono::steady_clock::now() >= blackout_until_;
}

bool SilvusMock::validate_frequency(const std::string& freqStr) const {
    double freq;
    try {
        freq = std::stod(freqStr);
    } catch (...) {
        return false;
    }

    for (const auto& profile : profiles_) {
        for (const auto& freqItem : profile["frequencies"]) {
            if (frequency_matches_profile(freq, freqItem.get<std::string>())) {
                return true;
            }
        }
    }
    return false;
}

bool SilvusMock::frequency_matches_profile(double freq, const std::string& profile) const {
    if (profile.find(':') == std::string::npos) {
        double value;
        try {
            value = std::stod(profile);
        } catch (...) {
            return false;
        }
        return almost_equal(freq, value);
    }

    auto parts = std::vector<std::string>{};
    size_t start = 0;
    for (size_t pos = 0; pos < profile.size(); ++pos) {
        if (profile[pos] == ':') {
            parts.push_back(profile.substr(start, pos - start));
            start = pos + 1;
        }
    }
    parts.push_back(profile.substr(start));
    if (parts.size() != 3) {
        return false;
    }

    double low;
    double step;
    double high;
    try {
        low = std::stod(parts[0]);
        step = std::stod(parts[1]);
        high = std::stod(parts[2]);
    } catch (...) {
        return false;
    }
    if (step <= 0.0 || high < low) {
        return false;
    }
    if (freq < low - 1e-6 || freq > high + 1e-6) {
        return false;
    }

    double delta = freq - low;
    double steps = std::round(delta / step);
    double candidate = low + steps * step;
    return almost_equal(candidate, freq);
}

double SilvusMock::to_mw(int dbm) {
    return std::pow(10.0, static_cast<double>(dbm) / 10.0);
}

json SilvusMock::make_error(int code, const std::string& message, const json& id) const {
    json error = json{{"code", code}, {"message", message}};
    return json{{"jsonrpc", "2.0"}, {"error", error}, {"id", id}};
}

json SilvusMock::make_error(int code, const std::string& message, const std::string& data, const json& id) const {
    json error = json{{"code", code}, {"message", message}};
    if (!data.empty()) {
        error["data"] = data;
    }
    return json{{"jsonrpc", "2.0"}, {"error", error}, {"id", id}};
}

json SilvusMock::make_result(const json& result, const json& id) const {
    return json{
        {"jsonrpc", "2.0"},
        {"result", result},
        {"id", id}
    };
}

json SilvusMock::handle_jsonrpc(const json& req) {
    std::cout << "[silvus-mock] handle_jsonrpc item: " << req.dump() << std::endl;
    if (!req.is_object() || req.value("jsonrpc", "") != "2.0" || !req.contains("method")) {
        return make_error(-32600, "Invalid Request", req.value("id", nullptr));
    }

    json id = json(nullptr);
    if (req.contains("id")) {
        id = req["id"];
    }

    try {
        auto methodNode = req["method"];
        std::cout << "[silvus-mock] method node type=" << methodNode.type_name() << " dump=" << methodNode.dump() << std::endl;
        auto method = methodNode.get<std::string>();
        json params = json::array();
        if (req.contains("params")) {
            if (req["params"].is_array()) {
                params = req["params"];
            } else {
                params = json::array({req["params"]});
            }
        }
        std::cout << "[silvus-mock] handle_jsonrpc method=" << method << " params=" << params.dump() << std::endl;
        std::cout << "[silvus-mock] handle_jsonrpc before lock" << std::endl;
        std::lock_guard lock(mutex_);
        std::cout << "[silvus-mock] handle_jsonrpc after lock" << std::endl;
        std::cout << "[silvus-mock] handle_jsonrpc availability=" << is_available() << std::endl;
        bool skip = (method != "max_link_distance" && method != "read_power_dBm" && method != "read_power_mw");
        std::cout << "[silvus-mock] handle_jsonrpc skip=" << skip << std::endl;
        if (!is_available() && skip) {
            std::cout << "[silvus-mock] handle_jsonrpc returning UNAVAILABLE" << std::endl;
            auto result = make_error(-32000, "UNAVAILABLE", id);
            std::cout << "[silvus-mock] UNAVAILABLE result=" << result.dump() << std::endl;
            return result;
        }

        if (method == "freq") {
            if (params.empty()) {
                return make_result(json::array({current_frequency_}), id);
            }
            std::string value = params[0].is_string() ? params[0].get<std::string>() : params[0].dump();
            if (!validate_frequency(value)) {
                return make_error(-32002, "INVALID_RANGE", id);
            }
            current_frequency_ = value;
            blackout_until_ = std::chrono::steady_clock::now() + soft_boot_duration_;
            return make_result(json::array({""}), id);
        }

        if (method == "power_dBm") {
            if (params.empty()) {
                return make_result(json::array({std::to_string(current_power_dBm_)}), id);
            }
            std::cout << "[silvus-mock] power_dBm params[0] type=" << params[0].type_name() << " dump=" << params[0].dump() << std::endl;
            int power;
            try {
                power = params[0].is_number() ? params[0].get<int>() : std::stoi(params[0].get<std::string>());
            } catch (...) {
                std::cout << "[silvus-mock] power_dBm invalid range parse failure" << std::endl;
                return make_error(-32002, "INVALID_RANGE", id);
            }
            std::cout << "[silvus-mock] power_dBm parsed power=" << power << std::endl;
            if (power < 0 || power > 39) {
                return make_error(-32002, "INVALID_RANGE", id);
            }
            current_power_dBm_ = power;
            blackout_until_ = std::chrono::steady_clock::now() + power_change_duration_;
            auto response = make_result(json::array({""}), id);
            std::cout << "[silvus-mock] power_dBm response=" << response.dump() << std::endl;
            return response;
        }

        if (method == "supported_frequency_profiles") {
            return make_result(profiles_, id);
        }

        if (method == "read_power_dBm") {
            return make_result(json::array({std::to_string(current_power_dBm_)}), id);
        }

        if (method == "read_power_mw") {
            return make_result(json::array({std::to_string(static_cast<int>(std::round(to_mw(current_power_dBm_))))}), id);
        }

        if (method == "max_link_distance") {
            if (params.empty()) {
                return make_result(json::array({std::to_string(max_link_distance_m_)}), id);
            }
            int value;
            try {
                value = params[0].is_number() ? params[0].get<int>() : std::stoi(params[0].get<std::string>());
            } catch (...) {
                return make_error(-32002, "INVALID_RANGE", "Invalid distance", id);
            }
            if (value < 0 || value > 100000) {
                return make_error(-32002, "INVALID_RANGE", "Distance out of range", id);
            }
            max_link_distance_m_ = value;
            return make_result(json::array({""}), id);
        }

        if (method == "gps_coordinates") {
            if (params.empty()) {
                return make_result(json{{"lat", gps_lat_}, {"lon", gps_lon_}, {"alt", gps_alt_}}, id);
            }
            if (params.size() != 3) {
                return make_error(-32602, "INVALID_RANGE", "gps_coordinates requires three parameters", id);
            }
            gps_lat_ = params[0].is_string() ? params[0].get<std::string>() : params[0].dump();
            gps_lon_ = params[1].is_string() ? params[1].get<std::string>() : params[1].dump();
            gps_alt_ = params[2].is_string() ? params[2].get<std::string>() : params[2].dump();
            return make_result(json::array({""}), id);
        }

        if (method == "gps_mode") {
            if (params.empty()) {
                return make_result(json{{"mode", gps_mode_}}, id);
            }
            gps_mode_ = params[0].is_string() ? params[0].get<std::string>() : params[0].dump();
            return make_result(json::array({""}), id);
        }

        if (method == "gps_time") {
            if (params.empty()) {
                return make_result(json::array({gps_time_}), id);
            }
            gps_time_ = params[0].is_string() ? params[0].get<std::string>() : params[0].dump();
            return make_result(json::array({""}), id);
        }

        if (method == "zeroize") {
            current_frequency_ = "2490.0";
            current_power_dBm_ = 30;
            blackout_until_ = std::chrono::steady_clock::time_point();
            return make_result(json::array({""}), id);
        }

        if (method == "radio_reset") {
            blackout_until_ = std::chrono::steady_clock::now() + radio_reset_duration_;
            return make_result(json::array({""}), id);
        }

        if (method == "factory_reset") {
            current_frequency_ = "2490.0";
            current_power_dBm_ = 30;
            return make_result(json::array({""}), id);
        }

        return make_error(-32601, "Method not found", id);
    } catch (const json::exception& e) {
        std::cout << "[silvus-mock] handle_jsonrpc json exception: " << e.what() << std::endl;
        return make_error(-32603, std::string("Internal error: ") + e.what(), id);
    }
}
json SilvusMock::get_status() const {
    std::lock_guard lock(mutex_);
    auto remaining = std::chrono::duration_cast<std::chrono::seconds>(blackout_until_ - std::chrono::steady_clock::now()).count();
    if (remaining < 0) {
        remaining = 0;
    }
    return json{
        {"frequency", current_frequency_},
        {"power_dBm", current_power_dBm_},
        {"available", is_available()},
        {"blackoutUntil", remaining},
        {"max_link_distance_m", max_link_distance_m_},
        {"gps_coordinates", json{{"lat", gps_lat_}, {"lon", gps_lon_}, {"alt", gps_alt_}}},
        {"gps_mode", gps_mode_},
        {"gps_time", gps_time_},
        {"supported_frequency_profiles", profiles_}
    };
}
std::string SilvusMock::handle_jsonrpc_text(const std::string& payload) {
    std::cout << "[silvus-mock] handle_jsonrpc_text payload: " << payload << std::endl;
    try {
        auto req = json::parse(payload);
        std::cout << "[silvus-mock] handle_jsonrpc_text parsed type: " << (req.is_array() ? "array" : "object") << std::endl;
        if (req.is_array()) {
            json responses = json::array();
            for (const auto& item : req) {
                responses.push_back(handle_jsonrpc(item));
            }
            auto output = responses.dump();
            std::cout << "[silvus-mock] handle_jsonrpc_text batch response: " << output << std::endl;
            return output;
        }
        auto resp = handle_jsonrpc(req);
        auto output = resp.dump();
        std::cout << "[silvus-mock] handle_jsonrpc_text single response: " << output << std::endl;
        return output;
    } catch (const json::parse_error&) {
        auto err = make_error(-32700, "Parse error", nullptr);
        return err.dump();
    } catch (const json::exception& e) {
        auto err = make_error(-32603, std::string("Internal error: ") + e.what(), nullptr);
        std::cout << "[silvus-mock] handle_jsonrpc_text json exception: " << e.what() << std::endl;
        return err.dump();
    } catch (const std::exception& e) {
        auto err = make_error(-32603, std::string("Internal error: ") + e.what(), nullptr);
        std::cout << "[silvus-mock] handle_jsonrpc_text exception: " << e.what() << std::endl;
        return err.dump();
    } catch (...) {
        auto err = make_error(-32603, "Internal error", nullptr);
        std::cout << "[silvus-mock] handle_jsonrpc_text unknown exception" << std::endl;
        return err.dump();
    }
}
