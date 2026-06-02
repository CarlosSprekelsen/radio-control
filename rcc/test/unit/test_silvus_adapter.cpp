#include "rcc/adapter/silvus_adapter.hpp"
#include "rcc/common/types.hpp"
#include "support/fake_radio_server.hpp"
#include "support/test_utils.hpp"
#include <gtest/gtest.h>
#include <asio.hpp>
#include <nlohmann/json.hpp>

#include <algorithm>
#include <optional>
#include <utility>

namespace {

std::string makeSupportedFrequenciesResponse() {
    nlohmann::json response = {
        {"jsonrpc", "2.0"},
        {"id", "1"},
        {"result", {{{"frequencies", {"2412", "2437", "2462"}}}}}
    };
    return response.dump();
}

std::string makeSilvusProfileResponse() {
    nlohmann::json response = {
        {"jsonrpc", "2.0"},
        {"id", "1"},
        {"result", {
            {{"antenna_mask", "15"}, {"bandwidth", "-1"}, {"frequencies", {"2200:20:2380", "4700"}}},
            {{"antenna_mask", "3"}, {"bandwidth", "-1"}, {"frequencies", {"4420:40:4700"}}},
            {{"antenna_mask", "12"}, {"bandwidth", "-1"}, {"frequencies", {"4700:20:4980"}}}
        }}
    };
    return response.dump();
}

std::string makeOkResponse() {
    return R"({"jsonrpc":"2.0","id":"1","result":[""]})";
}

std::string makePowerResponse(int dbm) {
    nlohmann::json response = {
        {"jsonrpc", "2.0"},
        {"id", "1"},
        {"result", {std::to_string(dbm)}}
    };
    return response.dump();
}

std::string makeFrequencyResponse(double mhz) {
    nlohmann::json response = {
        {"jsonrpc", "2.0"},
        {"id", "2"},
        {"result", {std::to_string(mhz)}}
    };
    return response.dump();
}

std::string makeEnableMaxPowerResponse(bool enabled) {
    nlohmann::json response = {
        {"jsonrpc", "2.0"},
        {"id", "enable_max_power"},
        {"result", {enabled ? "1" : "0"}}
    };
    return response.dump();
}

std::function<rcc::test::RadioResponse(const std::string&)> makeDefaultRadioHandler() {
    return [](const std::string& request) {
        if (request.find("\"supported_frequency_profiles\"") != std::string::npos) {
            return rcc::test::RadioResponse{200, makeSupportedFrequenciesResponse()};
        }
        if (request.find("\"enable_max_power\"") != std::string::npos) {
            return rcc::test::RadioResponse{200, makeEnableMaxPowerResponse(false)};
        }
        if (request.find("\"power_dBm\"") != std::string::npos) {
            if (request.find("\"params\"") != std::string::npos) {
                return rcc::test::RadioResponse{200, makeOkResponse()};
            }
            return rcc::test::RadioResponse{200, makePowerResponse(18)};
        }
        if (request.find("\"freq\"") != std::string::npos) {
            if (request.find("\"params\"") != std::string::npos) {
                return rcc::test::RadioResponse{200, makeOkResponse()};
            }
            return rcc::test::RadioResponse{200, makeFrequencyResponse(2462.0)};
        }
        return rcc::test::RadioResponse{200, makeOkResponse()};
    };
}

std::string makeEndpoint(uint16_t port) {
    return "http://127.0.0.1:" + std::to_string(port) + "/streamscape_api";
}

nlohmann::json requestJsonFromHttp(const std::string& request) {
    const auto split = request.find("\r\n\r\n");
    if (split == std::string::npos) {
        return nlohmann::json::object();
    }
    return nlohmann::json::parse(request.substr(split + 4));
}

}  // namespace

TEST(SilvusAdapter, InitialStateIsOffline) {
    rcc::adapter::SilvusAdapter adapter("radio-1", "http://127.0.0.1:19999");
    EXPECT_EQ(adapter.state().status, rcc::common::RadioStatus::Offline);
    EXPECT_EQ(adapter.id(), "radio-1");
}

TEST(SilvusAdapter, ConnectTransitionsToReady) {
    const uint16_t port = rcc::test::find_free_port();
    rcc::test::FakeRadioServer server(port);
    server.setHandler(makeDefaultRadioHandler());
    server.start();

    rcc::adapter::SilvusAdapter adapter("radio-1", makeEndpoint(port));
    const auto res = adapter.connect();
    EXPECT_EQ(res.code, rcc::common::CommandResultCode::Ok);
    EXPECT_EQ(adapter.state().status, rcc::common::RadioStatus::Ready);

    server.stop();
}

TEST(SilvusAdapter, SetPowerUpdatesState) {
    const uint16_t port = rcc::test::find_free_port();
    rcc::test::FakeRadioServer server(port);
    server.setHandler(makeDefaultRadioHandler());
    server.start();

    rcc::adapter::SilvusAdapter adapter("radio-1", makeEndpoint(port));
    ASSERT_EQ(adapter.connect().code, rcc::common::CommandResultCode::Ok);

    const auto res = adapter.set_power(2.5);
    EXPECT_EQ(res.code, rcc::common::CommandResultCode::Ok);
    ASSERT_TRUE(adapter.state().power_watts.has_value());
    EXPECT_DOUBLE_EQ(*adapter.state().power_watts, 2.5);

    server.stop();
}

TEST(SilvusAdapter, SetChannelUpdatesState) {
    const uint16_t port = rcc::test::find_free_port();
    rcc::test::FakeRadioServer server(port);
    server.setHandler(makeDefaultRadioHandler());
    server.start();

    rcc::adapter::SilvusAdapter adapter("radio-1", makeEndpoint(port));
    ASSERT_EQ(adapter.connect().code, rcc::common::CommandResultCode::Ok);

    const auto res = adapter.set_channel(3, 2462.0);
    EXPECT_EQ(res.code, rcc::common::CommandResultCode::Ok);
    ASSERT_TRUE(adapter.state().channel_index.has_value());
    EXPECT_EQ(*adapter.state().channel_index, 3);

    server.stop();
}

TEST(SilvusAdapter, CapabilitiesHavePowerRange) {
    rcc::adapter::SilvusAdapter adapter("radio-1", "http://127.0.0.1:19999");
    const auto caps = adapter.capabilities();
    EXPECT_GT(caps.power_range_watts.second, caps.power_range_watts.first);
    EXPECT_FALSE(caps.supported_frequencies_mhz.empty());
}

TEST(SilvusAdapter, ConfiguredPowerRangeOverridesVendorDefault) {
    rcc::adapter::SilvusAdapter adapter("radio-1",
                                        "http://127.0.0.1:19999",
                                        std::optional<std::pair<double, double>>{
                                            std::make_pair(0.0, 36.0)});
    const auto caps = adapter.capabilities();
    EXPECT_EQ(caps.power_range_source, "configured");
    EXPECT_NEAR(caps.power_range_watts.second, 3.9810717055, 1e-9);
}

TEST(SilvusAdapter, ConnectExpandsSilvusFrequencyProfileRanges) {
    const uint16_t port = rcc::test::find_free_port();
    rcc::test::FakeRadioServer server(port);
    server.setHandler([](const std::string& request) {
        if (request.find("\"supported_frequency_profiles\"") != std::string::npos) {
            return rcc::test::RadioResponse{200, makeSilvusProfileResponse()};
        }
        if (request.find("\"enable_max_power\"") != std::string::npos) {
            return rcc::test::RadioResponse{200, makeEnableMaxPowerResponse(false)};
        }
        return rcc::test::RadioResponse{200, makeOkResponse()};
    });
    server.start();

    rcc::adapter::SilvusAdapter adapter("radio-1", makeEndpoint(port));
    ASSERT_EQ(adapter.connect().code, rcc::common::CommandResultCode::Ok);

    const auto freqs = adapter.capabilities().supported_frequencies_mhz;
    EXPECT_EQ(freqs.size(), 32U);
    EXPECT_NE(std::find(freqs.begin(), freqs.end(), 2220.0), freqs.end());
    EXPECT_NE(std::find(freqs.begin(), freqs.end(), 4460.0), freqs.end());
    EXPECT_NE(std::find(freqs.begin(), freqs.end(), 4960.0), freqs.end());

    server.stop();
}

TEST(SilvusAdapter, SetChannelSendsCompactStringFrequencyParam) {
    const uint16_t port = rcc::test::find_free_port();
    nlohmann::json freqRequest;
    rcc::test::FakeRadioServer server(port);
    server.setHandler([&](const std::string& request) {
        if (request.find("\"supported_frequency_profiles\"") != std::string::npos) {
            return rcc::test::RadioResponse{200, makeSilvusProfileResponse()};
        }
        if (request.find("\"enable_max_power\"") != std::string::npos) {
            return rcc::test::RadioResponse{200, makeEnableMaxPowerResponse(false)};
        }
        if (request.find("\"freq\"") != std::string::npos &&
            request.find("\"params\"") != std::string::npos) {
            freqRequest = requestJsonFromHttp(request);
            return rcc::test::RadioResponse{200, makeOkResponse()};
        }
        return rcc::test::RadioResponse{200, makeOkResponse()};
    });
    server.start();

    rcc::adapter::SilvusAdapter adapter("radio-1", makeEndpoint(port));
    ASSERT_EQ(adapter.connect().code, rcc::common::CommandResultCode::Ok);

    const auto res = adapter.set_channel(1, 2200.0);
    EXPECT_EQ(res.code, rcc::common::CommandResultCode::Ok);
    ASSERT_TRUE(freqRequest.contains("params"));
    EXPECT_EQ(freqRequest["params"][0], "2200");

    server.stop();
}

TEST(SilvusAdapter, ConnectReportsMaxPowerModeDisablesManualPowerControl) {
    const uint16_t port = rcc::test::find_free_port();
    rcc::test::FakeRadioServer server(port);
    server.setHandler([](const std::string& request) {
        if (request.find("\"supported_frequency_profiles\"") != std::string::npos) {
            return rcc::test::RadioResponse{200, makeSupportedFrequenciesResponse()};
        }
        if (request.find("\"enable_max_power\"") != std::string::npos) {
            return rcc::test::RadioResponse{200, makeEnableMaxPowerResponse(true)};
        }
        return rcc::test::RadioResponse{200, makeOkResponse()};
    });
    server.start();

    rcc::adapter::SilvusAdapter adapter("radio-1", makeEndpoint(port));
    ASSERT_EQ(adapter.connect().code, rcc::common::CommandResultCode::Ok);

    const auto caps = adapter.capabilities();
    ASSERT_TRUE(caps.enable_max_power.has_value());
    EXPECT_TRUE(*caps.enable_max_power);
    EXPECT_FALSE(caps.manual_power_control_available);

    server.stop();
}

TEST(SilvusAdapter, SetPowerReturnsBusyWhenMaxPowerModeOwnsOutput) {
    const uint16_t port = rcc::test::find_free_port();
    bool sawPowerSet = false;
    rcc::test::FakeRadioServer server(port);
    server.setHandler([&](const std::string& request) {
        if (request.find("\"supported_frequency_profiles\"") != std::string::npos) {
            return rcc::test::RadioResponse{200, makeSupportedFrequenciesResponse()};
        }
        if (request.find("\"enable_max_power\"") != std::string::npos) {
            return rcc::test::RadioResponse{200, makeEnableMaxPowerResponse(true)};
        }
        if (request.find("\"power_dBm\"") != std::string::npos &&
            request.find("\"params\"") != std::string::npos) {
            sawPowerSet = true;
        }
        return rcc::test::RadioResponse{200, makeOkResponse()};
    });
    server.start();

    rcc::adapter::SilvusAdapter adapter("radio-1", makeEndpoint(port));
    ASSERT_EQ(adapter.connect().code, rcc::common::CommandResultCode::Ok);

    const auto res = adapter.set_power(1.0);
    EXPECT_EQ(res.code, rcc::common::CommandResultCode::Busy);
    EXPECT_FALSE(sawPowerSet);

    server.stop();
}

TEST(SilvusAdapter, RefreshStateKeepsReady) {
    const uint16_t port = rcc::test::find_free_port();
    rcc::test::FakeRadioServer server(port);
    server.setHandler(makeDefaultRadioHandler());
    server.start();

    rcc::adapter::SilvusAdapter adapter("radio-1", makeEndpoint(port));
    ASSERT_EQ(adapter.connect().code, rcc::common::CommandResultCode::Ok);

    const auto res = adapter.refresh_state();
    EXPECT_EQ(res.code, rcc::common::CommandResultCode::Ok);
    EXPECT_EQ(adapter.state().status, rcc::common::RadioStatus::Ready);

    server.stop();
}
