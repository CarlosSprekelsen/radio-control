#include "rcc/api/api_gateway.hpp"
#include "rcc/api/monitor_page.hpp"
#include "rcc/version.hpp"

#include "rcc/auth/authenticator.hpp"
#include "rcc/command/orchestrator.hpp"
#include "rcc/radio/radio_manager.hpp"
#include "rcc/telemetry/telemetry_hub.hpp"

#include <dts/common/core/timing_profile.hpp>
#include <dts/common/rest/rest_server.hpp>

#include <asio/ip/address.hpp>
#include <asio/ip/tcp.hpp>
#include <nlohmann/json.hpp>
#include <openssl/hmac.h>
#include <openssl/evp.h>
#include <dts/common/health/health_types.hpp>

#include <atomic>
#include <cmath>
#include <chrono>
#include <iostream>
#include <optional>
#include <sstream>
#include <string>
#include <vector>

namespace rcc::api {

// ─── Watts ↔ dBm helpers ─────────────────────────────────────────────────────

static double watts_to_dbm(double watts) {
    if (watts <= 0.0) return -100.0;
    return 10.0 * std::log10(watts * 1000.0);
}

static nlohmann::json build_channels(const std::vector<double>& frequencies) {
    nlohmann::json arr = nlohmann::json::array();
    int idx = 1;
    for (double f : frequencies) {
        arr.push_back({{"index", idx++}, {"frequencyMhz", f}});
    }
    return arr;
}

static std::string map_radio_status(const std::string& internal) {
    if (internal == "ready" || internal == "discovering" || internal == "busy") {
        return "online";
    }
    if (internal == "recovering") {
        return "recovering";
    }
    return "offline";
}

static std::string now_iso8601() {
    using namespace std::chrono;
    const auto now = system_clock::now();
    const auto time = system_clock::to_time_t(now);
    std::tm utc;
    gmtime_r(&time, &utc);
    char buffer[32];
    std::strftime(buffer, sizeof(buffer), "%Y-%m-%dT%H:%M:%SZ", &utc);
    return std::string(buffer);
}

static std::string makeCorrelationId() {
    static std::atomic<uint64_t> counter{0};
    const auto now = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();
    const uint64_t id = counter.fetch_add(1, std::memory_order_relaxed);
    std::ostringstream oss;
    oss << "corr-" << now << "-" << id;
    return oss.str();
}

static std::optional<int> find_channel_index(const adapter::CapabilityInfo& caps,
                                             double frequencyMhz) {
    for (size_t i = 0; i < caps.supported_frequencies_mhz.size(); ++i) {
        if (std::abs(caps.supported_frequencies_mhz[i] - frequencyMhz) < 0.001) {
            return static_cast<int>(i) + 1;
        }
    }
    return std::nullopt;
}

static nlohmann::json build_radio_state(const rcc::radio::RadioDescriptor& desc) {
    const auto state = desc.adapter ? desc.adapter->state() : desc.state;
    const auto caps = desc.adapter ? desc.adapter->capabilities()
                                   : adapter::CapabilityInfo{};

    double frequencyMhz = 0.0;
    if (state.channel_index.has_value() &&
        !caps.supported_frequencies_mhz.empty()) {
        const size_t idx = static_cast<size_t>(state.channel_index.value());
        if (idx >= 1 && idx <= caps.supported_frequencies_mhz.size()) {
            frequencyMhz = caps.supported_frequencies_mhz[idx - 1];
        }
    }

    nlohmann::json result;
    result["powerDbm"] = state.power_watts.has_value()
        ? nlohmann::json(std::round(watts_to_dbm(state.power_watts.value())))
        : nlohmann::json(nullptr);
    result["frequencyMhz"] = frequencyMhz > 0.0
        ? nlohmann::json(frequencyMhz)
        : nlohmann::json(nullptr);
    return result;
}

static nlohmann::json build_radio_item(const rcc::radio::RadioDescriptor& desc) {
    const auto state = desc.adapter ? desc.adapter->state() : desc.state;
    const auto caps = desc.adapter ? desc.adapter->capabilities()
                                   : adapter::CapabilityInfo{};

    nlohmann::json item;
    item["id"] = desc.id;
    item["model"] = "Silvus-" + desc.id;
    item["status"] = map_radio_status(common::to_string(state.status));
    item["capabilities"] = {
        {"channels", build_channels(caps.supported_frequencies_mhz)},
        {"minPowerDbm", std::round(watts_to_dbm(caps.power_range_watts.first))},
        {"maxPowerDbm", std::round(watts_to_dbm(caps.power_range_watts.second))}
    };
    item["state"] = build_radio_state(desc);
    return item;
}

static std::optional<nlohmann::json> find_radio_item(
    const rcc::radio::RadioManager& radioManager,
    const std::string& radioId) {
    const auto radios = radioManager.list_radios();
    for (const auto& desc : radios) {
        if (desc.id == radioId) {
            return build_radio_item(desc);
        }
    }
    return std::nullopt;
}

// ─── JWT / Base64url helpers ──────────────────────────────────────────────────

static std::string b64url(const unsigned char* data, size_t len) {
    static const char T[] =
        "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    std::string out;
    out.reserve(((len + 2) / 3) * 4);
    size_t val = 0; int valb = -6;
    for (size_t i = 0; i < len; ++i) {
        val = (val << 8) + data[i]; valb += 8;
        while (valb >= 0) { out.push_back(T[(val >> valb) & 0x3F]); valb -= 6; }
    }
    if (valb > -6) out.push_back(T[((val << 8) >> (valb + 8)) & 0x3F]);
    for (char& c : out) { if (c == '+') c = '-'; else if (c == '/') c = '_'; }
    return out;
}

static std::string b64url(const std::string& s) {
    return b64url(reinterpret_cast<const unsigned char*>(s.data()), s.size());
}

// Generate a long-lived "operator" JWT for the dev monitor page.
// NOT for production use — the endpoint that serves this is unauthenticated.
static std::string makeDevToken(const std::string& secret) {
    using namespace std::chrono;
    const auto now = system_clock::now();
    const long iat = duration_cast<seconds>(now.time_since_epoch()).count();
    const long exp = iat + 86400L * 365;   // 1 year

    const std::string hdr = b64url(
        nlohmann::json{{"alg","HS256"},{"typ","JWT"}}.dump());
    const std::string pay = b64url(
        nlohmann::json{{"sub","dev-monitor"},{"scope","operator"},
                       {"iat",iat},{"exp",exp}}.dump());
    const std::string msg = hdr + "." + pay;

    unsigned int sigLen = EVP_MAX_MD_SIZE;
    unsigned char sig[EVP_MAX_MD_SIZE];
    HMAC(EVP_sha256(), secret.data(), static_cast<int>(secret.size()),
         reinterpret_cast<const unsigned char*>(msg.data()), msg.size(),
         sig, &sigLen);

    return msg + "." + b64url(sig, sigLen);
}

// ─── HTTP response helpers ────────────────────────────────────────────────────

static std::string jsonResponse(int status, const std::string& statusText,
                                nlohmann::json body) {
    if (!body.contains("correlationId")) {
        body["correlationId"] = makeCorrelationId();
    }
    const std::string bodyStr = body.dump();
    std::ostringstream oss;
    oss << "HTTP/1.1 " << status << " " << statusText << "\r\n"
        << "Content-Type: application/json; charset=utf-8\r\n"
        << "Content-Length: " << bodyStr.size() << "\r\n"
        << "Cache-Control: no-cache\r\n"
        << "\r\n" << bodyStr;
    return oss.str();
}

static std::string okJson(const nlohmann::json& data = {}) {
    nlohmann::json body = {{"result", "ok"}};
    if (!data.is_null()) body["data"] = data;
    return jsonResponse(200, "OK", body);
}

static std::string errJson(int status, const std::string& statusText,
                           const std::string& message) {
    return jsonResponse(status, statusText,
                        {{"result", "error"}, {"message", message}});
}

static std::string errJson(int status, const std::string& statusText,
                           const std::string& code,
                           const std::string& message) {
    return jsonResponse(status, statusText,
                        {{"result", "error"}, {"code", code}, {"message", message}});
}

static std::string htmlResponse(std::string_view html) {
    std::ostringstream oss;
    oss << "HTTP/1.1 200 OK\r\n"
        << "Content-Type: text/html; charset=utf-8\r\n"
        << "Content-Length: " << html.size() << "\r\n"
        << "Cache-Control: no-cache\r\n"
        << "\r\n" << html;
    return oss.str();
}

static std::string commandCodeToSpec(command::CommandResult::Code code) {
    switch (code) {
        case command::CommandResult::Code::InvalidRange: return "INVALID_RANGE";
        case command::CommandResult::Code::Busy:         return "BUSY";
        case command::CommandResult::Code::Unavailable:  return "UNAVAILABLE";
        case command::CommandResult::Code::Unauthorized: return "UNAUTHORIZED";
        case command::CommandResult::Code::Ok:           return "OK";
        case command::CommandResult::Code::InternalError: return "INTERNAL";
        case command::CommandResult::Code::NotFound:     return "NOT_FOUND";
    }
    return "INTERNAL";
}

static int commandCodeToHttp(command::CommandResult::Code code) {
    switch (code) {
        case command::CommandResult::Code::InvalidRange: return 400;
        case command::CommandResult::Code::Busy:
        case command::CommandResult::Code::Unavailable:  return 503;
        case command::CommandResult::Code::Unauthorized: return 401;
        case command::CommandResult::Code::NotFound:     return 404;
        case command::CommandResult::Code::Ok:           return 200;
        case command::CommandResult::Code::InternalError: return 500;
    }
    return 500;
}

// ─── ApiGateway::Impl ─────────────────────────────────────────────────────────

class ApiGateway::Impl {
public:
    Impl(asio::io_context& io,
         auth::Authenticator& auth,
         command::Orchestrator& orchestrator,
         radio::RadioManager& radioManager,
         telemetry::TelemetryHub& /*telemetry*/,
         uint16_t restPort,
         std::string tokenSecret)
        : auth_{auth}
        , orchestrator_{orchestrator}
        , radioManager_{radioManager}
        , tokenSecret_{std::move(tokenSecret)}
        , timing_{}
        , restServer_{std::make_unique<dts::common::rest::RestServer>(
              io,
              asio::ip::tcp::endpoint{asio::ip::address_v4::any(), restPort},
              timing_)}
    {
        registerRoutes();
    }

    void start() {
        restServer_->start();
        std::cout << "[ApiGateway] REST server starting on port "
                  << restPort() << std::endl;
    }

    void stop() {
        restServer_->stop();
        std::cout << "[ApiGateway] REST server stopped" << std::endl;
    }

    bool awaitListening(std::chrono::milliseconds timeout) {
        return restServer_->awaitListening(timeout);
    }

private:
    uint16_t restPort() const { return restServer_->port(); }

    auth::AuthResult checkAuth(const dts::common::rest::HttpRequest& req,
                               auth::AccessLevel level) {
        return auth_.authorize(req, level);
    }

    void registerRoutes() {
        auto& router = restServer_->router();

        // Monitor page — no auth (browser direct navigation)
        router.addRoute("/monitor", [](const dts::common::rest::HttpRequest&) {
            return htmlResponse(RADIO_MONITOR_HTML);
        });

        // Dev token — unauthenticated, for the test monitor page only
        router.addRoute("/api/v1/dev-token",
            [this](const dts::common::rest::HttpRequest& req) {
                if (req.method != "GET") return errJson(405, "Method Not Allowed", "Use GET");
                if (tokenSecret_.empty())
                    return errJson(503, "Service Unavailable",
                                   "token_secret not configured — set it in config.yaml");
                const std::string token = makeDevToken(tokenSecret_);
                return okJson({{"token", token}, {"scope", "operator"},
                               {"note", "dev/test token — not for production"}});
            });

        // Health — no auth, reflects actual radio subsystem state
        router.addRoute("/api/v1/health", [this](const dts::common::rest::HttpRequest& req) {
            if (req.method != "GET") return errJson(405, "Method Not Allowed", "Use GET");

            const auto radios = radioManager_.list_radios();
            bool anyReady = false;
            bool adapterConnected = false;
            for (const auto& desc : radios) {
                const auto st = desc.adapter ? desc.adapter->state() : desc.state;
                if (st.status != common::RadioStatus::Offline) {
                    adapterConnected = true;
                }
                if (st.status == common::RadioStatus::Ready) {
                    anyReady = true;
                }
            }

            dts::common::health::HealthSnapshot snapshot;
            snapshot.container = dts::common::health::calculateHealthStatus(
                anyReady ? dts::common::health::DeviceState::Ready
                         : dts::common::health::DeviceState::Disconnected,
                true,  // heartbeat not tracked yet
                true,  // ring buffer not tracked yet
                adapterConnected);
            snapshot.device = anyReady ? dts::common::health::DeviceState::Ready
                                       : dts::common::health::DeviceState::Disconnected;
            snapshot.telemetry = dts::common::health::ComponentCheckStatus::Ok;
            snapshot.adapter = adapterConnected
                ? dts::common::health::ComponentCheckStatus::Connected
                : dts::common::health::ComponentCheckStatus::Disconnected;
            snapshot.uptimeSec = 0;
            snapshot.measurementCount = radios.size();
            snapshot.lastSeqId = 0;
            snapshot.ringFillRatio = 0.0;
            snapshot.lastHeartbeatIso = "";
            snapshot.lastEventIso = "";
            snapshot.nowIso = now_iso8601();
            snapshot.containerVersion = std::string(rcc::version());
            snapshot.buildTime = std::string(rcc::build_timestamp());
            snapshot.compiler = std::string("unknown");

            return jsonResponse(200, "OK", dts::common::health::to_json(snapshot));
        });

        // Capabilities
        router.addRoute("/api/v1/capabilities",
            [this](const dts::common::rest::HttpRequest& req) {
                if (req.method != "GET") return errJson(405, "Method Not Allowed", "Use GET");
                const auto ar = checkAuth(req, auth::AccessLevel::Telemetry);
                if (!ar.allowed) return errJson(401, "Unauthorized", ar.message);
                return okJson({{"telemetry", {"sse"}},
                               {"commands", {"http-json"}},
                               {"version", "1.0.0"}});
            });

        // GET /api/v1/radios — viewer level
        router.addRoute("/api/v1/radios",
            [this](const dts::common::rest::HttpRequest& req) {
                if (req.method != "GET") return errJson(405, "Method Not Allowed", "Use GET");
                const auto ar = checkAuth(req, auth::AccessLevel::Telemetry);
                if (!ar.allowed) return errJson(401, "Unauthorized", ar.message);
                return buildRadioListResponse();
            });

        // POST /api/v1/radios/select — operator level
        router.addRoute("/api/v1/radios/select",
            [this](const dts::common::rest::HttpRequest& req) {
                if (req.method != "POST") return errJson(405, "Method Not Allowed", "Use POST");
                const auto ar = checkAuth(req, auth::AccessLevel::Control);
                if (!ar.allowed) return errJson(401, "Unauthorized", ar.message);
                return handleSelect(req);
            });

        // Prefix route for /api/v1/radios/{id} and its subresources.
        router.addPrefixRoute("/api/v1/radios/",
            [this](const dts::common::rest::HttpRequest& req) {
                return handleRadiosPrefix(req);
            });
    }

    // ─── Radios list (v1 spec) ─────────────────────────────────────────────────
    std::string buildRadioListResponse() {
        const auto descriptors = radioManager_.list_radios();
        const auto activeId    = radioManager_.active_radio();

        nlohmann::json items = nlohmann::json::array();
        for (const auto& desc : descriptors) {
            items.push_back(build_radio_item(desc));
        }

        return okJson({
            {"activeRadioId", activeId.has_value() ? nlohmann::json(*activeId) : nlohmann::json(nullptr)},
            {"items", items}
        });
    }

    std::string handleGetRadio(const dts::common::rest::HttpRequest& req,
                               const std::string& radioId) {
        (void)req;
        const auto item = find_radio_item(radioManager_, radioId);
        if (!item.has_value()) {
            return errJson(404, "Not Found", "NOT_FOUND", "Radio not found");
        }
        return okJson(*item);
    }

    std::string handleGetRadioPower(const dts::common::rest::HttpRequest& req,
                                    const std::string& radioId) {
        (void)req;
        const auto adapter = radioManager_.get_adapter(radioId);
        if (!adapter) {
            return errJson(404, "Not Found", "NOT_FOUND", "Radio not found");
        }
        const auto state = adapter->state();
        nlohmann::json data;
        data["powerDbm"] = state.power_watts.has_value()
            ? nlohmann::json(std::round(watts_to_dbm(state.power_watts.value())))
            : nlohmann::json(nullptr);
        return okJson(data);
    }

    std::string handleGetRadioChannel(const dts::common::rest::HttpRequest& req,
                                      const std::string& radioId) {
        (void)req;
        const auto adapter = radioManager_.get_adapter(radioId);
        if (!adapter) {
            return errJson(404, "Not Found", "NOT_FOUND", "Radio not found");
        }
        const auto state = adapter->state();
        const auto caps = adapter->capabilities();
        double frequencyMhz = 0.0;
        if (state.channel_index.has_value() && !caps.supported_frequencies_mhz.empty()) {
            const size_t idx = static_cast<size_t>(state.channel_index.value());
            if (idx >= 1 && idx <= caps.supported_frequencies_mhz.size()) {
                frequencyMhz = caps.supported_frequencies_mhz[idx - 1];
            }
        }
        nlohmann::json data;
        data["channelIndex"] = state.channel_index.has_value()
            ? nlohmann::json(state.channel_index.value()) : nlohmann::json(nullptr);
        data["frequencyMhz"] = frequencyMhz > 0.0
            ? nlohmann::json(frequencyMhz) : nlohmann::json(nullptr);
        return okJson(data);
    }

    // ─── Prefix handler: /api/v1/radios/{id}/power | /api/v1/radios/{id}/channel
    std::string handleRadiosPrefix(const dts::common::rest::HttpRequest& req) {
        // req.path starts with "/api/v1/radios/"
        std::string suffix = req.path.substr(std::string("/api/v1/radios/").size());
        if (suffix.empty()) {
            return errJson(404, "Not Found", "NOT_FOUND", "Invalid radio path");
        }

        auto slashPos = suffix.find('/');
        if (slashPos == std::string::npos) {
            const auto ar = checkAuth(req, auth::AccessLevel::Telemetry);
            if (!ar.allowed) return errJson(401, "Unauthorized", ar.message);
            if (req.method != "GET") return errJson(405, "Method Not Allowed", "Use GET");
            return handleGetRadio(req, suffix);
        }

        std::string radioId = suffix.substr(0, slashPos);
        std::string sub     = suffix.substr(slashPos + 1);
        if (sub == "power" || sub == "channel") {
            if (req.method == "GET") {
                const auto ar = checkAuth(req, auth::AccessLevel::Telemetry);
                if (!ar.allowed) return errJson(401, "Unauthorized", ar.message);
            } else {
                const auto ar = checkAuth(req, auth::AccessLevel::Control);
                if (!ar.allowed) return errJson(401, "Unauthorized", ar.message);
            }
        } else {
            return errJson(404, "Not Found", "NOT_FOUND", "Unknown sub-resource: " + sub);
        }

        if (sub == "power") {
            if (req.method == "GET") return handleGetRadioPower(req, radioId);
            if (req.method != "POST") return errJson(405, "Method Not Allowed", "Use POST");
            return handleSetPower(req, radioId);
        }
        if (sub == "channel") {
            if (req.method == "GET") return handleGetRadioChannel(req, radioId);
            if (req.method != "POST") return errJson(405, "Method Not Allowed", "Use POST");
            return handleSetChannel(req, radioId);
        }
        return errJson(404, "Not Found", "NOT_FOUND", "Unknown sub-resource: " + sub);
    }

    // ─── Command handlers ──────────────────────────────────────────────────────
    std::string handleSelect(const dts::common::rest::HttpRequest& req) {
        nlohmann::json body;
        try { body = nlohmann::json::parse(req.body); }
        catch (...) { return errJson(400, "Bad Request", "BAD_REQUEST", "Invalid JSON body"); }

        const auto radioId = body.value("id", "");
        if (radioId.empty()) return errJson(400, "Bad Request", "BAD_REQUEST", "Missing id");

        const auto result = orchestrator_.selectRadio(radioId);
        if (result.code == command::CommandResult::Code::Ok)
            return okJson({{"activeRadioId", radioId}});
        return errJson(commandCodeToHttp(result.code), "Error",
                       commandCodeToSpec(result.code), result.message);
    }

    std::string handleSetPower(const dts::common::rest::HttpRequest& req,
                               const std::string& radioId) {
        nlohmann::json body;
        try { body = nlohmann::json::parse(req.body); }
        catch (...) { return errJson(400, "Bad Request", "BAD_REQUEST", "Invalid JSON body"); }

        if (!body.contains("powerDbm") || !body["powerDbm"].is_number())
            return errJson(400, "Bad Request", "BAD_REQUEST", "Missing or invalid powerDbm");

        const double dbm = body["powerDbm"].get<double>();
        if (dbm < 0.0 || dbm > 39.0) {
            return errJson(400, "Bad Request", "INVALID_RANGE",
                           "Power must be between 0 and 39 dBm");
        }

        const double watts = std::pow(10.0, dbm / 10.0) / 1000.0;
        const auto result  = orchestrator_.setPower(radioId, watts);
        if (result.code == command::CommandResult::Code::Ok)
            return okJson({{"powerDbm", dbm}});
        return errJson(commandCodeToHttp(result.code), "Error",
                       commandCodeToSpec(result.code), result.message);
    }

    std::string handleSetChannel(const dts::common::rest::HttpRequest& req,
                                 const std::string& radioId) {
        nlohmann::json body;
        try { body = nlohmann::json::parse(req.body); }
        catch (...) { return errJson(400, "Bad Request", "BAD_REQUEST", "Invalid JSON body"); }

        const bool hasIndex = body.contains("channelIndex") && body["channelIndex"].is_number_integer();
        const bool hasFrequency = body.contains("frequencyMhz") && body["frequencyMhz"].is_number();
        if (!hasIndex && !hasFrequency) {
            return errJson(400, "Bad Request", "BAD_REQUEST",
                           "Missing channelIndex or frequencyMhz");
        }

        const auto adapter = radioManager_.get_adapter(radioId);
        if (!adapter) {
            return errJson(404, "Not Found", "NOT_FOUND", "Radio not found");
        }

        int channelIndex = -1;
        double frequencyMhz = 0.0;
        const auto caps = adapter->capabilities();

        if (hasIndex) {
            channelIndex = body["channelIndex"].get<int>();
            if (channelIndex < 1 || channelIndex > static_cast<int>(caps.supported_frequencies_mhz.size())) {
                return errJson(400, "Bad Request", "INVALID_RANGE",
                               "channelIndex is outside supported range");
            }
            frequencyMhz = caps.supported_frequencies_mhz[static_cast<size_t>(channelIndex) - 1];
        }

        if (hasFrequency) {
            frequencyMhz = body["frequencyMhz"].get<double>();
            const auto maybeIndex = find_channel_index(caps, frequencyMhz);
            if (!maybeIndex) {
                return errJson(400, "Bad Request", "INVALID_RANGE",
                               "frequencyMhz is not supported");
            }
            if (hasIndex && channelIndex != *maybeIndex) {
                return errJson(400, "Bad Request", "INVALID_RANGE",
                               "channelIndex and frequencyMhz do not match");
            }
            channelIndex = *maybeIndex;
        }

        const auto result = orchestrator_.setChannel(radioId, channelIndex);
        if (result.code == command::CommandResult::Code::Ok) {
            return okJson({{"channelIndex", channelIndex},
                           {"frequencyMhz", frequencyMhz > 0.0 ? nlohmann::json(frequencyMhz)
                                                                : nlohmann::json(nullptr)}});
        }
        return errJson(commandCodeToHttp(result.code), "Error",
                       commandCodeToSpec(result.code), result.message);
    }

    // ─── Members ─────────────────────────────────────────────────────────────
    auth::Authenticator&   auth_;
    command::Orchestrator& orchestrator_;
    radio::RadioManager&   radioManager_;
    std::string            tokenSecret_;
    dts::common::core::TimingProfile timing_;   // must outlive restServer_
    std::unique_ptr<dts::common::rest::RestServer> restServer_;
};

// ─── ApiGateway public interface ─────────────────────────────────────────────

ApiGateway::ApiGateway(asio::io_context& io,
                       auth::Authenticator& authenticator,
                       command::Orchestrator& orchestrator,
                       radio::RadioManager& radioManager,
                       telemetry::TelemetryHub& telemetry,
                       uint16_t restPort,
                       std::string tokenSecret)
    : impl_{std::make_unique<Impl>(io, authenticator, orchestrator,
                                   radioManager, telemetry,
                                   restPort, std::move(tokenSecret))}
{}

ApiGateway::~ApiGateway() = default;
void ApiGateway::start() { impl_->start(); }
void ApiGateway::stop()  { impl_->stop(); }
bool ApiGateway::awaitListening(std::chrono::milliseconds timeout) {
    return impl_->awaitListening(timeout);
}

}  // namespace rcc::api
