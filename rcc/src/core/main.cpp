#include "rcc/application.hpp"
#include "rcc/version.hpp"

#include <asio/io_context.hpp>
#include <iostream>
#include <csignal>
#include <atomic>
#include <filesystem>

namespace {
    std::atomic<bool> g_running{true};
    asio::io_context* g_io_context = nullptr;

    void handle_signal(int signal) {
        std::cout << "\nReceived signal " << signal << ", shutting down..." << std::endl;
        g_running = false;
        if (g_io_context) {
            g_io_context->stop();
        }
    }
}

int main(int argc, char* argv[]) {
    std::cout << "Radio Control Container (C++20)" << std::endl;
    std::cout << "Version: " << rcc::version() << std::endl;
    std::cout << "Git: " << rcc::git_revision() << std::endl;
    std::cout << "Built: " << rcc::build_timestamp() << std::endl;
    std::cout << std::endl;

    try {
        std::signal(SIGINT, handle_signal);
        std::signal(SIGTERM, handle_signal);

        std::filesystem::path configPath = "config/default.yaml";
        if (argc > 1) {
            configPath = argv[1];
        }

        asio::io_context io_context{1};
        g_io_context = &io_context;

        rcc::Application app{io_context, configPath};
        app.start();

        io_context.run();

        app.stop();
        return 0;
    } catch (const std::exception& ex) {
        std::cerr << "Fatal error: " << ex.what() << std::endl;
        return 1;
    }
}


