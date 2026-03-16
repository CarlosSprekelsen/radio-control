#pragma once

#include "rcc/common/types.hpp"
#include <string>
#include <memory>
#include <optional>
#include <vector>

namespace rcc::adapter {

struct CapabilityInfo {
    std::vector<double> supported_frequencies_mhz;
    std::pair<double, double> power_range_watts{0.0, 0.0};
};

class IRadioAdapter {
public:
    virtual ~IRadioAdapter() = default;

    virtual std::string id() const = 0;
    virtual CapabilityInfo capabilities() const = 0;

    virtual common::CommandResult connect() = 0;
    virtual common::CommandResult set_power(double watts) = 0;
    virtual common::CommandResult set_channel(int channel_index, double frequency_mhz) = 0;
    virtual common::CommandResult refresh_state() = 0;

    virtual common::RadioState state() const = 0;
};

using AdapterPtr = std::shared_ptr<IRadioAdapter>;

}  // namespace rcc::adapter

#pragma once

#include <string>

namespace rcc::adapter {

struct AdapterResponse {
    bool success{false};
    std::string message{};
};

class RadioAdapter {
public:
    virtual ~RadioAdapter() = default;

    virtual AdapterResponse connect() = 0;
    virtual AdapterResponse setPower(double watts) = 0;
    virtual AdapterResponse setChannel(int channelIndex) = 0;
};

}  // namespace rcc::adapter


