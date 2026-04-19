// Integration test: RadioManager loads adapters from config and manages state.

#include "rcc/config/types.hpp"
#include "rcc/radio/radio_manager.hpp"
#include "rcc/common/types.hpp"
#include "support/test_utils.hpp"

#include <gtest/gtest.h>
#include <asio/io_context.hpp>

namespace {

rcc::config::Config makeConfig(const std::string& endpoint = "http://127.0.0.1:19999") {
    rcc::config::Config cfg;
    cfg.security.token_secret = "s";
    rcc::config::RadioEntry radio;
    radio.id       = "silvus-1";
    radio.adapter  = "silvus";
    radio.endpoint = endpoint;
    cfg.radios.push_back(radio);
    return cfg;
}

}  // namespace

TEST(RadioManager, LoadsRadioFromConfig) {
    asio::io_context io{1};
    const auto cfg = makeConfig();
    rcc::radio::RadioManager mgr(io, cfg);

    const auto radios = mgr.list_radios();
    ASSERT_EQ(radios.size(), 1u);
    EXPECT_EQ(radios[0].id, "silvus-1");
    EXPECT_EQ(radios[0].adapter_type, "silvus");
}

TEST(RadioManager, NoActiveRadioInitially) {
    asio::io_context io{1};
    rcc::radio::RadioManager mgr(io, makeConfig());
    EXPECT_FALSE(mgr.active_radio().has_value());
}

TEST(RadioManager, SetActiveRadioSucceeds) {
    asio::io_context io{1};
    rcc::radio::RadioManager mgr(io, makeConfig());
    EXPECT_TRUE(mgr.set_active_radio("silvus-1"));
    ASSERT_TRUE(mgr.active_radio().has_value());
    EXPECT_EQ(*mgr.active_radio(), "silvus-1");
}

TEST(RadioManager, SetUnknownRadioReturnsFalse) {
    asio::io_context io{1};
    rcc::radio::RadioManager mgr(io, makeConfig());
    EXPECT_FALSE(mgr.set_active_radio("does-not-exist"));
}

TEST(RadioManager, GetAdapterReturnsNullForUnknown) {
    asio::io_context io{1};
    rcc::radio::RadioManager mgr(io, makeConfig());
    EXPECT_EQ(mgr.get_adapter("no-such-radio"), nullptr);
}

TEST(RadioManager, GetAdapterReturnsValidForKnown) {
    asio::io_context io{1};
    rcc::radio::RadioManager mgr(io, makeConfig());
    const auto adapter = mgr.get_adapter("silvus-1");
    EXPECT_NE(adapter, nullptr);
    EXPECT_EQ(adapter->id(), "silvus-1");
}

TEST(RadioManager, StopPreservesRadioCatalog) {
    asio::io_context io{1};
    rcc::radio::RadioManager mgr(io, makeConfig());
    mgr.set_active_radio("silvus-1");

    mgr.stop();

    EXPECT_FALSE(mgr.active_radio().has_value());
    ASSERT_EQ(mgr.list_radios().size(), 1u);
    EXPECT_NE(mgr.get_adapter("silvus-1"), nullptr);
}

TEST(RadioManager, EmptyConfigHasNoRadios) {
    asio::io_context io{1};
    rcc::config::Config cfg;
    cfg.security.token_secret = "s";
    rcc::radio::RadioManager mgr(io, cfg);
    EXPECT_TRUE(mgr.list_radios().empty());
}
