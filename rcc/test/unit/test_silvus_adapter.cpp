#include "rcc/adapter/silvus_adapter.hpp"
#include "rcc/common/types.hpp"
#include <gtest/gtest.h>

// Unit tests for SilvusAdapter — exercises state machine, no network.
// Use the integration test (test_silvus_adapter_contract.cpp) for
// real HTTP round-trips against FakeRadioServer.

TEST(SilvusAdapter, InitialStateIsOffline) {
    rcc::adapter::SilvusAdapter adapter("radio-1", "http://127.0.0.1:19999");
    EXPECT_EQ(adapter.state().status, rcc::common::RadioStatus::Offline);
    EXPECT_EQ(adapter.id(), "radio-1");
}

TEST(SilvusAdapter, ConnectTransitionsToReady) {
    rcc::adapter::SilvusAdapter adapter("radio-1", "http://127.0.0.1:19999");
    // Stub connect() does not do real HTTP — just sets state
    const auto res = adapter.connect();
    EXPECT_EQ(res.code, rcc::common::CommandResultCode::Ok);
    EXPECT_EQ(adapter.state().status, rcc::common::RadioStatus::Ready);
}

TEST(SilvusAdapter, SetPowerUpdatesState) {
    rcc::adapter::SilvusAdapter adapter("radio-1", "http://127.0.0.1:19999");
    adapter.connect();
    const auto res = adapter.set_power(2.5);
    EXPECT_EQ(res.code, rcc::common::CommandResultCode::Ok);
    ASSERT_TRUE(adapter.state().power_watts.has_value());
    EXPECT_DOUBLE_EQ(*adapter.state().power_watts, 2.5);
}

TEST(SilvusAdapter, SetChannelUpdatesState) {
    rcc::adapter::SilvusAdapter adapter("radio-1", "http://127.0.0.1:19999");
    adapter.connect();
    const auto res = adapter.set_channel(3, 2462.0);
    EXPECT_EQ(res.code, rcc::common::CommandResultCode::Ok);
    ASSERT_TRUE(adapter.state().channel_index.has_value());
    EXPECT_EQ(*adapter.state().channel_index, 3);
}

TEST(SilvusAdapter, CapabilitiesHavePowerRange) {
    rcc::adapter::SilvusAdapter adapter("radio-1", "http://127.0.0.1:19999");
    const auto caps = adapter.capabilities();
    EXPECT_GT(caps.power_range_watts.second, caps.power_range_watts.first);
    EXPECT_FALSE(caps.supported_frequencies_mhz.empty());
}

TEST(SilvusAdapter, RefreshStateKeepsReady) {
    rcc::adapter::SilvusAdapter adapter("radio-1", "http://127.0.0.1:19999");
    adapter.connect();
    const auto res = adapter.refresh_state();
    EXPECT_EQ(res.code, rcc::common::CommandResultCode::Ok);
    EXPECT_EQ(adapter.state().status, rcc::common::RadioStatus::Ready);
}
