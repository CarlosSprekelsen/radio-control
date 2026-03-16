// Global test setup/teardown (mirrors gpsd-proxy test_globals.cpp).
// Add any process-wide GTest event listeners here if needed.
#include <gtest/gtest.h>

int main(int argc, char* argv[]) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
