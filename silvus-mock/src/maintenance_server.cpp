#include "maintenance_server.hpp"
#include "silvus_mock.hpp"
#include <asio.hpp>
#include <chrono>
#include <iostream>
#include <mutex>
#include <string>
#include <thread>

using asio::ip::tcp;

struct MaintenanceServer::Impl {
    Impl(int port, SilvusMock* mock)
        : io_context(),
          acceptor(io_context, tcp::endpoint(tcp::v4(), port)),
          mock(mock),
          running(true) {}

    void run() {
        while (running) {
            try {
                tcp::socket socket(io_context);
                acceptor.accept(socket);
                std::thread(&Impl::handle_connection, this, std::move(socket)).detach();
            } catch (const std::exception& ex) {
                if (running) {
                    std::cerr << "[silvus-mock] Maintenance accept error: " << ex.what() << std::endl;
                }
            }
        }
    }

    void handle_connection(tcp::socket socket) {
        try {
            asio::streambuf buffer;
            asio::read_until(socket, buffer, '\n');
            std::istream stream(&buffer);
            std::string payload;
            std::getline(stream, payload);
            if (payload.empty()) {
                return;
            }
            std::cout << "[silvus-mock] Maintenance JSON-RPC request: " << payload << std::endl;
            std::string response_text;
            try {
                response_text = mock->handle_jsonrpc_text(payload);
            } catch (const std::exception& ex) {
                response_text = "{\"jsonrpc\":\"2.0\",\"error\":{\"code\":-32603,\"message\":\"Internal error\",\"data\":\"" + std::string(ex.what()) + "\"},\"id\":null}";
                std::cerr << "[silvus-mock] Maintenance JSON-RPC handler exception: " << ex.what() << std::endl;
            }
            if (response_text.empty() || response_text.back() != '\n') {
                response_text.push_back('\n');
            }
            std::cout << "[silvus-mock] Maintenance JSON-RPC response: " << response_text << std::endl;
            asio::write(socket, asio::buffer(response_text));
        } catch (const std::exception& ex) {
            std::cerr << "[silvus-mock] Maintenance handler error: " << ex.what() << std::endl;
        }
    }

    asio::io_context io_context;
    tcp::acceptor acceptor;
    SilvusMock* mock;
    std::atomic<bool> running;
};

MaintenanceServer::MaintenanceServer(int port, SilvusMock* mock)
    : port_(port), mock_(mock), impl_(std::make_unique<Impl>(port, mock)) {}

MaintenanceServer::~MaintenanceServer() {
    stop();
}

void MaintenanceServer::start() {
    std::thread([this] {
        impl_->run();
    }).detach();
}

void MaintenanceServer::stop() {
    if (impl_) {
        impl_->running = false;
        std::error_code ec;
        impl_->acceptor.close(ec);
        impl_->io_context.stop();
    }
}
