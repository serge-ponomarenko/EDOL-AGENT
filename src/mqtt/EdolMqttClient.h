#pragma once

#include <PubSubClient.h>
#include <WiFiClient.h>

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
    WiFiClient wifiClient_;
    PubSubClient mqttClient_;

    String mqttHost_;
};