#include "rcc/common/types.hpp"
#include <gtest/gtest.h>

TEST(RadioStatus, ToStringRoundTrip) {
    using rcc::common::RadioStatus;
    using rcc::common::to_string;

    EXPECT_EQ(to_string(RadioStatus::Offline),     "offline");
    EXPECT_EQ(to_string(RadioStatus::Discovering), "discovering");
    EXPECT_EQ(to_string(RadioStatus::Ready),       "ready");
    EXPECT_EQ(to_string(RadioStatus::Busy),        "busy");
    EXPECT_EQ(to_string(RadioStatus::Recovering),  "recovering");
}

TEST(CommandResultCode, ToStringRoundTrip) {
    using rcc::common::CommandResultCode;
    using rcc::common::to_string;

    EXPECT_EQ(to_string(CommandResultCode::Ok),            "ok");
    EXPECT_EQ(to_string(CommandResultCode::InvalidRange),  "invalid_range");
    EXPECT_EQ(to_string(CommandResultCode::Busy),          "busy");
    EXPECT_EQ(to_string(CommandResultCode::Unavailable),   "unavailable");
    EXPECT_EQ(to_string(CommandResultCode::InternalError), "internal");
}

TEST(RadioState, DefaultIsOffline) {
    rcc::common::RadioState s;
    EXPECT_EQ(s.status, rcc::common::RadioStatus::Offline);
    EXPECT_FALSE(s.channel_index.has_value());
    EXPECT_FALSE(s.power_watts.has_value());
}
