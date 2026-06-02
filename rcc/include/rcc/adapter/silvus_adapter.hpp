#pragma once

#include "rcc/adapter/radio_adapter.hpp"
#include <chrono>
#include <mutex>
#include <optional>
#include <string>
#include <utility>

namespace rcc::adapter {

class SilvusAdapter final : public IRadioAdapter {
public:
    SilvusAdapter(std::string id,
                  std::string endpoint,
                  std::optional<std::pair<double, double>> configured_power_dbm_range = std::nullopt,
                  std::chrono::milliseconds http_timeout = std::chrono::milliseconds{3000},
                  std::chrono::seconds soft_boot_recovery_duration = std::chrono::seconds{30});

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
    std::chrono::milliseconds http_timeout_;
    std::chrono::steady_clock::time_point recovering_until_{};
    std::chrono::seconds soft_boot_recovery_duration_;
    CapabilityInfo capabilities_;
    common::RadioState state_;
    mutable std::mutex mutex_;
};

}  // namespace rcc::adapter
