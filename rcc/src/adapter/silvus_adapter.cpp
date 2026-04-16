#include "rcc/adapter/silvus_adapter.hpp"

#include <asio/connect.hpp>
#include <asio/ip/tcp.hpp>
#include <asio/read.hpp>
#include <asio/read_until.hpp>
#include <asio/streambuf.hpp>
#include <asio/write.hpp>
#include <nlohmann/json.hpp>

#include <algorithm>
#include <cmath>
#include <chrono>
#include <iostream>
#include <optional>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

namespace rcc::adapter {

namespace {

struct EndpointInfo {
    std::string host;
    std::string port;
    std::string path;
};

std::optional<EndpointInfo> parseEndpoint(const std::string& endpoint) {
    const std::string prefixHttp = "http://";
    if (endpoint.rfind(prefixHttp, 0) != 0) {
        return std::nullopt;
    }
    auto remainder = endpoint.substr(prefixHttp.size());
    std::string host;
    std::string port = "80";
    std::string path = "/streamscape_api";

    const auto slashPos = remainder.find('/');
    std::string hostPort = slashPos == std::string::npos
                               ? remainder
                               : remainder.substr(0, slashPos);
    if (slashPos != std::string::npos) {
        const auto remainderPath = remainder.substr(slashPos);
        if (!remainderPath.empty()) {
            path = remainderPath;
        }
    }

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
                                        std::string& error) {
    const auto parsed = parseEndpoint(endpoint);
    if (!parsed) {
        error = "Invalid endpoint URI";
        return std::nullopt;
    }

    asio::io_context io;
    asio::ip::tcp::resolver resolver(io);
    asio::ip::tcp::resolver::results_type endpoints;
    try {
        endpoints = resolver.resolve(parsed->host, parsed->port);
    } catch (const std::exception& ex) {
        error = std::string("DNS resolve failed: ") + ex.what();
        return std::nullopt;
    }

    asio::ip::tcp::socket socket(io);
    try {
        asio::connect(socket, endpoints);
    } catch (const std::exception& ex) {
        error = std::string("Connection failed: ") + ex.what();
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

    try {
        asio::write(socket, asio::buffer(request.str()));
    } catch (const std::exception& ex) {
        error = std::string("Write failed: ") + ex.what();
        return std::nullopt;
    }

    asio::streambuf responseBuffer;
    try {
        asio::read_until(socket, responseBuffer, "\r\n\r\n");
    } catch (const std::exception& ex) {
        error = std::string("Read failed: ") + ex.what();
        return std::nullopt;
    }

    std::istream responseStream(&responseBuffer);
    std::string statusLine;
    std::getline(responseStream, statusLine);
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
    while (std::getline(responseStream, header) && header != "\r") {
        if (!header.empty() && header.back() == '\r') {
            header.pop_back();
        }
        std::string lower = header;
        std::transform(lower.begin(), lower.end(), lower.begin(), [](unsigned char c) {
            return static_cast<char>(std::tolower(c));
        });
        if (lower.rfind("content-length:", 0) == 0) {
            const auto value = header.substr(std::string("content-length:").size());
            contentLength = std::stoull(value);
        }
    }

    std::string body;
    if (contentLength > 0) {
        std::vector<char> data(contentLength);
        asio::read(socket, asio::buffer(data), asio::transfer_exactly(contentLength));
        body.assign(data.data(), contentLength);
    } else {
        std::ostringstream remaining;
        if (responseStream.rdbuf()->in_avail() > 0) {
            remaining << responseStream.rdbuf();
        }
        std::error_code ec;
        while (asio::read(socket, responseBuffer, asio::transfer_at_least(1), ec)) {
            // continue reading until EOF or error
        }
        if (ec == asio::error::eof || !ec) {
            if (responseBuffer.size() > 0) {
                std::ostringstream ss;
                ss << &responseBuffer;
                remaining << ss.str();
            }
        }
        body = remaining.str();
    }

    if (statusCode != 200) {
        error = "HTTP " + std::to_string(statusCode);
        return std::nullopt;
    }
    return body;
}

std::optional<int> findChannelIndex(const CapabilityInfo& caps, double frequencyMhz) {
    for (size_t i = 0; i < caps.supported_frequencies_mhz.size(); ++i) {
        if (std::abs(caps.supported_frequencies_mhz[i] - frequencyMhz) < 0.001) {
            return static_cast<int>(i) + 1;
        }
    }
    return std::nullopt;
}

}  // namespace

SilvusAdapter::SilvusAdapter(std::string id, std::string endpoint)
    : id_(std::move(id))
    , endpoint_(std::move(endpoint)) {
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
    nlohmann::json request = {
        {"jsonrpc", "2.0"},
        {"id", "1"},
        {"method", "supported_frequency_profiles"}
    };
    std::string error;
    const auto responseBody = sendHttpPost(endpoint_, request.dump(), error);
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
        return {common::CommandResultCode::Unavailable, response["error"].dump(), std::nullopt};
    }

    if (response.contains("result") && response["result"].is_array()) {
        std::vector<double> freqs;
        for (const auto& profile : response["result"]) {
            if (!profile.is_object()) continue;
            if (!profile.contains("frequencies") || !profile["frequencies"].is_array()) continue;
            for (const auto& entry : profile["frequencies"]) {
                if (!entry.is_string()) continue;
                const auto text = entry.get<std::string>();
                const auto colon = text.find(':');
                if (colon == std::string::npos) {
                    try {
                        freqs.push_back(std::stod(text));
                    } catch (...) {}
                } else {
                    const auto parts = text;
                    const auto first = parts.substr(0, colon);
                    const auto last = parts.substr(parts.rfind(':') + 1);
                    try {
                        const double low = std::stod(first);
                        const double high = std::stod(last);
                        freqs.push_back(low);
                        freqs.push_back(high);
                    } catch (...) {}
                }
            }
        }
        if (!freqs.empty()) {
            std::sort(freqs.begin(), freqs.end());
            freqs.erase(std::unique(freqs.begin(), freqs.end()), freqs.end());
            capabilities_.supported_frequencies_mhz = std::move(freqs);
        }
    }

    state_.status = common::RadioStatus::Ready;
    return {common::CommandResultCode::Ok, {}, {}};
}

common::CommandResult SilvusAdapter::set_power(double watts) {
    std::lock_guard<std::mutex> lock(mutex_);
    const double dbm = std::round(10.0 * std::log10(watts * 1000.0));
    nlohmann::json request = {
        {"jsonrpc", "2.0"},
        {"id", "1"},
        {"method", "power_dBm"},
        {"params", {std::to_string(static_cast<int>(dbm))}}
    };
    std::string error;
    const auto responseBody = sendHttpPost(endpoint_, request.dump(), error);
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
        return {common::CommandResultCode::InvalidRange, response["error"].dump(), std::nullopt};
    }

    state_.power_watts = watts;
    state_.status = common::RadioStatus::Ready;
    return {common::CommandResultCode::Ok, {}, {}};
}

common::CommandResult SilvusAdapter::set_channel(int channel_index,
                                                  double frequency_mhz) {
    std::lock_guard<std::mutex> lock(mutex_);
    nlohmann::json request = {
        {"jsonrpc", "2.0"},
        {"id", "1"},
        {"method", "freq"},
        {"params", {std::to_string(frequency_mhz)}}
    };
    std::string error;
    const auto responseBody = sendHttpPost(endpoint_, request.dump(), error);
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
        return {common::CommandResultCode::InvalidRange, response["error"].dump(), std::nullopt};
    }

    state_.channel_index = channel_index;
    state_.status = common::RadioStatus::Ready;
    return {common::CommandResultCode::Ok, {}, {}};
}

common::CommandResult SilvusAdapter::refresh_state() {
    std::lock_guard<std::mutex> lock(mutex_);
    nlohmann::json powerRequest = {
        {"jsonrpc", "2.0"},
        {"id", "1"},
        {"method", "power_dBm"}
    };
    std::string error;
    const auto powerBody = sendHttpPost(endpoint_, powerRequest.dump(), error);
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
        return {common::CommandResultCode::Unavailable, powerResponse["error"].dump(), std::nullopt};
    }

    if (powerResponse.contains("result") && powerResponse["result"].is_array() && !powerResponse["result"].empty()) {
        const auto value = powerResponse["result"][0].get<std::string>();
        try {
            const int dbm = std::stoi(value);
            state_.power_watts = std::pow(10.0, dbm / 10.0) / 1000.0;
        } catch (...) {
            state_.power_watts.reset();
        }
    }

    nlohmann::json freqRequest = {
        {"jsonrpc", "2.0"},
        {"id", "2"},
        {"method", "freq"}
    };
    const auto freqBody = sendHttpPost(endpoint_, freqRequest.dump(), error);
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
        return {common::CommandResultCode::Unavailable, freqResponse["error"].dump(), std::nullopt};
    }

    if (freqResponse.contains("result") && freqResponse["result"].is_array() && !freqResponse["result"].empty()) {
        const auto value = freqResponse["result"][0].get<std::string>();
        try {
            const double freq = std::stod(value);
            const auto maybeIndex = findChannelIndex(capabilities_, freq);
            if (maybeIndex) {
                state_.channel_index = *maybeIndex;
            } else {
                state_.channel_index.reset();
            }
        } catch (...) {
            state_.channel_index.reset();
        }
    }

    state_.status = common::RadioStatus::Ready;
    return {common::CommandResultCode::Ok, {}, {}};
}

common::RadioState SilvusAdapter::state() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return state_;
}

}  // namespace rcc::adapter
