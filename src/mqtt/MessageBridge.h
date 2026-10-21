#pragma once

#include "EdolMqttClient.h"
#include "../led/LedService.h"

class MessageBridge
{
public:
    MessageBridge(
        EdolMqttClient &edolMqttClient,
        LedService &ledService);

    void forward(
        const char *topic,
        const byte *payload,
        unsigned int length);

private:
    EdolMqttClient &edolMqttClient_;
    LedService &ledService_;
};