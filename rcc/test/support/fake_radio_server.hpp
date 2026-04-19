#pragma once
// Simulates a Silvus radio's HTTP endpoint for integration tests.
// Mirrors the fake_gnss_source_server pattern from gpsd-Container-proxy.

#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>

#include <atomic>
#include <functional>
#include <nlohmann/json.hpp>
#include <string>
#include <thread>

namespace rcc::test {

struct RadioResponse {
    int         http_status{200};
    std::string body{R"({"status":"ok"})"};
};

// Minimal single-threaded HTTP stub that returns scripted responses.
// Not production quality – purely for unit/integration test isolation.
class FakeRadioServer {
public:
    explicit FakeRadioServer(uint16_t port);
    ~FakeRadioServer();

    FakeRadioServer(const FakeRadioServer&)            = delete;
    FakeRadioServer& operator=(const FakeRadioServer&) = delete;

    void setResponse(RadioResponse resp) { response_ = std::move(resp); response_handler_ = std::nullopt; }
    void setHandler(std::function<RadioResponse(const std::string&)> handler) {
        response_handler_ = std::move(handler);
    }
    uint16_t port() const noexcept { return port_; }
    int requestCount() const noexcept { return request_count_.load(); }

    void start();
    void stop();

private:
    void acceptLoop();

    uint16_t            port_;
    RadioResponse       response_;
    std::optional<std::function<RadioResponse(const std::string&)>> response_handler_;
    std::atomic<bool>   running_{false};
    std::atomic<int>    request_count_{0};
    std::atomic<int>    server_fd_{-1};
    std::thread         thread_;
};

}  // namespace rcc::test
