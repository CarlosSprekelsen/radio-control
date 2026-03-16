#pragma once

#include "rcc/config/types.hpp"

#include <filesystem>
#include <mutex>

namespace rcc::config {

class ConfigManager {
public:
    explicit ConfigManager(std::filesystem::path path);

    const Config& current() const;
    void reload();

private:
    Config loadFromFile(const std::filesystem::path& path) const;

    std::filesystem::path path_;
    Config config_;
    mutable std::mutex mutex_;
};

}  // namespace rcc::config
