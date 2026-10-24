#include <cassert>
#include <iostream>
#include <HTTPClient.h>
#include "mqtt/EdolMqttClient.h"
#include "security/AgentCredentialStore.h"
#include "security/AgentMqttEnrollmentClient.h"
#include "security/SecureTransport.h"
#include "config/ConfigManager.h"

FakeSerial Serial;
FakeSocket socketState;
FakeHttpState httpState;
FakeMqttState mqttState;
uint32_t fakeMillis = 0;
bool identityReady = true, mqttReady = true, trustReady = true, clockReady = true;
unsigned provisions = 0, preparations = 0;
String host = "mqtt.example.invalid";
const char *id = "00000000-0000-4000-8000-000000000001";
const char *mqttSecret = "BBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBB";
String ConfigManager::getEdolMqttHost() { return host; }
uint16_t ConfigManager::getEdolMqttPort() { return Config::EDOL_MQTT_PORT; }
String ConfigManager::getEdolAgentId() { return "home-agent"; }
String ConfigManager::getEdolAgentPassword() { return "home-password"; }
bool AgentCredentialStore::load(AgentIdentity &value)
{
    if (!identityReady) return false;
    value.id = id;
    value.secret = "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA";
    return true;
}
bool AgentCredentialStore::loadMqtt(String &value) { value = mqttSecret; return mqttReady; }
bool AgentMqttEnrollmentClient::provision() { ++provisions; mqttReady = true; return true; }
#if EDOL_AGENT_SECURE_BUILD
bool SecureTransport::synchronizeClock() { return clockReady; }
bool SecureTransport::prepare(SecureTransport::Client &) { ++preparations; return trustReady; }
#endif

int main()
{
    EdolMqttClient client;
    assert(client.connect());
#if EDOL_AGENT_SECURE_BUILD
    assert(mqttState.id == std::string("edol-agent:") + id);
    assert(mqttState.username == std::string("agent:") + id);
    assert(mqttState.password == mqttSecret);
    assert(mqttState.password != "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA");
    assert(mqttState.subscriptions[0] == std::string("edol/agent/v1/") + id + "/commands/printer");
    assert(preparations == 1 && provisions == 0);
    client.disconnect();
    identityReady = false;
    assert(!client.connect());
    identityReady = true;
    clockReady = false;
    assert(!client.connect());
    clockReady = true;
    trustReady = false;
    assert(!client.connect());
    trustReady = true;
    host = "https://mqtt.example.invalid";
    assert(!client.connect());
    host = "mqtt.example.invalid";
    mqttReady = false;
    assert(client.connect() && provisions == 1);
    client.disconnect();
    mqttState.subscribeOk = false;
    assert(!client.connect() && !client.isConnected());
    mqttState.subscribeOk = true;
#else
    assert(mqttState.id == "home-agent" && mqttState.username == "home-agent");
    assert(mqttState.password == "home-password");
    assert(mqttState.subscriptions[0] == "edol/agents/home-agent/commands/printer");
    assert(provisions == 0 && preparations == 0);
#endif
    client.disconnect();
    unsigned before = mqttState.connects;
    fakeMillis = 6000;
    client.reconnectIfNeeded();
    assert(client.isConnected() && mqttState.connects == before + 1);
    client.disconnect();
    // A reconstructed client uses the retained credentials and restores its subscriptions.
    EdolMqttClient recovered;
    assert(recovered.connect());
    std::cout << "MQTT identity, topics and reconnect contract passed (fake MQTT/TLS)\n";
}
