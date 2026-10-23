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
    return provisionDetailed() == ProvisioningResult::Success;
}

ProvisioningResult AgentMqttEnrollmentClient::provisionDetailed()
{
#if EDOL_AGENT_SECURE_BUILD
    AgentIdentity identity;
    String host = ConfigManager::getCoreHost();
    if (!AgentCredentialStore::load(identity)) return ProvisioningResult::CodeRejected;
    if (AgentCredentialStore::mqttProvisioningAttempted()) return ProvisioningResult::MqttUncertain;
    if (!PairingProtocol::validHost(host.c_str()) || ConfigManager::getCorePort() == 0)
        return ProvisioningResult::InvalidEndpoint;
    if (!SecureTransport::synchronizeClock()) return ProvisioningResult::ClockTimeout;
    SecureTransport::Client transport;
    if (!SecureTransport::prepare(transport)) return ProvisioningResult::ClockTimeout;
    HTTPClient http;
    auto close = [&]() { http.end(); transport.stop(); };
    ScopeExit<decltype(close)> cleanup(close);
    String url = String("https://") + host + ":" + ConfigManager::getCorePort() + "/api/agent/v1/mqtt/credentials";
    if (!http.begin(transport, url)) return ProvisioningResult::InvalidEndpoint;
    http.setReuse(false);
    http.setConnectTimeout(10000);
    http.setTimeout(10000);
    http.setFollowRedirects(HTTPC_DISABLE_FOLLOW_REDIRECTS);
    http.addHeader("Authorization", String("EdolAgent ") + identity.id + "." + identity.secret);
    // A failed TLS handshake cannot issue authority. Do not burn the one-time marker yet.
    if (!transport.connect(host.c_str(), ConfigManager::getCorePort()))
    {
#if EDOL_AGENT_SMOKE_BUILD
        SecureTransport::reportFailure(transport, "mqtt-credentials-tls", host.c_str(), ConfigManager::getCorePort());
#endif
        return ProvisioningResult::TlsFailed;
    }
    // After this point a lost response requires owner replacement, including after restart.
    if (!AgentCredentialStore::beginMqttProvisioning()) return ProvisioningResult::MqttUncertain;
    int status = http.POST("");
    Serial.printf("MQTT provisioning HTTP status=%d\n", status);
    if (status != 200)
    {
#if EDOL_AGENT_SMOKE_BUILD
        if (status < 0) SecureTransport::reportFailure(transport, "mqtt-credentials", host.c_str(), ConfigManager::getCorePort());
#endif
        return ProvisioningResult::MqttUncertain;
    }
    BoundedResponse response;
    if (http.writeToStream(&response) < 0 || response.failed()) return ProvisioningResult::MqttUncertain;
    JsonDocument document;
    if (deserializeJson(document, response.data(), response.size())) return ProvisioningResult::MqttUncertain;
    return AgentCredentialStore::saveMqtt(document["mqttSecret"].as<const char *>())
        ? ProvisioningResult::Success : ProvisioningResult::MqttUncertain;
#else
    return ProvisioningResult::MqttUncertain;
#endif
}
