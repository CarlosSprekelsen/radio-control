#include "silvus_mock.hpp"

#include <cassert>
#include <string>
#include <utility>

namespace {

nlohmann::json call(SilvusMock& mock, const std::string& method, nlohmann::json params = nullptr) {
    nlohmann::json request = {
        {"jsonrpc", "2.0"},
        {"method", method},
        {"id", "test"}
    };
    if (!params.is_null()) {
        request["params"] = std::move(params);
    }
    return mock.handle_jsonrpc(request);
}

void expect_string_array_result(const nlohmann::json& response, size_t size) {
    assert(response.contains("result"));
    assert(response["result"].is_array());
    assert(response["result"].size() == size);
    for (const auto& item : response["result"]) {
        assert(item.is_string());
    }
}

}  // namespace

int main() {
    SilvusMock mock;

    expect_string_array_result(call(mock, "freq"), 1);
    expect_string_array_result(call(mock, "power_dBm"), 1);
    expect_string_array_result(call(mock, "enable_max_power"), 1);
    expect_string_array_result(call(mock, "read_power_dBm"), 1);
    expect_string_array_result(call(mock, "read_power_mw"), 1);
    expect_string_array_result(call(mock, "max_link_distance"), 1);
    expect_string_array_result(call(mock, "gps_coordinates"), 3);
    expect_string_array_result(call(mock, "gps_mode"), 1);
    expect_string_array_result(call(mock, "gps_time"), 1);

    auto profiles = call(mock, "supported_frequency_profiles");
    assert(profiles.contains("result"));
    assert(profiles["result"].is_array());
    assert(profiles["result"][0].contains("frequencies"));
    assert(profiles["result"][0].contains("bandwidth"));
    assert(profiles["result"][0].contains("antenna_mask"));

    auto powerSet = call(mock, "power_dBm", nlohmann::json::array({"20"}));
    expect_string_array_result(powerSet, 1);
    assert(call(mock, "power_dBm")["result"][0] == "20");

    auto enableMaxPower = call(mock, "enable_max_power", nlohmann::json::array({"1"}));
    expect_string_array_result(enableMaxPower, 1);
    assert(call(mock, "enable_max_power")["result"][0] == "1");

    auto ignoredPowerSet = call(mock, "power_dBm", nlohmann::json::array({"10"}));
    expect_string_array_result(ignoredPowerSet, 1);
    assert(call(mock, "power_dBm")["result"][0] == "20");

    auto maxDistance = call(mock, "max_link_distance", nlohmann::json::array({"1000000"}));
    expect_string_array_result(maxDistance, 1);

    auto invalidDistance = call(mock, "max_link_distance", nlohmann::json::array({"1000001"}));
    assert(invalidDistance.contains("error"));

    auto gpsSet = call(mock, "gps_coordinates", nlohmann::json::array({"34.057", "-118.447", "0"}));
    expect_string_array_result(gpsSet, 1);
    auto gpsRead = call(mock, "gps_coordinates");
    assert(gpsRead["result"][0] == "34.057");
    assert(gpsRead["result"][1] == "-118.447");
    assert(gpsRead["result"][2] == "0");

    assert(call(mock, "gps_mode", nlohmann::json::array({"locked-3d"})).contains("error"));
    assert(call(mock, "gps_time", nlohmann::json::array({"4561387.1654"})).contains("error"));
    assert(call(mock, "supported_frequency_profiles", nlohmann::json::array({"x"})).contains("error"));
}
