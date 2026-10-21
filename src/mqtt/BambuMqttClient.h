#pragma once

#include <PubSubClient.h>
#include <WiFiClientSecure.h>

using MessageHandler =
    void (*)(const char *topic,
             const byte *payload,
             unsigned int length);

class BambuMqttClient
{
public:
    BambuMqttClient();

    bool connect();

    void loop();

    void setMessageHandler(MessageHandler handler);

    bool publish(
        const char *topic,
        const uint8_t *payload,
        unsigned int length);

    bool isConnected();
    bool isHealthy();
    bool requiresReconnect();

    void reconnectIfNeeded();

    void disconnect();

private:
    static void mqttCallback(
        char *topic,
        byte *payload,
        unsigned int length);

    static BambuMqttClient *instance_;

    MessageHandler messageHandler_ = nullptr;

    uint32_t lastMessageReceivedTime_;

    WiFiClientSecure secureClient_;
    PubSubClient mqttClient_;

    String mqttHost_;
};