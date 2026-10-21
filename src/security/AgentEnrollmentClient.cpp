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
#if EDOL_AGENT_SECURE_BUILD
    String host = ConfigManager::getCoreHost();
    if (!PairingProtocol::validCode(code) || !PairingProtocol::validHost(host.c_str())
        || ConfigManager::getCorePort() == 0 || !SecureTransport::synchronizeClock()) return false;
    WiFiClientSecure transport;
    if (!SecureTransport::prepare(transport)) return false;
    HTTPClient http;
    auto close = [&]() { http.end(); transport.stop(); };
    ScopeExit<decltype(close)> cleanup(close);
    String url = String("https://") + host + ":" + ConfigManager::getCorePort() + "/api/agent/v1/enroll";
    if (!http.begin(transport, url)) return false;
    http.setReuse(false);
    http.setConnectTimeout(10000);
    http.setTimeout(10000);
    http.setFollowRedirects(HTTPC_DISABLE_FOLLOW_REDIRECTS);
    http.addHeader("Content-Type", "application/json");
    String request = String("{\"pairingCode\":\"") + code + "\"}";
    const int status = http.POST(request);
    Serial.printf("Enrollment HTTP status=%d\n", status);
    if (status != 200)
    {
        if (status < 0)
        {
            char ignored[1];
            Serial.printf("Secure TLS error=%d\n", transport.lastError(ignored, sizeof(ignored)));
        }
        return false;
    }
    BoundedResponse response;
    if (http.writeToStream(&response) < 0 || response.failed()) return false;
    JsonDocument document;
    if (deserializeJson(document, response.data(), response.size())) return false;
    const char *id = document["agentId"];
    const char *secret = document["secret"];
    return AgentCredentialStore::save(id, secret);
#else
    (void)code;
    return false;
#endif
}
