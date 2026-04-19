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

// Poll until a TCP connect to 127.0.0.1:port succeeds or timeout expires.
bool wait_for_port(uint16_t port,
                   std::chrono::milliseconds timeout = std::chrono::milliseconds{2000}) {
    return rcc::test::wait_for([port] {
        int tfd = ::socket(AF_INET, SOCK_STREAM, 0);
        if (tfd < 0) return false;
        sockaddr_in addr{};
        addr.sin_family      = AF_INET;
        addr.sin_port        = htons(port);
        addr.sin_addr.s_addr = inet_addr("127.0.0.1");
        bool ok = ::connect(tfd, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) == 0;
        ::close(tfd);
        return ok;
    }, timeout);
}

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
    cfg.security.token_secret          = "test-secret-key-16bytes";
    return cfg;
}

// Read SSE lines from fd until we find one with the event type we want,
// or until timeout.
bool readSseEvent(int fd, const std::string& expected_event,
                  std::chrono::milliseconds timeout = std::chrono::milliseconds{3000}) {
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

bool readSsePayload(int fd,
                    const std::string& expected_fragment,
                    std::string& payload,
                    std::chrono::milliseconds timeout = std::chrono::milliseconds{3000}) {
    char buf[4096] = {};
    payload.clear();
    const auto deadline = std::chrono::steady_clock::now() + timeout;

    while (std::chrono::steady_clock::now() < deadline) {
        timeval tv{0, 50'000};
        ::setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
        const ssize_t n = ::recv(fd, buf, sizeof(buf) - 1, 0);
        if (n > 0) {
            payload.append(buf, static_cast<size_t>(n));
            if (payload.find(expected_fragment) != std::string::npos) {
                return true;
            }
        }
    }
    return false;
}

int connectSseClient(uint16_t port) {
    int fd = ::socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0) {
        return -1;
    }

    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    addr.sin_addr.s_addr = inet_addr("127.0.0.1");
    if (::connect(fd, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) != 0) {
        ::close(fd);
        return -1;
    }

    const std::string jwt = rcc::test::createTestJWT("viewer");
    const std::string req =
        "GET /api/v1/telemetry HTTP/1.1\r\n"
        "Host: 127.0.0.1\r\n"
        "Accept: text/event-stream\r\n"
        "Authorization: Bearer " + jwt + "\r\n"
        "Connection: keep-alive\r\n\r\n";
    ::send(fd, req.data(), req.size(), MSG_NOSIGNAL);

    return fd;
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

    // Wait until the SSE server is actually accepting connections
    ASSERT_TRUE(wait_for_port(port)) << "SSE server did not start within 2s";

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
    const std::string jwt = rcc::test::createTestJWT("viewer");
    const std::string req =
        "GET /api/v1/telemetry HTTP/1.1\r\n"
        "Host: 127.0.0.1\r\n"
        "Accept: text/event-stream\r\n"
        "Authorization: Bearer " + jwt + "\r\n"
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

    // Wait until the SSE server is actually accepting connections
    ASSERT_TRUE(wait_for_port(port)) << "SSE server did not start within 2s";

    const int fd = connectSseClient(port);
    ASSERT_GE(fd, 0);

    // Give server time to accept
    std::this_thread::sleep_for(std::chrono::milliseconds{100});

    // Publish an event
    hub.publishPowerChanged("radio-1", 1.5);

    // Per SSE v1 contract, power updates are published as `event: powerChanged`.
    const bool found = readSseEvent(fd, "event: powerChanged");
    EXPECT_TRUE(found) << "Did not receive powerChanged event within timeout";

    ::close(fd);
    hub.stop();
    work.reset();
    io.stop();
    io_thread.join();
}

TEST(TelemetrySSE, HeartbeatEventReachesClient) {
    const uint16_t port = rcc::test::find_free_port();
    asio::io_context io{1};
    auto work = asio::make_work_guard(io);
    std::thread io_thread([&io] { io.run(); });

    auto cfg = makeTestConfig(port);
    cfg.telemetry.heartbeat_interval = std::chrono::seconds{1};
    rcc::telemetry::TelemetryHub hub(io, cfg);
    hub.start();

    ASSERT_TRUE(wait_for_port(port)) << "SSE server did not start within 2s";

    const int fd = connectSseClient(port);
    ASSERT_GE(fd, 0);

    EXPECT_TRUE(readSseEvent(fd, "event: heartbeat", std::chrono::milliseconds{2500}))
        << "Did not receive heartbeat event within timeout";

    ::close(fd);
    hub.stop();
    work.reset();
    io.stop();
    io_thread.join();
}

TEST(TelemetrySSE, FaultEventCarriesCodeAndRetryDetails) {
    const uint16_t port = rcc::test::find_free_port();
    asio::io_context io{1};
    auto work = asio::make_work_guard(io);
    std::thread io_thread([&io] { io.run(); });

    const auto cfg = makeTestConfig(port);
    rcc::telemetry::TelemetryHub hub(io, cfg);
    hub.start();

    ASSERT_TRUE(wait_for_port(port)) << "SSE server did not start within 2s";

    const int fd = connectSseClient(port);
    ASSERT_GE(fd, 0);
    std::this_thread::sleep_for(std::chrono::milliseconds{100});

    hub.publishFault("radio-1", "BUSY", "retry later", 1500);

    std::string payload;
    ASSERT_TRUE(readSsePayload(fd, "event: fault", payload))
        << "Did not receive fault event within timeout";
    EXPECT_NE(payload.find("\"code\":\"BUSY\""), std::string::npos);
    EXPECT_NE(payload.find("\"retryMs\":1500"), std::string::npos);

    ::close(fd);
    hub.stop();
    work.reset();
    io.stop();
    io_thread.join();
}

TEST(TelemetrySSE, StateEventNormalizesOnlineStatus) {
    const uint16_t port = rcc::test::find_free_port();
    asio::io_context io{1};
    auto work = asio::make_work_guard(io);
    std::thread io_thread([&io] { io.run(); });

    const auto cfg = makeTestConfig(port);
    rcc::telemetry::TelemetryHub hub(io, cfg);
    hub.start();

    ASSERT_TRUE(wait_for_port(port)) << "SSE server did not start within 2s";

    const int fd = connectSseClient(port);
    ASSERT_GE(fd, 0);
    std::this_thread::sleep_for(std::chrono::milliseconds{100});

    hub.publishRadioState("radio-1", "ready", -1, 0.0, 2412.0);

    std::string payload;
    ASSERT_TRUE(readSsePayload(fd, "event: state", payload))
        << "Did not receive state event within timeout";
    EXPECT_NE(payload.find("\"status\":\"online\""), std::string::npos);
    EXPECT_NE(payload.find("\"powerDbm\":null"), std::string::npos);

    ::close(fd);
    hub.stop();
    work.reset();
    io.stop();
    io_thread.join();
}
