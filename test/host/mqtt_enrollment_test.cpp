#include <cassert>
#include <iostream>
#include <HTTPClient.h>
#include "security/AgentMqttEnrollmentClient.h"
#include "security/AgentCredentialStore.h"
#include "security/PairingProtocol.h"
#include "security/SecureTransport.h"
#include "config/ConfigManager.h"

FakeSerial Serial;
FakeSocket socketState;
FakeHttpState httpState;
uint32_t fakeMillis = 0;
bool attempted = false, stored = false, clockReady = true;
String ConfigManager::getCoreHost() { return "core.example.invalid"; }
uint16_t ConfigManager::getCorePort() { return 443; }
bool SecureTransport::synchronizeClock() { return clockReady; }
bool SecureTransport::prepare(SecureTransport::Client &) { return true; }
bool AgentCredentialStore::load(AgentIdentity &value)
{
    value.id = "00000000-0000-4000-8000-000000000001";
    value.secret = "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA";
    return true;
}
bool AgentCredentialStore::beginMqttProvisioning()
{
    if (attempted) return false;
    attempted = true;
    return true;
}
bool AgentCredentialStore::mqttProvisioningAttempted() { return attempted; }
bool AgentCredentialStore::saveMqtt(const char *secret)
{
    stored = PairingProtocol::validIdentity("00000000-0000-4000-8000-000000000001", secret);
    return stored;
}
void reset()
{
    httpState = FakeHttpState{};
    socketState = FakeSocket{};
    httpState.response = "{\"mqttSecret\":\"BBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBB\"}";
    attempted = stored = false;
    clockReady = true;
}
int main()
{
    reset();
    assert(AgentMqttEnrollmentClient::provision());
    assert(attempted && stored && httpState.posts == 1);
    assert(httpState.url == "https://core.example.invalid:443/api/agent/v1/mqtt/credentials");
    assert(httpState.authorization == "EdolAgent 00000000-0000-4000-8000-000000000001.AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA");
    assert(httpState.redirects == HTTPC_DISABLE_FOLLOW_REDIRECTS);
    assert(!AgentMqttEnrollmentClient::provision() && httpState.posts == 1);
    for (int status : {401, 409, 503, -1})
    {
        reset();
        httpState.status = status;
        assert(!AgentMqttEnrollmentClient::provision() && !stored && attempted);
        assert(!AgentMqttEnrollmentClient::provision() && httpState.posts == 1);
    }
    for (const std::string &body : {std::string(1025, 'x'), std::string("{}"), std::string("not-json")})
    {
        reset();
        httpState.response = body;
        assert(!AgentMqttEnrollmentClient::provision() && !stored);
    }
    reset();
    clockReady = false;
    assert(!AgentMqttEnrollmentClient::provision() && !attempted && httpState.posts == 0);
    reset();
    socketState.connectOk = false;
    assert(AgentMqttEnrollmentClient::provisionDetailed() == ProvisioningResult::TlsFailed);
    assert(!attempted && httpState.posts == 0);
    socketState.connectOk = true;
    assert(AgentMqttEnrollmentClient::provision() && attempted && stored);
    std::cout << "One-time MQTT provisioning contract passed (fake HTTP/TLS)\n";
}
