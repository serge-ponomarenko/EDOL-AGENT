#pragma once

#include <PubSubClient.h>
#include <WiFiClient.h>
#include "../security/SecureConfig.h"
#if EDOL_AGENT_SECURE_BUILD
#include <WiFiClientSecure.h>
#endif

#include <functional>

using EdolMessageHandler =
    std::function<void(
        const char *topic,
        const uint8_t *payload,
        unsigned int length)>;

class EdolMqttClient
{
public:
    EdolMqttClient();

    bool connect();

    void loop();

    bool publish(
        const char *topic,
        const uint8_t *payload,
        unsigned int length);

    bool publish(
        const char *topic,
        const char *payload);

    bool isConnected();

    void reconnectIfNeeded();

    void disconnect();

    void setMessageHandler(
        EdolMessageHandler handler);

private:
    static void mqttCallback(
        char *topic,
        byte *payload,
        unsigned int length);

    static EdolMqttClient *instance_;

    EdolMessageHandler messageHandler_;
#if EDOL_AGENT_SECURE_BUILD
    WiFiClientSecure wifiClient_;
#else
    WiFiClient wifiClient_;
#endif
    PubSubClient mqttClient_;

    String mqttHost_;
};
