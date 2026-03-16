#pragma once
// Shared test utilities for radio-control-container tests.
// Mirrors dts-common/test/test_utils.hpp from gpsd-Container-proxy.

#include <dts/common/security/jwt_validator.hpp>
#include <gtest/gtest.h>

#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>

#include <asio.hpp>
#include <chrono>
#include <condition_variable>
#include <functional>
#include <memory>
#include <mutex>
#include <nlohmann/json.hpp>
#include <openssl/evp.h>
#include <openssl/hmac.h>
#include <sstream>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>

namespace rcc::test {

// ─── Port utilities ───────────────────────────────────────────────────────

inline uint16_t find_free_port() {
    int fd = ::socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0) throw std::runtime_error("socket() failed finding free port");

    sockaddr_in addr{};
    addr.sin_family      = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    addr.sin_port        = 0;

    if (::bind(fd, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) < 0) {
        ::close(fd);
        throw std::runtime_error("bind() failed finding free port");
    }
    socklen_t len = sizeof(addr);
    ::getsockname(fd, reinterpret_cast<sockaddr*>(&addr), &len);
    const uint16_t port = ntohs(addr.sin_port);
    ::close(fd);
    return port;
}

// ─── JWT helpers (mirrors dts-common test_utils.hpp) ─────────────────────

inline std::string base64UrlEncode(const std::string& input) {
    static const char b64[] =
        "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    std::string out;
    size_t val = 0; int valb = -6;
    for (unsigned char c : input) {
        val = (val << 8) + c; valb += 8;
        while (valb >= 0) { out.push_back(b64[(val >> valb) & 0x3F]); valb -= 6; }
    }
    if (valb > -6) out.push_back(b64[((val << 8) >> (valb + 8)) & 0x3F]);
    while (out.size() % 4) out.push_back('=');
    for (char& ch : out) {
        if (ch == '+') ch = '-';
        else if (ch == '/') ch = '_';
    }
    while (!out.empty() && out.back() == '=') out.pop_back();
    return out;
}

inline std::string hmacSha256(const std::string& message, const std::string& secret) {
    unsigned int len = EVP_MAX_MD_SIZE;
    unsigned char hash[EVP_MAX_MD_SIZE];
    HMAC(EVP_sha256(), secret.data(), static_cast<int>(secret.size()),
         reinterpret_cast<const unsigned char*>(message.data()), message.size(),
         hash, &len);
    return std::string(reinterpret_cast<char*>(hash), len);
}

inline std::string createTestJWT(
    const std::string& scope  = "viewer",
    const std::string& sub    = "test-user",
    const std::string& secret = "test-secret-key-16bytes")
{
    using namespace std::chrono;
    const auto now = system_clock::now();
    const auto iat = duration_cast<seconds>(now.time_since_epoch()).count();
    const auto exp = iat + 3600;

    nlohmann::json hdr  = {{"alg", "HS256"}, {"typ", "JWT"}};
    nlohmann::json body = {{"sub", sub}, {"scope", scope}, {"iat", iat}, {"exp", exp}};

    std::string hdrEnc  = base64UrlEncode(hdr.dump());
    std::string bodyEnc = base64UrlEncode(body.dump());
    std::string sig     = base64UrlEncode(hmacSha256(hdrEnc + "." + bodyEnc, secret));

    return hdrEnc + "." + bodyEnc + "." + sig;
}

// ─── Async wait helper ────────────────────────────────────────────────────

template <typename Predicate>
inline bool wait_for(Predicate pred,
                     std::chrono::milliseconds timeout = std::chrono::milliseconds{2000},
                     std::chrono::milliseconds poll    = std::chrono::milliseconds{10})
{
    const auto deadline = std::chrono::steady_clock::now() + timeout;
    while (!pred()) {
        if (std::chrono::steady_clock::now() >= deadline) return false;
        std::this_thread::sleep_for(poll);
    }
    return true;
}

// ─── io_context test fixture base ─────────────────────────────────────────

class IoContextFixture : public ::testing::Test {
protected:
    void SetUp() override {
        io_   = std::make_unique<asio::io_context>(1);
        work_ = std::make_unique<asio::executor_work_guard<asio::io_context::executor_type>>(
            io_->get_executor());
        thread_ = std::thread([this] { io_->run(); });
    }

    void TearDown() override {
        work_.reset();
        io_->stop();
        if (thread_.joinable()) thread_.join();
    }

    std::unique_ptr<asio::io_context> io_;
    std::unique_ptr<asio::executor_work_guard<asio::io_context::executor_type>> work_;
    std::thread thread_;
};

}  // namespace rcc::test
