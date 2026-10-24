#pragma once

#include "EdolMqttClient.h"

#include <Arduino.h>
#include <atomic>

class AgentEventPublisher
{
public:
    explicit AgentEventPublisher(
        EdolMqttClient &mqttClient);

    void publishModelUploadStarted(
        const String &fileName);

    void publishModelUploadCompleted(
        const String &fileName);

    void publishModelUploadFailed(
        const String &fileName,
        const char *reason);

    void drain();
    bool hasPending() const { return produced_.load() != consumed_.load(); }

private:
    void publish(
        const String &payload);

    EdolMqttClient &mqttClient_;
    String pending_[3];
    uint32_t createdAt_[3] = {}, lastAttempt_[3] = {};
    unsigned attempts_[3] = {};
    std::atomic<unsigned> produced_{0}, consumed_{0};
};