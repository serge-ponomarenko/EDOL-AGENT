#pragma once
#include "Arduino.h"
#include <vector>
#include <functional>
#include <cassert>

struct FakeMqttState
{
    bool connectOk = true;
    bool connected = false;
    bool subscribeOk = true, publishOk = true;
    unsigned connects = 0, loops = 0, publishes = 0, disconnects = 0;
    bool inLoop = false, hasMessage = false;
    std::string topic, payload;
    std::function<void()> ownerCheck;
    std::string host, id, username, password;
    std::vector<std::string> subscriptions;
};
extern FakeMqttState mqttState;

class PubSubClient
{
public:
    static std::vector<PubSubClient *> &instances() { static std::vector<PubSubClient *> items; return items; }
    PubSubClient() { instances().push_back(this); }
    template <typename T> explicit PubSubClient(T &) { instances().push_back(this); }
    void useState(FakeMqttState &state) { state_ = &state; }
    void setServer(const char *host, uint16_t) { state_->host = host; }
    bool setBufferSize(unsigned) { return true; }
    template <typename T> void setCallback(T callback) { callback_ = callback; }
    void setSocketTimeout(unsigned) {}
    bool connect(const char *id, const char *username, const char *password)
    {
        ++state_->connects;
        state_->id = id;
        state_->username = username;
        state_->password = password;
        state_->connected = state_->connectOk;
        return state_->connected;
    }
    bool subscribe(const char *topic)
    {
        state_->subscriptions.push_back(topic);
        return state_->subscribeOk;
    }
    bool publish(const char *, const uint8_t *, unsigned, bool = false) { checkOwner(); ++state_->publishes; return state_->connected && state_->publishOk; }
    bool publish(const char *, const char *) { checkOwner(); ++state_->publishes; return state_->connected && state_->publishOk; }
    bool connected() { return state_->connected; }
    void disconnect() { checkOwner(); ++state_->disconnects; state_->connected = false; }
    int state() { return -1; }
    void loop() {
        checkOwner(); assert(!state_->inLoop); state_->inLoop = true; ++state_->loops;
        if (state_->hasMessage && callback_) {
            state_->hasMessage = false;
            std::vector<char> topic(state_->topic.begin(), state_->topic.end()); topic.push_back(0);
            std::vector<byte> payload(state_->payload.begin(), state_->payload.end());
            callback_(topic.data(), payload.data(), static_cast<unsigned>(payload.size()));
        }
        state_->inLoop = false;
    }
private:
    FakeMqttState *state_ = &mqttState;
    std::function<void(char *, byte *, unsigned)> callback_;
    void checkOwner() { if (state_->ownerCheck) state_->ownerCheck(); }
};
