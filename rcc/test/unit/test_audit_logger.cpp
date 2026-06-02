#include "rcc/audit/audit_logger.hpp"

#include <gtest/gtest.h>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>

namespace {

std::filesystem::path uniqueAuditPath() {
    const auto base = std::filesystem::temp_directory_path();
    return base / ("rcc-audit-" + std::to_string(::testing::UnitTest::GetInstance()->random_seed()) +
                   "-" + std::to_string(reinterpret_cast<std::uintptr_t>(&base)) + ".log");
}

std::string readFile(const std::filesystem::path& path) {
    std::ifstream input(path);
    std::ostringstream buffer;
    buffer << input.rdbuf();
    return buffer.str();
}

}  // namespace

TEST(AuditLogger, WritesThroughDtsCommonAuditLogger) {
    const auto path = uniqueAuditPath();
    std::filesystem::remove(path);

    rcc::config::AuditConfig config;
    config.file_path = path.string();
    config.rotate_after_bytes = 4096;
    config.rotated_file_count = 2;

    {
        rcc::audit::AuditLogger logger(config);
        logger.record({
            .actor = "operator-1",
            .action = "setPower",
            .radio_id = "silvus-001",
            .parameters = {{"powerWatts", 1.0}},
            .result = rcc::common::CommandResultCode::Ok,
            .message = "OK"
        });
    }

    ASSERT_TRUE(std::filesystem::exists(path));
    const auto content = readFile(path);
    EXPECT_NE(content.find("\"actor\":\"operator-1\""), std::string::npos);
    EXPECT_NE(content.find("\"operation\":\"setPower\""), std::string::npos);
    EXPECT_NE(content.find("\"radioId\":\"silvus-001\""), std::string::npos);
    EXPECT_NE(content.find("\"result\":\"ok\""), std::string::npos);

    std::filesystem::remove(path);
}
