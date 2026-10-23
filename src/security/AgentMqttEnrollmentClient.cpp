#include "AgentMqttEnrollmentClient.h"
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

bool AgentMqttEnrollmentClient::provision()
{
#if EDOL_AGENT_SECURE_BUILD
    AgentIdentity identity;
    String host = ConfigManager::getCoreHost();
    if (!AgentCredentialStore::load(identity) || !PairingProtocol::validHost(host.c_str())
        || ConfigManager::getCorePort() == 0 || !SecureTransport::synchronizeClock()) return false;
    SecureTransport::Client transport;
    if (!SecureTransport::prepare(transport)) return false;
    HTTPClient http;
    auto close = [&]() { http.end(); transport.stop(); };
    ScopeExit<decltype(close)> cleanup(close);
    String url = String("https://") + host + ":" + ConfigManager::getCorePort() + "/api/agent/v1/mqtt/credentials";
    if (!http.begin(transport, url)) return false;
    http.setReuse(false);
    http.setConnectTimeout(10000);
    http.setTimeout(10000);
    http.setFollowRedirects(HTTPC_DISABLE_FOLLOW_REDIRECTS);
    http.addHeader("Authorization", String("EdolAgent ") + identity.id + "." + identity.secret);
    // A lost response cannot retrieve this secret. Owner replacement is required, including after restart.
    if (!AgentCredentialStore::beginMqttProvisioning()) return false;
    int status = http.POST("");
    Serial.printf("MQTT provisioning HTTP status=%d\n", status);
    if (status != 200)
    {
#if EDOL_AGENT_SMOKE_BUILD
        if (status < 0) SecureTransport::reportFailure(transport, "mqtt-credentials", host.c_str(), ConfigManager::getCorePort());
#endif
        return false;
    }
    BoundedResponse response;
    if (http.writeToStream(&response) < 0 || response.failed()) return false;
    JsonDocument document;
    if (deserializeJson(document, response.data(), response.size())) return false;
    return AgentCredentialStore::saveMqtt(document["mqttSecret"].as<const char *>());
#else
    return false;
#endif
}
