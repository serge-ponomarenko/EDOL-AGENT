#pragma once

#include "EdolMqttClient.h"

#include <Arduino.h>

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

private:
    void publish(
        const String &payload);

    EdolMqttClient &mqttClient_;
};