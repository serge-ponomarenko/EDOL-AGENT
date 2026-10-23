#pragma once

#include <Arduino.h>

struct AgentIdentity
{
    String id;
    String secret;
};

class AgentCredentialStore
{
public:
    static bool load(AgentIdentity &identity);
    static bool save(const char *id, const char *secret);
    static bool clear();
    static bool loadMqtt(String &secret);
    static bool beginMqttProvisioning();
    static bool mqttProvisioningAttempted();
    static bool saveMqtt(const char *secret);
};
