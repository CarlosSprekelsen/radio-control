#include "rcc/adapter/silvus_adapter.hpp"

#include <asio/ip/tcp.hpp>
#include <nlohmann/json.hpp>

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cctype>
#include <iomanip>
#include <optional>
#include <sstream>
#include <string>
#include <vector>

namespace rcc::adapter {

namespace {

struct EndpointInfo {
    std::string host;
    std::string port;
    std::string path;
};

std::optional<EndpointInfo> parseEndpoint(const std::string& endpoint) {
    const std::string prefix = "http://";
    if (endpoint.rfind(prefix, 0) != 0) {
        return std::nullopt;
    }

    std::string remainder = endpoint.substr(prefix.size());
    std::string hostPort;
    std::string path = "/streamscape_api";

    const auto slashPos = remainder.find('/');
    if (slashPos == std::string::npos) {
        hostPort = remainder;
    } else {
        hostPort = remainder.substr(0, slashPos);
        const auto candidate = remainder.substr(slashPos);
        if (!candidate.empty()) {
            path = candidate;
        }
    }

    std::string host;
    std::string port = "80";
    const auto colonPos = hostPort.find(':');
    if (colonPos == std::string::npos) {
        host = hostPort;
    } else {
        host = hostPort.substr(0, colonPos);
        port = hostPort.substr(colonPos + 1);
        if (port.empty()) {
            port = "80";
        }
    }

    if (host.empty()) {
        return std::nullopt;
    }
    return EndpointInfo{std::move(host), std::move(port), std::move(path)};
}

std::optional<std::string> sendHttpPost(const std::string& endpoint,
                                        const std::string& requestBody,
                                        std::string& error,
                                        std::chrono::milliseconds timeout) {
    const auto parsed = parseEndpoint(endpoint);
    if (!parsed) {
        error = "Invalid endpoint URI";
        return std::nullopt;
    }

    asio::ip::tcp::iostream stream;
    stream.expires_after(timeout);
    stream.connect(parsed->host, parsed->port);
    if (!stream) {
        error = "Connection failed: " + stream.error().message();
        return std::nullopt;
    }

    std::ostringstream request;
    request << "POST " << parsed->path << " HTTP/1.1\r\n"
            << "Host: " << parsed->host << "\r\n"
            << "Content-Type: application/json\r\n"
            << "Content-Length: " << requestBody.size() << "\r\n"
            << "Connection: close\r\n"
            << "\r\n"
            << requestBody;

    stream << request.str();
    stream.flush();
    if (!stream) {
        error = "Write failed: " + stream.error().message();
        return std::nullopt;
    }

    std::string statusLine;
    std::getline(stream, statusLine);
    if (!stream) {
        error = "Read failed: " + stream.error().message();
        return std::nullopt;
    }
    if (!statusLine.empty() && statusLine.back() == '\r') {
        statusLine.pop_back();
    }

    int statusCode = 0;
    {
        std::istringstream ss(statusLine);
        std::string httpVersion;
        ss >> httpVersion >> statusCode;
    }

    std::string header;
    size_t contentLength = 0;
    while (std::getline(stream, header) && header != "\r") {
        if (!header.empty() && header.back() == '\r') {
            header.pop_back();
        }
        std::string lower = header;
        std::transform(lower.begin(), lower.end(), lower.begin(), [](unsigned char c) {
            return static_cast<char>(std::tolower(c));
        });
        if (lower.rfind("content-length:", 0) == 0) {
            const auto value = header.substr(std::string("Content-Length:").size());
            try {
                contentLength = std::stoull(value);
            } catch (...) {
                contentLength = 0;
            }
        }
    }

    std::string body;
    if (contentLength > 0) {
        body.resize(contentLength);
        stream.read(body.data(), static_cast<std::streamsize>(contentLength));
        if (stream.gcount() != static_cast<std::streamsize>(contentLength)) {
            error = "Body read failed: " + stream.error().message();
            return std::nullopt;
        }
    } else {
        std::ostringstream remaining;
        remaining << stream.rdbuf();
        body = remaining.str();
    }

    if (statusCode != 200) {
        error = "HTTP " + std::to_string(statusCode);
        return std::nullopt;
    }

    return body;
}

std::string formatSilvusNumber(double value) {
    std::ostringstream ss;
    ss << std::fixed << std::setprecision(6) << value;
    std::string text = ss.str();
    while (text.size() > 1 && text.back() == '0') {
        text.pop_back();
    }
    if (!text.empty() && text.back() == '.') {
        text.pop_back();
    }
    return text;
}

double dbmToWatts(double dbm) {
    return std::pow(10.0, dbm / 10.0) / 1000.0;
}

std::string uppercase(std::string text) {
    std::transform(text.begin(), text.end(), text.begin(), [](unsigned char c) {
        return static_cast<char>(std::toupper(c));
    });
    return text;
}

std::string extractVendorMessage(const nlohmann::json& error) {
    if (error.is_string()) {
        return error.get<std::string>();
    }
    if (error.is_object()) {
        if (const auto it = error.find("message"); it != error.end() && it->is_string()) {
            return it->get<std::string>();
        }
        if (const auto it = error.find("code"); it != error.end()) {
            if (it->is_string()) {
                return it->get<std::string>();
            }
            if (it->is_number_integer()) {
                return std::to_string(it->get<int>());
            }
        }
    }
    return error.dump();
}

common::CommandResult normalizeSilvusError(const nlohmann::json& error) {
    const auto payload = error.dump();
    const auto message = extractVendorMessage(error);
    const auto normalized = uppercase(message + " " + payload);

    if (normalized.find("OUT_OF_RANGE") != std::string::npos ||
        normalized.find("INVALID_RANGE") != std::string::npos ||
        normalized.find("INVALID PARAM") != std::string::npos ||
        normalized.find("INVALID_PARAMS") != std::string::npos ||
        normalized.find("-32002") != std::string::npos ||
        normalized.find("-32602") != std::string::npos) {
        return {common::CommandResultCode::InvalidRange, message, payload};
    }
    if (normalized.find("BUSY") != std::string::npos ||
        normalized.find("RECOVER") != std::string::npos) {
        return {common::CommandResultCode::Busy, message, payload};
    }
    if (normalized.find("UNAVAILABLE") != std::string::npos ||
        normalized.find("REBOOT") != std::string::npos ||
        normalized.find("BOOT") != std::string::npos ||
        normalized.find("DISCONNECT") != std::string::npos ||
        normalized.find("TIMEOUT") != std::string::npos ||
        normalized.find("-32000") != std::string::npos) {
        return {common::CommandResultCode::Unavailable, message, payload};
    }

    return {common::CommandResultCode::InternalError, message, payload};
}

void addFrequencySpec(std::vector<double>& freqs, const std::string& text) {
    const auto firstColon = text.find(':');
    if (firstColon == std::string::npos) {
        try {
            freqs.push_back(std::stod(text));
        } catch (...) {
        }
        return;
    }

    const auto secondColon = text.find(':', firstColon + 1);
    if (secondColon == std::string::npos || text.find(':', secondColon + 1) != std::string::npos) {
        return;
    }

    try {
        const double start = std::stod(text.substr(0, firstColon));
        const double step = std::stod(text.substr(firstColon + 1, secondColon - firstColon - 1));
        const double end = std::stod(text.substr(secondColon + 1));
        if (step <= 0.0 || end < start) {
            return;
        }

        const auto count = static_cast<int>(std::floor(((end - start) / step) + 0.5));
        for (int i = 0; i <= count; ++i) {
            const double value = start + (step * i);
            if (value <= end + 0.000001) {
                freqs.push_back(value);
            }
        }
    } catch (...) {
    }
}

std::vector<double> parseSupportedFrequencies(const nlohmann::json& response) {
    std::vector<double> freqs;
    if (!response.contains("result") || !response["result"].is_array()) {
        return freqs;
    }

    for (const auto& item : response["result"]) {
        if (!item.is_object()) {
            continue;
        }
        if (!item.contains("frequencies") || !item["frequencies"].is_array()) {
            continue;
        }
        for (const auto& entry : item["frequencies"]) {
            if (!entry.is_string()) {
                continue;
            }
            addFrequencySpec(freqs, entry.get<std::string>());
        }
    }

    std::sort(freqs.begin(), freqs.end());
    freqs.erase(std::unique(freqs.begin(), freqs.end()), freqs.end());
    return freqs;
}

std::optional<int> parsePowerResult(const nlohmann::json& response) {
    if (!response.contains("result") || !response["result"].is_array() || response["result"].empty()) {
        return std::nullopt;
    }
    const auto& result = response["result"][0];
    try {
        if (result.is_string()) {
            return std::stoi(result.get<std::string>());
        }
        if (result.is_number_integer()) {
            return result.get<int>();
        }
    } catch (...) {
    }
    return std::nullopt;
}

std::optional<bool> parseBooleanResult(const nlohmann::json& response) {
    if (!response.contains("result") || !response["result"].is_array() || response["result"].empty()) {
        return std::nullopt;
    }

    const auto& result = response["result"][0];
    if (result.is_boolean()) {
        return result.get<bool>();
    }
    if (result.is_number_integer()) {
        return result.get<int>() != 0;
    }
    if (!result.is_string()) {
        return std::nullopt;
    }

    std::string text = result.get<std::string>();
    std::transform(text.begin(), text.end(), text.begin(), [](unsigned char c) {
        return static_cast<char>(std::tolower(c));
    });
    if (text == "1" || text == "true" || text == "enabled" || text == "on") {
        return true;
    }
    if (text == "0" || text == "false" || text == "disabled" || text == "off") {
        return false;
    }
    return std::nullopt;
}

std::optional<double> parseFrequencyResult(const nlohmann::json& response) {
    if (!response.contains("result") || !response["result"].is_array() || response["result"].empty()) {
        return std::nullopt;
    }
    const auto& result = response["result"][0];
    try {
        if (result.is_string()) {
            return std::stod(result.get<std::string>());
        }
        if (result.is_number()) {
            return result.get<double>();
        }
    } catch (...) {
    }
    return std::nullopt;
}

std::optional<bool> readEnableMaxPower(const std::string& endpoint,
                                       std::chrono::milliseconds timeout) {
    nlohmann::json request = {
        {"jsonrpc", "2.0"},
        {"id", "enable_max_power"},
        {"method", "enable_max_power"}
    };

    std::string error;
    const auto responseBody = sendHttpPost(endpoint, request.dump(), error, timeout);
    if (!responseBody) {
        return std::nullopt;
    }

    nlohmann::json response;
    try {
        response = nlohmann::json::parse(*responseBody);
    } catch (...) {
        return std::nullopt;
    }
    if (response.contains("error")) {
        return std::nullopt;
    }
    return parseBooleanResult(response);
}

std::optional<int> findChannelIndex(const CapabilityInfo& caps, double frequencyMhz) {
    for (size_t i = 0; i < caps.supported_frequencies_mhz.size(); ++i) {
        if (std::abs(caps.supported_frequencies_mhz[i] - frequencyMhz) < 0.001) {
            return static_cast<int>(i) + 1;
        }
    }
    return std::nullopt;
}

bool isRecoveryWindowActive(std::chrono::steady_clock::time_point until) {
    return until != std::chrono::steady_clock::time_point{} &&
           std::chrono::steady_clock::now() < until;
}

}  // namespace

SilvusAdapter::SilvusAdapter(std::string id,
                             std::string endpoint,
                             std::optional<std::pair<double, double>> configured_power_dbm_range,
                             std::chrono::milliseconds http_timeout,
                             std::chrono::seconds soft_boot_recovery_duration)
    : id_(std::move(id))
    , endpoint_(std::move(endpoint))
    , http_timeout_(http_timeout)
    , soft_boot_recovery_duration_(soft_boot_recovery_duration) {
    capabilities_.supported_frequencies_mhz = {2412.0, 2437.0, 2462.0};
    if (configured_power_dbm_range) {
        capabilities_.power_range_watts = {
            dbmToWatts(configured_power_dbm_range->first),
            dbmToWatts(configured_power_dbm_range->second)
        };
        capabilities_.power_range_source = "configured";
    } else {
        capabilities_.power_range_watts = {dbmToWatts(0.0), dbmToWatts(39.0)};
        capabilities_.power_range_source = "vendor_default";
    }
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

    nlohmann::json request = {
        {"jsonrpc", "2.0"},
        {"id", "1"},
        {"method", "supported_frequency_profiles"}
    };

    std::string error;
    const auto responseBody = sendHttpPost(endpoint_, request.dump(), error, http_timeout_);
    if (!responseBody) {
        return {common::CommandResultCode::Unavailable, error, std::nullopt};
    }

    nlohmann::json response;
    try {
        response = nlohmann::json::parse(*responseBody);
    } catch (...) {
        return {common::CommandResultCode::InternalError, "Invalid JSON from radio", std::nullopt};
    }

    if (response.contains("error")) {
        return normalizeSilvusError(response["error"]);
    }

    const auto freqs = parseSupportedFrequencies(response);
    if (!freqs.empty()) {
        capabilities_.supported_frequencies_mhz = freqs;
    }
    if (const auto enableMaxPower = readEnableMaxPower(endpoint_, http_timeout_)) {
        capabilities_.enable_max_power = *enableMaxPower;
        capabilities_.manual_power_control_available = !*enableMaxPower;
    }

    recovering_until_ = {};
    state_.status = common::RadioStatus::Ready;
    return {common::CommandResultCode::Ok, {}, std::nullopt};
}

common::CommandResult SilvusAdapter::set_power(double watts) {
    std::lock_guard<std::mutex> lock(mutex_);

    if (isRecoveryWindowActive(recovering_until_)) {
        state_.status = common::RadioStatus::Recovering;
        return {common::CommandResultCode::Busy,
                "Radio is recovering after a frequency change", std::nullopt};
    }

    if (const auto enableMaxPower = readEnableMaxPower(endpoint_, http_timeout_)) {
        capabilities_.enable_max_power = *enableMaxPower;
        capabilities_.manual_power_control_available = !*enableMaxPower;
    }
    if (!capabilities_.manual_power_control_available) {
        return {common::CommandResultCode::Busy,
                "Manual power control is unavailable while enable_max_power=1",
                std::optional<std::string>{"{\"enable_max_power\":true}"}};
    }

    if (watts < capabilities_.power_range_watts.first ||
        watts > capabilities_.power_range_watts.second) {
        return {common::CommandResultCode::InvalidRange,
                "Power outside supported range", std::nullopt};
    }

    const double dbm = std::round(10.0 * std::log10(watts * 1000.0));
    nlohmann::json request = {
        {"jsonrpc", "2.0"},
        {"id", "1"},
        {"method", "power_dBm"},
        {"params", {std::to_string(static_cast<int>(dbm))}}
    };

    std::string error;
    const auto responseBody = sendHttpPost(endpoint_, request.dump(), error, http_timeout_);
    if (!responseBody) {
        return {common::CommandResultCode::Unavailable, error, std::nullopt};
    }

    nlohmann::json response;
    try {
        response = nlohmann::json::parse(*responseBody);
    } catch (...) {
        return {common::CommandResultCode::InternalError, "Invalid JSON from radio", std::nullopt};
    }

    if (response.contains("error")) {
        return normalizeSilvusError(response["error"]);
    }

    state_.power_watts = watts;
    state_.status = common::RadioStatus::Ready;
    return {common::CommandResultCode::Ok, {}, std::nullopt};
}

common::CommandResult SilvusAdapter::set_channel(int channel_index,
                                                  double frequency_mhz) {
    std::lock_guard<std::mutex> lock(mutex_);

    if (isRecoveryWindowActive(recovering_until_)) {
        state_.status = common::RadioStatus::Recovering;
        return {common::CommandResultCode::Busy,
                "Radio is recovering after a frequency change", std::nullopt};
    }

    if (channel_index <= 0 || frequency_mhz <= 0.0) {
        return {common::CommandResultCode::InvalidRange,
                "Invalid channel or frequency", std::nullopt};
    }
    if (!capabilities_.supported_frequencies_mhz.empty() &&
        !findChannelIndex(capabilities_, frequency_mhz)) {
        return {common::CommandResultCode::InvalidRange,
                "Frequency outside supported profile", std::nullopt};
    }

    nlohmann::json request = {
        {"jsonrpc", "2.0"},
        {"id", "1"},
        {"method", "freq"},
        {"params", {formatSilvusNumber(frequency_mhz)}}
    };

    std::string error;
    const auto responseBody = sendHttpPost(endpoint_, request.dump(), error, http_timeout_);
    if (!responseBody) {
        return {common::CommandResultCode::Unavailable, error, std::nullopt};
    }

    nlohmann::json response;
    try {
        response = nlohmann::json::parse(*responseBody);
    } catch (...) {
        return {common::CommandResultCode::InternalError, "Invalid JSON from radio", std::nullopt};
    }

    if (response.contains("error")) {
        return normalizeSilvusError(response["error"]);
    }

    state_.channel_index = findChannelIndex(capabilities_, frequency_mhz).value_or(channel_index);
    recovering_until_ = std::chrono::steady_clock::now() + soft_boot_recovery_duration_;
    state_.status = common::RadioStatus::Recovering;
    return {common::CommandResultCode::Ok, {}, std::nullopt};
}

common::CommandResult SilvusAdapter::refresh_state() {
    std::lock_guard<std::mutex> lock(mutex_);

    if (isRecoveryWindowActive(recovering_until_)) {
        state_.status = common::RadioStatus::Recovering;
        return {common::CommandResultCode::Busy,
                "Radio is recovering after a frequency change", std::nullopt};
    }

    nlohmann::json powerRequest = {
        {"jsonrpc", "2.0"},
        {"id", "1"},
        {"method", "power_dBm"}
    };
    std::string error;
    const auto powerBody = sendHttpPost(endpoint_, powerRequest.dump(), error, http_timeout_);
    if (!powerBody) {
        return {common::CommandResultCode::Unavailable, error, std::nullopt};
    }

    nlohmann::json powerResponse;
    try {
        powerResponse = nlohmann::json::parse(*powerBody);
    } catch (...) {
        return {common::CommandResultCode::InternalError, "Invalid JSON from radio", std::nullopt};
    }
    if (powerResponse.contains("error")) {
        return normalizeSilvusError(powerResponse["error"]);
    }

    if (const auto powerDbm = parsePowerResult(powerResponse)) {
        state_.power_watts = dbmToWatts(*powerDbm);
    } else {
        state_.power_watts.reset();
    }

    nlohmann::json freqRequest = {
        {"jsonrpc", "2.0"},
        {"id", "2"},
        {"method", "freq"}
    };
    const auto freqBody = sendHttpPost(endpoint_, freqRequest.dump(), error, http_timeout_);
    if (!freqBody) {
        return {common::CommandResultCode::Unavailable, error, std::nullopt};
    }

    nlohmann::json freqResponse;
    try {
        freqResponse = nlohmann::json::parse(*freqBody);
    } catch (...) {
        return {common::CommandResultCode::InternalError, "Invalid JSON from radio", std::nullopt};
    }
    if (freqResponse.contains("error")) {
        return normalizeSilvusError(freqResponse["error"]);
    }

    if (const auto frequency = parseFrequencyResult(freqResponse)) {
        if (const auto maybeIndex = findChannelIndex(capabilities_, *frequency)) {
            state_.channel_index = *maybeIndex;
        } else {
            state_.channel_index.reset();
        }
    } else {
        state_.channel_index.reset();
    }
    if (const auto enableMaxPower = readEnableMaxPower(endpoint_, http_timeout_)) {
        capabilities_.enable_max_power = *enableMaxPower;
        capabilities_.manual_power_control_available = !*enableMaxPower;
    }

    recovering_until_ = {};
    state_.status = common::RadioStatus::Ready;
    return {common::CommandResultCode::Ok, {}, std::nullopt};
}

common::RadioState SilvusAdapter::state() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return state_;
}

}  // namespace rcc::adapter
