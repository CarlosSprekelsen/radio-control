#include "rcc/radio/radio_manager.hpp"

#include "rcc/adapter/silvus_adapter.hpp"

#include <stdexcept>

namespace rcc::radio {

RadioManager::RadioManager(asio::io_context& io, const config::Configuration& config)
    : io_(io) {
    load_from_config(config);
}

void RadioManager::start() {
    std::lock_guard<std::mutex> lock(mutex_);
    for (auto& [id, descriptor] : radios_) {
        auto result = descriptor.adapter->connect();
        if (result.code == common::CommandResultCode::Ok) {
            descriptor.state = descriptor.adapter->state();
        }
    }
}

void RadioManager::stop() {
    std::lock_guard<std::mutex> lock(mutex_);
    radios_.clear();
    active_radio_.reset();
}

std::vector<RadioDescriptor> RadioManager::list_radios() const {
    std::lock_guard<std::mutex> lock(mutex_);
    std::vector<RadioDescriptor> result;
    result.reserve(radios_.size());
    for (const auto& [id, descriptor] : radios_) {
        result.push_back(descriptor);
    }
    return result;
}

std::optional<std::string> RadioManager::active_radio() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return active_radio_;
}

bool RadioManager::set_active_radio(const std::string& id) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (radios_.find(id) == radios_.end()) {
        return false;
    }
    active_radio_ = id;
    return true;
}

adapter::AdapterPtr RadioManager::get_adapter(const std::string& id) const {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = radios_.find(id);
    if (it == radios_.end()) {
        return nullptr;
    }
    return it->second.adapter;
}

common::RadioState RadioManager::get_state(const std::string& id) const {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = radios_.find(id);
    if (it == radios_.end()) {
        return {};
    }
    return it->second.adapter->state();
}

void RadioManager::load_from_config(const config::Configuration& config) {
    std::lock_guard<std::mutex> lock(mutex_);
    radios_.clear();
    for (const auto& radio : config.radios) {
        adapter::AdapterPtr adapter;
        if (radio.adapter == "silvus") {
            adapter = std::make_shared<adapter::SilvusAdapter>(radio.id, radio.endpoint);
        } else {
            continue;  // Unsupported adapter type for now
        }

        RadioDescriptor descriptor{
            .id = radio.id,
            .adapter_type = radio.adapter,
            .adapter = adapter,
            .state = adapter->state()
        };
        radios_.emplace(radio.id, std::move(descriptor));
    }
}

}  // namespace rcc::radio

#include "rcc/radio/radio_manager.hpp"

#include "rcc/config/config_manager.hpp"

#include <asio/io_context.hpp>
#include <iostream>

namespace rcc::radio {

RadioManager::RadioManager(asio::io_context& io, config::ConfigManager& config)
    : io_{io}, config_{config} {
    radios_.push_back({"radio-1", "Silvus-Stub"});
}

RadioManager::~RadioManager() = default;

void RadioManager::start() {
    std::cout << "[RadioManager] start() – discovery pending" << std::endl;
}

void RadioManager::stop() {
    std::cout << "[RadioManager] stop()" << std::endl;
}

std::vector<RadioInfo> RadioManager::listRadios() const {
    return radios_;
}

bool RadioManager::setActiveRadio(const std::string& radioId) {
    for (const auto& radio : radios_) {
        if (radio.id == radioId) {
            activeRadio_ = radioId;
            std::cout << "[RadioManager] Active radio set to " << radioId << std::endl;
            return true;
        }
    }
    return false;
}

std::string RadioManager::activeRadio() const {
    return activeRadio_;
}

}  // namespace rcc::radio


