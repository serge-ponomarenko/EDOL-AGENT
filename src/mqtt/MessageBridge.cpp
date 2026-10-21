#include "MessageBridge.h"

#include "../config/Config.h"
#include "../config/ConfigManager.h"

MessageBridge::MessageBridge(
    EdolMqttClient &edolMqttClient,
    LedService &ledService)
    : edolMqttClient_(edolMqttClient),
      ledService_(ledService)
{
}

void MessageBridge::forward(
    const char *topic,
    const byte *payload,
    unsigned int length)
{
    if (!edolMqttClient_.isConnected())
    {
        return;
    }

    String edolTopic =
        String(Config::EDOL_AGENT_TOPIC_PREFIX) +
        ConfigManager::getEdolAgentId() +
        "/printer/report";

    bool success =
        edolMqttClient_.publish(
            edolTopic.c_str(),
            payload,
            length);

    if (!success)
    {
        Serial.println(
            "EDOL MQTT publish failed");
    }

    ledService_.blink();
}
