#pragma once
#include "Arduino.h"
#include <vector>

struct FakeMqttState
{
    bool connectOk = true;
    bool connected = false;
    bool subscribeOk = true;
    unsigned connects = 0;
    std::string host, id, username, password;
    std::vector<std::string> subscriptions;
};
extern FakeMqttState mqttState;

class PubSubClient
{
public:
    PubSubClient() = default;
    template <typename T> explicit PubSubClient(T &) {}
    void setServer(const char *host, uint16_t) { mqttState.host = host; }
    bool setBufferSize(unsigned) { return true; }
    template <typename T> void setCallback(T) {}
    void setSocketTimeout(unsigned) {}
    bool connect(const char *id, const char *username, const char *password)
    {
        ++mqttState.connects;
        mqttState.id = id;
        mqttState.username = username;
        mqttState.password = password;
        mqttState.connected = mqttState.connectOk;
        return mqttState.connected;
    }
    bool subscribe(const char *topic)
    {
        mqttState.subscriptions.push_back(topic);
        return mqttState.subscribeOk;
    }
    bool publish(const char *, const uint8_t *, unsigned) { return mqttState.connected; }
    bool publish(const char *, const char *) { return mqttState.connected; }
    bool connected() { return mqttState.connected; }
    void disconnect() { mqttState.connected = false; }
    int state() { return -1; }
    void loop() {}
};
