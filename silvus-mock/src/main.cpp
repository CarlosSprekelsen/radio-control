// Silvus Mock Main Entry Point
#include "http_server.hpp"
#include "maintenance_server.hpp"
#include "silvus_mock.hpp"
#include <cstdlib>
#include <iostream>
#include <system_error>

static int env_port(const char* name, int fallback) {
    const char* value = std::getenv(name);
    if (!value) {
        return fallback;
    }
    try {
        return std::stoi(value);
    } catch (...) {
        return fallback;
    }
}

int main(int argc, char* argv[]) {
    SilvusMock mock;
    int http_port = env_port("SILVUS_MOCK_HTTP_PORT", 80);
    int maintenance_port = env_port("SILVUS_MOCK_MAINT_PORT", 50000);

    try {
        MaintenanceServer maintenance(maintenance_port, &mock);
        std::cout << "[silvus-mock] Maintenance server starting on port " << maintenance_port << std::endl;
        maintenance.start();

        HttpServer server(http_port, &mock);
        std::cout << "[silvus-mock] HTTP REST server starting on port " << http_port << std::endl;
        server.serve();
    } catch (const std::system_error& e) {
        std::cerr << "[silvus-mock] Failed to start server: " << e.what() << std::endl;
        std::cerr << "[silvus-mock] Hint: Port " << http_port << " (HTTP) or "
                  << maintenance_port << " (maintenance) may already be in use."
                  << " Check for another silvus-mock instance or change ports." << std::endl;
        return 1;
    } catch (const std::exception& e) {
        std::cerr << "[silvus-mock] Fatal error: " << e.what() << std::endl;
        return 1;
    }

    std::cout << "[silvus-mock] Exiting" << std::endl;
    return 0;
}
