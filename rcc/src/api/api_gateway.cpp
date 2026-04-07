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

#include <chrono>
#include <iostream>
#include <sstream>
#include <string>

namespace rcc::api {

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
                                const nlohmann::json& body) {
    const std::string bodyStr = body.dump();
    std::ostringstream oss;
    oss << "HTTP/1.1 " << status << " " << statusText << "\r\n"
        << "Content-Type: application/json\r\n"
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

static std::string htmlResponse(std::string_view html) {
    std::ostringstream oss;
    oss << "HTTP/1.1 200 OK\r\n"
        << "Content-Type: text/html; charset=utf-8\r\n"
        << "Content-Length: " << html.size() << "\r\n"
        << "Cache-Control: no-cache\r\n"
        << "\r\n" << html;
    return oss.str();
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
        , timing_{}   // must be declared before restServer_ (holds reference)
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
        // Returns a 1-year operator-scope JWT so the page can use SSE and control APIs.
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
            const auto activeId = radioManager_.active_radio();

            // Determine overall status from radio states
            std::string status = "ready";
            bool anyReady = false;
            bool allOffline = true;
            for (const auto& desc : radios) {
                const auto st = desc.adapter ? desc.adapter->state() : desc.state;
                if (st.status != common::RadioStatus::Offline) allOffline = false;
                if (st.status == common::RadioStatus::Ready) anyReady = true;
            }
            if (radios.empty()) {
                status = "degraded";
            } else if (allOffline) {
                status = "degraded";
            } else if (!anyReady) {
                status = "initializing";
            }

            nlohmann::json body;
            body["status"] = status;
            body["radioCount"] = radios.size();
            body["service"] = "radio-control-container";
            body["version"] = std::string(rcc::version());
            body["gitVersion"] = std::string(rcc::git_revision());
            body["buildDate"] = std::string(rcc::build_timestamp());
            if (activeId.has_value()) body["activeRadio"] = *activeId;
            return okJson(body);
        });

        // GET /api/v1/radios — viewer level
        router.addRoute("/api/v1/radios",
            [this](const dts::common::rest::HttpRequest& req) {
                if (req.method != "GET") return errJson(405, "Method Not Allowed", "Use GET");
                const auto ar = checkAuth(req, auth::AccessLevel::Telemetry);
                if (!ar.allowed) return errJson(401, "Unauthorized", ar.message);
                return buildRadioListResponse();
            });

        // POST /api/v1/radio/connect — operator level
        router.addRoute("/api/v1/radio/connect",
            [this](const dts::common::rest::HttpRequest& req) {
                if (req.method != "POST") return errJson(405, "Method Not Allowed", "Use POST");
                const auto ar = checkAuth(req, auth::AccessLevel::Control);
                if (!ar.allowed) return errJson(401, "Unauthorized", ar.message);
                return handleConnect(req);
            });

        // PUT /api/v1/radio/power — operator level
        router.addRoute("/api/v1/radio/power",
            [this](const dts::common::rest::HttpRequest& req) {
                if (req.method != "PUT") return errJson(405, "Method Not Allowed", "Use PUT");
                const auto ar = checkAuth(req, auth::AccessLevel::Control);
                if (!ar.allowed) return errJson(401, "Unauthorized", ar.message);
                return handleSetPower(req);
            });

        // PUT /api/v1/radio/channel — operator level
        router.addRoute("/api/v1/radio/channel",
            [this](const dts::common::rest::HttpRequest& req) {
                if (req.method != "PUT") return errJson(405, "Method Not Allowed", "Use PUT");
                const auto ar = checkAuth(req, auth::AccessLevel::Control);
                if (!ar.allowed) return errJson(401, "Unauthorized", ar.message);
                return handleSetChannel(req);
            });
    }

    // ─── Radios list ─────────────────────────────────────────────────────────
    std::string buildRadioListResponse() {
        const auto descriptors = radioManager_.list_radios();
        const auto activeId    = radioManager_.active_radio();

        nlohmann::json radiosArr = nlohmann::json::array();
        for (const auto& desc : descriptors) {
            const auto state = desc.adapter ? desc.adapter->state() : desc.state;
            const auto caps  = desc.adapter ? desc.adapter->capabilities()
                                            : adapter::CapabilityInfo{};

            nlohmann::json freqs = nlohmann::json::array();
            for (double f : caps.supported_frequencies_mhz) freqs.push_back(f);

            nlohmann::json r;
            r["id"]           = desc.id;
            r["adapterType"]  = desc.adapter_type;
            r["status"]       = common::to_string(state.status);
            r["channelIndex"] = state.channel_index.has_value()
                ? nlohmann::json(state.channel_index.value()) : nlohmann::json(nullptr);
            r["powerWatts"]   = state.power_watts.has_value()
                ? nlohmann::json(state.power_watts.value()) : nlohmann::json(nullptr);
            r["capabilities"] = {
                {"frequenciesMhz", freqs},
                {"powerRangeWatts", {
                    {"min", caps.power_range_watts.first},
                    {"max", caps.power_range_watts.second}
                }}
            };
            radiosArr.push_back(r);
        }

        return okJson({
            {"active", activeId.has_value() ? nlohmann::json(*activeId) : nlohmann::json(nullptr)},
            {"radios", radiosArr}
        });
    }

    // ─── Command handlers ─────────────────────────────────────────────────────
    std::string handleConnect(const dts::common::rest::HttpRequest& req) {
        nlohmann::json body;
        try { body = nlohmann::json::parse(req.body); }
        catch (...) { return errJson(400, "Bad Request", "Invalid JSON body"); }

        const auto radioId = body.value("radioId", "");
        if (radioId.empty()) return errJson(400, "Bad Request", "Missing radioId");

        const auto result = orchestrator_.selectRadio(radioId);
        if (result.code == command::CommandResult::Code::Ok)
            return okJson({{"message", "connect accepted"}});
        return errJson(400, "Bad Request", result.message);
    }

    std::string handleSetPower(const dts::common::rest::HttpRequest& req) {
        nlohmann::json body;
        try { body = nlohmann::json::parse(req.body); }
        catch (...) { return errJson(400, "Bad Request", "Invalid JSON body"); }

        const auto radioId = body.value("radioId", "");
        if (radioId.empty()) return errJson(400, "Bad Request", "Missing radioId");
        if (!body.contains("watts") || !body["watts"].is_number())
            return errJson(400, "Bad Request", "Missing or invalid watts");

        const double watts = body["watts"].get<double>();
        const auto result  = orchestrator_.setPower(radioId, watts);
        if (result.code == command::CommandResult::Code::Ok)
            return okJson({{"message", "power set"}, {"watts", watts}});
        return errJson(400, "Bad Request", result.message);
    }

    std::string handleSetChannel(const dts::common::rest::HttpRequest& req) {
        nlohmann::json body;
        try { body = nlohmann::json::parse(req.body); }
        catch (...) { return errJson(400, "Bad Request", "Invalid JSON body"); }

        const auto radioId = body.value("radioId", "");
        if (radioId.empty()) return errJson(400, "Bad Request", "Missing radioId");
        if (!body.contains("channelIndex") || !body["channelIndex"].is_number_integer())
            return errJson(400, "Bad Request", "Missing or invalid channelIndex");

        const int channelIndex     = body["channelIndex"].get<int>();
        const double frequencyMhz  = body.value("frequencyMhz", 0.0);
        const auto result          = orchestrator_.setChannel(radioId, channelIndex);
        if (result.code == command::CommandResult::Code::Ok)
            return okJson({{"message", "channel set"},
                           {"channelIndex", channelIndex},
                           {"frequencyMhz", frequencyMhz}});
        return errJson(400, "Bad Request", result.message);
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
