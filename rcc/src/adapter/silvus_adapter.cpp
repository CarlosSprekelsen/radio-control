#include "rcc/adapter/silvus_adapter.hpp"

#include <utility>

namespace rcc::adapter {

SilvusAdapter::SilvusAdapter(std::string id, std::string endpoint)
    : id_(std::move(id))
    , endpoint_(std::move(endpoint)) {
    // Default capability stubs — replace with HTTP discovery from radio
    capabilities_.supported_frequencies_mhz = {2412.0, 2437.0, 2462.0};
    capabilities_.power_range_watts          = {0.1, 5.0};
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
    // TODO: perform real HTTP handshake with endpoint_
    state_.status = common::RadioStatus::Ready;
    return {common::CommandResultCode::Ok, {}, {}};
}

common::CommandResult SilvusAdapter::set_power(double watts) {
    std::lock_guard<std::mutex> lock(mutex_);
    // TODO: send power command to endpoint_
    state_.power_watts = watts;
    return {common::CommandResultCode::Ok, {}, {}};
}

common::CommandResult SilvusAdapter::set_channel(int channel_index,
                                                  double frequency_mhz) {
    std::lock_guard<std::mutex> lock(mutex_);
    // TODO: send channel command to endpoint_
    state_.channel_index = channel_index;
    (void)frequency_mhz;
    return {common::CommandResultCode::Ok, {}, {}};
}

common::CommandResult SilvusAdapter::refresh_state() {
    std::lock_guard<std::mutex> lock(mutex_);
    // TODO: poll status from endpoint_
    if (state_.status == common::RadioStatus::Offline) {
        state_.status = common::RadioStatus::Ready;
    }
    return {common::CommandResultCode::Ok, {}, {}};
}

common::RadioState SilvusAdapter::state() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return state_;
}

}  // namespace rcc::adapter
