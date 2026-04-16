#include "http_server.hpp"
#include "silvus_mock.hpp"
#include "web_assets.hpp"
#include <dts/common/rest/rest_server.hpp>
#include <dts/common/rest/rest_router.hpp>
#include <dts/common/core/timing_profile.hpp>
#include <asio.hpp>
#include <iostream>

HttpServer::HttpServer(int port, SilvusMock* mock)
    : port_(port), mock_(mock) {}

static std::string make_http_response(const std::string& body, const std::string& contentType) {
    return "HTTP/1.1 200 OK\r\n" +
           std::string("Content-Type: ") + contentType + "\r\n" +
           "Content-Length: " + std::to_string(body.size()) + "\r\n" +
           "Connection: close\r\n" +
           "\r\n" +
           body;
}

void HttpServer::serve() {
    using namespace dts::common;
    asio::io_context io;
    rest::RestServer server(io, rest::RestServer::tcp::endpoint(asio::ip::address_v4::any(), port_), core::TimingProfile());

    server.router().addRoute("/streamscape_api", [this](const rest::HttpRequest &req) {
        std::string body = req.body;
        if (body.empty()) {
            std::string bad = "{\"jsonrpc\":\"2.0\",\"error\":{\"code\":-32600,\"message\":\"Invalid Request\"},\"id\":null}";
            std::cout << "[silvus-mock] JSON-RPC request empty /streamscape_api" << std::endl;
            std::cout << "[silvus-mock] JSON-RPC response: " << bad << std::endl;
            return make_http_response(bad, "application/json");
        }
        std::cout << "[silvus-mock] JSON-RPC request: " << body << std::endl;
        try {
            std::string response = mock_->handle_jsonrpc_text(body);
            std::cout << "[silvus-mock] JSON-RPC response: " << response << std::endl;
            return make_http_response(response, "application/json");
        } catch (const std::exception& ex) {
            std::string bad = "{\"jsonrpc\":\"2.0\",\"error\":{\"code\":-32603,\"message\":\"Internal error\",\"data\":\"" + std::string(ex.what()) + "\"},\"id\":null}";
            std::cout << "[silvus-mock] JSON-RPC handler exception: " << ex.what() << std::endl;
            return make_http_response(bad, "application/json");
        }
    });

    server.router().addRoute("/status", [this](const rest::HttpRequest &) {
        return make_http_response(mock_->get_status().dump(), "application/json");
    });

    server.router().addRoute("/", [](const rest::HttpRequest &) {
        return make_http_response(get_web_index_html(), "text/html");
    });
    server.router().addRoute("/index.html", [](const rest::HttpRequest &) {
        return make_http_response(get_web_index_html(), "text/html");
    });

    server.start();
    std::cout << "[silvus-mock] REST server started on port " << port_ << std::endl;
    io.run();
}
