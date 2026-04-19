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
    const int fd = ::socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0) throw std::runtime_error("FakeRadioServer: socket() failed");
    server_fd_.store(fd);

    int opt = 1;
    ::setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    sockaddr_in addr{};
    addr.sin_family      = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    addr.sin_port        = htons(port_);

    if (::bind(fd, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) < 0)
        throw std::runtime_error("FakeRadioServer: bind() failed on port " +
                                 std::to_string(port_));
    ::listen(fd, 4);

    running_ = true;
    thread_  = std::thread([this] { acceptLoop(); });
}

void FakeRadioServer::stop() {
    running_ = false;
    const int fd = server_fd_.exchange(-1);
    if (fd >= 0) {
        ::shutdown(fd, SHUT_RDWR);
        ::close(fd);
    }
    if (thread_.joinable()) thread_.join();
}

void FakeRadioServer::acceptLoop() {
    while (running_) {
        sockaddr_in client{};
        socklen_t   len = sizeof(client);
        const int fd = server_fd_.load();
        if (fd < 0) break;
        int cfd = ::accept(fd, reinterpret_cast<sockaddr*>(&client), &len);
        if (cfd < 0) break;

        // Read request (ignore content)
        char buf[4096] = {};
        const ssize_t received = ::recv(cfd, buf, sizeof(buf) - 1, 0);
        if (received >= 0) {
            ++request_count_;
        }

        const std::string request(buf, received > 0 ? static_cast<size_t>(received) : 0);
        RadioResponse response = response_;
        if (response_handler_) {
            response = (*response_handler_)(request);
        }

        std::ostringstream resp;
        resp << "HTTP/1.1 " << response.http_status << " OK\r\n"
             << "Content-Type: application/json\r\n"
             << "Content-Length: " << response.body.size() << "\r\n"
             << "Connection: close\r\n\r\n"
             << response.body;
        const auto s = resp.str();
        ::send(cfd, s.data(), s.size(), MSG_NOSIGNAL);
        ::close(cfd);
    }
}

}  // namespace rcc::test
