#pragma once

#include "rcc/adapter/radio_adapter.hpp"
#include "rcc/common/types.hpp"
#include "rcc/config/types.hpp"
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
    RadioManager([[maybe_unused]] asio::io_context& io,
                 const config::Config& config);

    void start();
    void stop();

    std::vector<RadioDescriptor> list_radios() const;
    std::optional<std::string> active_radio() const;
    bool set_active_radio(const std::string& id);
    adapter::AdapterPtr get_adapter(const std::string& id) const;
    common::RadioState get_state(const std::string& id) const;

private:
    mutable std::mutex mutex_;
    std::unordered_map<std::string, RadioDescriptor> radios_;
    std::optional<std::string> active_radio_;

    void load_from_config(const config::Config& config);
};

}  // namespace rcc::radio
