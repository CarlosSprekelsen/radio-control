#include "rcc/api/monitor_page.hpp"

#include <gtest/gtest.h>

#include <string_view>

TEST(MonitorPage, UsesApiV1RoutesAndSseV1Events) {
    const std::string_view html = rcc::api::RADIO_MONITOR_HTML;

    EXPECT_NE(html.find("const API_BASE = \"/api/v1\";"), std::string_view::npos);
    EXPECT_NE(html.find("/radios/select"), std::string_view::npos);
    EXPECT_NE(html.find("/dev-token"), std::string_view::npos);
    EXPECT_NE(html.find("/api/v1/telemetry"), std::string_view::npos);
    EXPECT_NE(html.find("channelChanged"), std::string_view::npos);
    EXPECT_NE(html.find("powerChanged"), std::string_view::npos);
    EXPECT_NE(html.find("heartbeat"), std::string_view::npos);
    EXPECT_NE(html.find("Last-Event-ID"), std::string_view::npos);
    EXPECT_NE(html.find("localStorage"), std::string_view::npos);
}

TEST(MonitorPage, DoesNotReferenceLegacyRoutesOrTags) {
    const std::string_view html = rcc::api::RADIO_MONITOR_HTML;

    EXPECT_EQ(html.find("/api/v1/radio/connect"), std::string_view::npos);
    EXPECT_EQ(html.find("/api/v1/radio/channel"), std::string_view::npos);
    EXPECT_EQ(html.find("/api/v1/radio/power"), std::string_view::npos);
    EXPECT_EQ(html.find("rcc.radio.state"), std::string_view::npos);
    EXPECT_EQ(html.find("rcc.radio.channel"), std::string_view::npos);
    EXPECT_EQ(html.find("rcc.radio.power"), std::string_view::npos);
}
