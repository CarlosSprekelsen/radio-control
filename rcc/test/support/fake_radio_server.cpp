#include "support/fake_radio_server.hpp"

#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>

#include <cstring>
#include <sstream>
#include <stdexcept>

namespace rcc::test {

FakeRadioServer::FakeRadioServer(uint16_t port) : port_(port) {}

FakeRadioServer::~FakeRadioServer() { stop(); }

void FakeRadioServer::start() {
    server_fd_ = ::socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd_ < 0) throw std::runtime_error("FakeRadioServer: socket() failed");

    int opt = 1;
    ::setsockopt(server_fd_, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    sockaddr_in addr{};
    addr.sin_family      = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    addr.sin_port        = htons(port_);

    if (::bind(server_fd_, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) < 0)
        throw std::runtime_error("FakeRadioServer: bind() failed on port " +
                                 std::to_string(port_));
    ::listen(server_fd_, 4);

    running_ = true;
    thread_  = std::thread([this] { acceptLoop(); });
}

void FakeRadioServer::stop() {
    running_ = false;
    if (server_fd_ >= 0) { ::shutdown(server_fd_, SHUT_RDWR); ::close(server_fd_); server_fd_ = -1; }
    if (thread_.joinable()) thread_.join();
}

void FakeRadioServer::acceptLoop() {
    while (running_) {
        sockaddr_in client{};
        socklen_t   len = sizeof(client);
        int cfd = ::accept(server_fd_, reinterpret_cast<sockaddr*>(&client), &len);
        if (cfd < 0) break;

        // Read request (ignore content)
        char buf[4096] = {};
        ::recv(cfd, buf, sizeof(buf) - 1, 0);
        ++request_count_;

        // Send scripted response
        std::ostringstream resp;
        resp << "HTTP/1.1 " << response_.http_status << " OK\r\n"
             << "Content-Type: application/json\r\n"
             << "Content-Length: " << response_.body.size() << "\r\n"
             << "Connection: close\r\n\r\n"
             << response_.body;
        const auto s = resp.str();
        ::send(cfd, s.data(), s.size(), MSG_NOSIGNAL);
        ::close(cfd);
    }
}

}  // namespace rcc::test
