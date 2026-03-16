#pragma once

#include "rcc/adapter/radio_adapter.hpp"
#include "rcc/config/config_manager.hpp"
#include "rcc/common/types.hpp"
#include <asio/io_context.hpp>
#include <mutex>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

namespace rcc::radio {

struct RadioDescriptor {
    std::string id;
    std::string adapter_type;
    adapter::AdapterPtr adapter;
    common::RadioState state;
};

class RadioManager {
public:
    RadioManager(asio::io_context& io, const config::Configuration& config);

    void start();
    void stop();

    std::vector<RadioDescriptor> list_radios() const;
    std::optional<std::string> active_radio() const;
    bool set_active_radio(const std::string& id);
    adapter::AdapterPtr get_adapter(const std::string& id) const;
    common::RadioState get_state(const std::string& id) const;

private:
    asio::io_context& io_;
    mutable std::mutex mutex_;
    std::unordered_map<std::string, RadioDescriptor> radios_;
    std::optional<std::string> active_radio_;

    void load_from_config(const config::Configuration& config);
};

}  // namespace rcc::radio

#pragma once

#include <string>
#include <vector>

namespace asio {
class io_context;
}

namespace rcc::config {
class ConfigManager;
}  // namespace rcc::config

namespace rcc::radio {

struct RadioInfo {
    std::string id;
    std::string model;
};

class RadioManager {
public:
    RadioManager(asio::io_context& io, config::ConfigManager& config);
    ~RadioManager();

    void start();
    void stop();

    std::vector<RadioInfo> listRadios() const;
    bool setActiveRadio(const std::string& radioId);
    std::string activeRadio() const;

private:
    asio::io_context& io_;
    config::ConfigManager& config_;
    std::string activeRadio_{};
    std::vector<RadioInfo> radios_;
};

}  // namespace rcc::radio


