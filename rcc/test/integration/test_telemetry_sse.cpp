// Integration test: TelemetryHub starts an SSE server and publishes events.
// Mirrors test_sse_contract.cpp from gpsd-Container-proxy.

#include "rcc/config/types.hpp"
#include "rcc/telemetry/telemetry_hub.hpp"
#include "support/test_utils.hpp"

#include <gtest/gtest.h>
#include <asio.hpp>
#include <nlohmann/json.hpp>

#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>

#include <atomic>
#include <chrono>
#include <string>
#include <thread>

namespace {

rcc::config::Config makeTestConfig(uint16_t sse_port) {
    rcc::config::Config cfg;
    cfg.container.container_id = "test-rcc";
    cfg.container.deployment   = "ci";
    cfg.network.bind_address   = "127.0.0.1";
    cfg.network.command_port   = static_cast<uint16_t>(sse_port + 100);
    cfg.telemetry.sse_port     = sse_port;
    cfg.telemetry.event_buffer_size    = 64;
    cfg.telemetry.event_retention      = std::chrono::hours{1};
    cfg.telemetry.max_sse_clients      = 4;
    cfg.telemetry.client_idle_timeout  = std::chrono::seconds{10};
    cfg.security.token_secret          = "";  // no auth for test
    return cfg;
}

// Read SSE lines from fd until we find one with the event type we want,
// or until timeout.
bool readSseEvent(int fd, const std::string& expected_event,
                  std::chrono::milliseconds timeout = std::chrono::milliseconds{3000}) {
    // Skip HTTP headers
    char buf[4096] = {};
    std::string acc;
    const auto deadline = std::chrono::steady_clock::now() + timeout;

    while (std::chrono::steady_clock::now() < deadline) {
        timeval tv{0, 50'000};
        ::setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
        const ssize_t n = ::recv(fd, buf, sizeof(buf) - 1, 0);
        if (n > 0) acc.append(buf, static_cast<size_t>(n));
        if (acc.find(expected_event) != std::string::npos) return true;
    }
    return false;
}

}  // namespace

TEST(TelemetrySSE, ServerStartsAndAcceptsConnection) {
    const uint16_t port = rcc::test::find_free_port();
    asio::io_context io{1};
    auto work = asio::make_work_guard(io);
    std::thread io_thread([&io] { io.run(); });

    const auto cfg = makeTestConfig(port);
    rcc::telemetry::TelemetryHub hub(io, cfg);
    hub.start();

    // Give the SSE server a moment to bind
    ASSERT_TRUE(rcc::test::wait_for([&] { return true; },
                std::chrono::milliseconds{200}));

    // Connect a raw TCP client
    int fd = ::socket(AF_INET, SOCK_STREAM, 0);
    ASSERT_GE(fd, 0);

    sockaddr_in addr{};
    addr.sin_family      = AF_INET;
    addr.sin_port        = htons(port);
    addr.sin_addr.s_addr = inet_addr("127.0.0.1");

    ASSERT_EQ(::connect(fd, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)), 0)
        << "Could not connect to SSE server on port " << port;

    // Send a minimal SSE GET request
    const std::string req =
        "GET /api/v1/telemetry HTTP/1.1\r\n"
        "Host: 127.0.0.1\r\n"
        "Accept: text/event-stream\r\n"
        "Connection: keep-alive\r\n\r\n";
    ::send(fd, req.data(), req.size(), MSG_NOSIGNAL);

    // We should receive an HTTP 200 and at least one SSE heartbeat or event
    char buf[1024] = {};
    timeval tv{2, 0};
    ::setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
    const ssize_t n = ::recv(fd, buf, sizeof(buf) - 1, 0);
    EXPECT_GT(n, 0) << "Expected data from SSE server";
    EXPECT_NE(std::string(buf, n).find("HTTP/1.1"), std::string::npos)
        << "Expected HTTP response, got: " << std::string(buf, n);

    ::close(fd);
    hub.stop();
    work.reset();
    io.stop();
    io_thread.join();
}

TEST(TelemetrySSE, PublishEventReachesClient) {
    const uint16_t port = rcc::test::find_free_port();
    asio::io_context io{1};
    auto work = asio::make_work_guard(io);
    std::thread io_thread([&io] { io.run(); });

    const auto cfg = makeTestConfig(port);
    rcc::telemetry::TelemetryHub hub(io, cfg);
    hub.start();

    std::this_thread::sleep_for(std::chrono::milliseconds{100});

    // Connect and send request
    int fd = ::socket(AF_INET, SOCK_STREAM, 0);
    ASSERT_GE(fd, 0);
    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port   = htons(port);
    addr.sin_addr.s_addr = inet_addr("127.0.0.1");
    ASSERT_EQ(::connect(fd, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)), 0);

    const std::string req =
        "GET /api/v1/telemetry HTTP/1.1\r\nHost: 127.0.0.1\r\n"
        "Accept: text/event-stream\r\nConnection: keep-alive\r\n\r\n";
    ::send(fd, req.data(), req.size(), MSG_NOSIGNAL);

    // Give server time to accept
    std::this_thread::sleep_for(std::chrono::milliseconds{100});

    // Publish an event
    hub.publishPowerChanged("radio-1", 1.5);

    // Read the SSE stream and look for the event
    const bool found = readSseEvent(fd, "rcc.radio.power");
    EXPECT_TRUE(found) << "Did not receive rcc.radio.power event within timeout";

    ::close(fd);
    hub.stop();
    work.reset();
    io.stop();
    io_thread.join();
}
