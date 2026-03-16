#include "rcc/adapter/silvus_adapter.hpp"

#include <utility>

namespace rcc::adapter {

SilvusAdapter::SilvusAdapter(std::string id, std::string endpoint)
    : id_(std::move(id))
    , endpoint_(std::move(endpoint)) {
    capabilities_.supported_frequencies_mhz = {2412.0, 2437.0, 2462.0};
    capabilities_.power_range_watts = {0.1, 5.0};
    state_.status = common::RadioStatus::Offline;
}

std::string SilvusAdapter::id() const {
    return id_;
}

CapabilityInfo SilvusAdapter::capabilities() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return capabilities_;
}

common::CommandResult SilvusAdapter::connect() {
    std::lock_guard<std::mutex> lock(mutex_);
    state_.status = common::RadioStatus::Ready;
    return {.code = common::CommandResultCode::Ok};
}

common::CommandResult SilvusAdapter::set_power(double watts) {
    std::lock_guard<std::mutex> lock(mutex_);
    state_.power_watts = watts;
    state_.status = common::RadioStatus::Busy;
    state_.status = common::RadioStatus::Ready;
    return {.code = common::CommandResultCode::Ok};
}

common::CommandResult SilvusAdapter::set_channel(int channel_index, double frequency_mhz) {
    std::lock_guard<std::mutex> lock(mutex_);
    state_.channel_index = channel_index;
    state_.status = common::RadioStatus::Busy;
    state_.status = common::RadioStatus::Ready;
    (void)frequency_mhz;
    return {.code = common::CommandResultCode::Ok};
}

common::CommandResult SilvusAdapter::refresh_state() {
    std::lock_guard<std::mutex> lock(mutex_);
    if (state_.status == common::RadioStatus::Offline) {
        state_.status = common::RadioStatus::Ready;
    }
    return {.code = common::CommandResultCode::Ok};
}

common::RadioState SilvusAdapter::state() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return state_;
}

}  // namespace rcc::adapter

#include "rcc/adapter/silvus_adapter.hpp"

#include <iostream>

namespace rcc::adapter {

SilvusAdapter::SilvusAdapter() = default;

SilvusAdapter::~SilvusAdapter() = default;

AdapterResponse SilvusAdapter::connect() {
    std::cout << "[SilvusAdapter] connect()" << std::endl;
    return {true, "connected"};
}

AdapterResponse SilvusAdapter::setPower(double watts) {
    std::cout << "[SilvusAdapter] setPower(" << watts << ")" << std::endl;
    return {true, "stub"};
}

AdapterResponse SilvusAdapter::setChannel(int channelIndex) {
    std::cout << "[SilvusAdapter] setChannel(" << channelIndex << ")" << std::endl;
    return {true, "stub"};
}

}  // namespace rcc::adapter


