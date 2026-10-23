#include "AgentEnrollmentClient.h"
#include "SecureConfig.h"

#if EDOL_AGENT_SECURE_BUILD
#include <ArduinoJson.h>
#include <HTTPClient.h>
#include "AgentCredentialStore.h"
#include "BoundedResponse.h"
#include "PairingProtocol.h"
#include "SecureTransport.h"
#include "../config/ConfigManager.h"
#include "../util/ScopeExit.h"
#endif

bool AgentEnrollmentClient::enroll(const char *code)
{
    return enrollDetailed(code) == ProvisioningResult::Success;
}

ProvisioningResult AgentEnrollmentClient::enrollDetailed(const char *code)
{
#if EDOL_AGENT_SECURE_BUILD
    String host = ConfigManager::getCoreHost();
    if (!PairingProtocol::validCode(code)) return ProvisioningResult::CodeRejected;
    if (!PairingProtocol::validHost(host.c_str()) || ConfigManager::getCorePort() == 0)
        return ProvisioningResult::InvalidEndpoint;
    if (!SecureTransport::synchronizeClock()) return ProvisioningResult::ClockTimeout;
    SecureTransport::Client transport;
    if (!SecureTransport::prepare(transport)) return ProvisioningResult::ClockTimeout;
#if EDOL_AGENT_SMOKE_BUILD
    SecureTransport::reportClock("enrollment");
#endif
    HTTPClient http;
    auto close = [&]() { http.end(); transport.stop(); };
    ScopeExit<decltype(close)> cleanup(close);
    String url = String("https://") + host + ":" + ConfigManager::getCorePort() + "/api/agent/v1/enroll";
    if (!http.begin(transport, url)) return ProvisioningResult::InvalidEndpoint;
    http.setReuse(false);
    http.setConnectTimeout(10000);
    http.setTimeout(10000);
    http.setFollowRedirects(HTTPC_DISABLE_FOLLOW_REDIRECTS);
    http.addHeader("Content-Type", "application/json");
    // Establish verified TLS before sending the one-time code. Pre-send failures are retryable.
    if (!transport.connect(host.c_str(), ConfigManager::getCorePort()))
    {
#if EDOL_AGENT_SMOKE_BUILD
        SecureTransport::reportFailure(transport, "enrollment-tls", host.c_str(), ConfigManager::getCorePort());
#endif
        return ProvisioningResult::TlsFailed;
    }
    String request = String("{\"pairingCode\":\"") + code + "\"}";
    const int status = http.POST(request);
    Serial.printf("Enrollment HTTP status=%d\n", status);
    if (status != 200)
    {
        if (status < 0)
        {
            char ignored[1];
            Serial.printf("Secure TLS error=%d\n", transport.lastError(ignored, sizeof(ignored)));
#if EDOL_AGENT_SMOKE_BUILD
            SecureTransport::reportFailure(transport, "enrollment", host.c_str(), ConfigManager::getCorePort());
#endif
        }
        if (status == 429) return ProvisioningResult::RateLimited;
        if (status == 400 || status == 401 || status == 403 || status == 410)
            return ProvisioningResult::CodeRejected;
        return ProvisioningResult::EnrollmentUncertain;
    }
    BoundedResponse response;
    if (http.writeToStream(&response) < 0 || response.failed()) return ProvisioningResult::EnrollmentUncertain;
    JsonDocument document;
    if (deserializeJson(document, response.data(), response.size())) return ProvisioningResult::EnrollmentUncertain;
    const char *id = document["agentId"];
    const char *secret = document["secret"];
    return AgentCredentialStore::save(id, secret) ? ProvisioningResult::Success : ProvisioningResult::EnrollmentUncertain;
#else
    (void)code;
    return ProvisioningResult::CodeRejected;
#endif
}
