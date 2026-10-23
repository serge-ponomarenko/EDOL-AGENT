#include <cassert>
#include <iostream>
#include <Preferences.h>
#include "security/AgentCredentialStore.h"
#include "security/PairingProtocol.h"
#include "security/BoundedResponse.h"

FakePreferencesState preferencesState;

int main()
{
    const char *id = "00000000-0000-4000-8000-000000000001";
    const char *secret = "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA";
    assert(PairingProtocol::validCode("0123456789AB"));
    assert(PairingProtocol::validCode("0123456789ab"));
    assert(!PairingProtocol::validCode("0123456789AI"));
    assert(!PairingProtocol::validCode("0123456789AB\r\n"));
    assert(!PairingProtocol::validIdentity("bad", secret));
    assert(!PairingProtocol::validIdentity(id, "bad\r\n"));
    assert(PairingProtocol::validHost("core.example.invalid"));
    assert(!PairingProtocol::validHost("https://core.example.invalid"));
    assert(!PairingProtocol::validHost("core.example.invalid\r\nHeader: value"));
    AgentIdentity identity;
    assert(!AgentCredentialStore::load(identity));
    preferencesState.values["edol_config"]["edol_pass"] = "synthetic-home";
    assert(AgentCredentialStore::save(id, secret));
    assert(AgentCredentialStore::load(identity));
    assert(std::strcmp(identity.id.c_str(), id) == 0);
    assert(std::strcmp(identity.secret.c_str(), secret) == 0);
    String mqttSecret;
    assert(!AgentCredentialStore::loadMqtt(mqttSecret));
    assert(!AgentCredentialStore::mqttProvisioningAttempted());
    assert(AgentCredentialStore::beginMqttProvisioning());
    assert(AgentCredentialStore::mqttProvisioningAttempted());
    assert(!AgentCredentialStore::beginMqttProvisioning());
    assert(AgentCredentialStore::saveMqtt("BBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBB"));
    assert(AgentCredentialStore::loadMqtt(mqttSecret));
    assert(std::strcmp(mqttSecret.c_str(), secret) != 0);
    preferencesState.writesBeforeFailure = 1;
    assert(!AgentCredentialStore::saveMqtt(secret));
    assert(!AgentCredentialStore::loadMqtt(mqttSecret));
    preferencesState.writesBeforeFailure = 1000;
    assert(!AgentCredentialStore::save("bad", secret));
    assert(AgentCredentialStore::load(identity));
    preferencesState.writesBeforeFailure = 2;
    assert(!AgentCredentialStore::save(id, secret));
    assert(!AgentCredentialStore::load(identity));
    assert(identity.id.length() == 0 && identity.secret.length() == 0);
    preferencesState.writesBeforeFailure = 1000;
    assert(AgentCredentialStore::save(id, secret));
    assert(AgentCredentialStore::clear());
    assert(!AgentCredentialStore::load(identity));
    assert(preferencesState.values["edol_config"]["edol_pass"] == "synthetic-home");
    BoundedResponse response;
    uint8_t bytes[1024] = {};
    assert(response.write(bytes, sizeof(bytes)) == sizeof(bytes));
    assert(response.write(static_cast<uint8_t>('x')) == 0);
    assert(response.failed() && response.size() == 1024);
    assert(response.data()[1024] == '\0');
    std::cout << "Pairing format, bounded response and credential store tests passed\n";
}
