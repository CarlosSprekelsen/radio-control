#pragma once

#include "rcc/adapter/radio_adapter.hpp"
#include <mutex>
#include <string>

namespace rcc::adapter {

class SilvusAdapter final : public IRadioAdapter {
public:
    SilvusAdapter(std::string id, std::string endpoint);

    std::string id() const override;
    CapabilityInfo capabilities() const override;

    common::CommandResult connect() override;
    common::CommandResult set_power(double watts) override;
    common::CommandResult set_channel(int channel_index, double frequency_mhz) override;
    common::CommandResult refresh_state() override;

    common::RadioState state() const override;

private:
    std::string id_;
    std::string endpoint_;
    CapabilityInfo capabilities_;
    common::RadioState state_;
    mutable std::mutex mutex_;
};

}  // namespace rcc::adapter
