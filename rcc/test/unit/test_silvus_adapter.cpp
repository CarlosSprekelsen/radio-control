#include "rcc/adapter/silvus_adapter.hpp"
#include "rcc/common/types.hpp"
#include "support/fake_radio_server.hpp"
#include "support/test_utils.hpp"
#include <gtest/gtest.h>
#include <asio.hpp>
#include <nlohmann/json.hpp>

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

std::function<rcc::test::RadioResponse(const std::string&)> makeDefaultRadioHandler() {
    return [](const std::string& request) {
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
    };
}

std::string makeEndpoint(uint16_t port) {
    return "http://127.0.0.1:" + std::to_string(port) + "/streamscape_api";
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
