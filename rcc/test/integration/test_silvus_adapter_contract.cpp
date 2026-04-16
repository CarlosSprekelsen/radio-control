#include "rcc/adapter/silvus_adapter.hpp"
#include "support/fake_radio_server.hpp"
#include "support/test_utils.hpp"

#include <gtest/gtest.h>
#include <asio.hpp>
#include <nlohmann/json.hpp>

#include <string>

namespace {

std::string makeSupportedFrequenciesResponse() {
    nlohmann::json response = {
        {"jsonrpc", "2.0"},
        {"id", "1"},
        {"result", {{{"frequencies", {"2412", "2437", "2462"}}}}}
    };
    return response.dump();
}

std::string makeOkResponse() {
    return R"({"jsonrpc":"2.0","id":"1","result":[]})";
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

}  // namespace

TEST(SilvusAdapterContract, ConnectSetPowerRefreshStateAgainstFakeRadio) {
    const uint16_t port = rcc::test::find_free_port();
    const std::string endpoint = "http://127.0.0.1:" + std::to_string(port) + "/streamscape_api";

    rcc::test::FakeRadioServer server(port);
    server.start();

    server.setHandler([&](const std::string& request) {
        if (request.find("\"supported_frequency_profiles\"") != std::string::npos) {
            return rcc::test::RadioResponse{200, makeSupportedFrequenciesResponse()};
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
    });

    rcc::adapter::SilvusAdapter adapter("radio-1", endpoint);

    const auto connectResult = adapter.connect();
    EXPECT_EQ(connectResult.code, rcc::common::CommandResultCode::Ok);
    EXPECT_EQ(adapter.state().status, rcc::common::RadioStatus::Ready);
    EXPECT_FALSE(adapter.capabilities().supported_frequencies_mhz.empty());

    const auto powerResult = adapter.set_power(1.0);
    EXPECT_EQ(powerResult.code, rcc::common::CommandResultCode::Ok);
    ASSERT_TRUE(adapter.state().power_watts.has_value());
    EXPECT_NEAR(*adapter.state().power_watts, 1.0, 1e-6);

    const auto channelResult = adapter.set_channel(3, 2462.0);
    EXPECT_EQ(channelResult.code, rcc::common::CommandResultCode::Ok);
    ASSERT_TRUE(adapter.state().channel_index.has_value());
    EXPECT_EQ(*adapter.state().channel_index, 3);

    const auto refreshResult = adapter.refresh_state();
    EXPECT_EQ(refreshResult.code, rcc::common::CommandResultCode::Ok);
    ASSERT_TRUE(adapter.state().power_watts.has_value());
    EXPECT_NEAR(*adapter.state().power_watts, 0.0631, 1e-4);

    const auto stateAfterRefresh = adapter.state();
    ASSERT_TRUE(stateAfterRefresh.channel_index.has_value());
    EXPECT_EQ(*stateAfterRefresh.channel_index, 3);

    server.stop();
}
