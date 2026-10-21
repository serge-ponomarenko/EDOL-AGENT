#include "AgentEventPublisher.h"

#include <ArduinoJson.h>

#include "../config/Config.h"
#include "../config/ConfigManager.h"

AgentEventPublisher::AgentEventPublisher(
    EdolMqttClient &mqttClient)
    : mqttClient_(mqttClient)
{
}

void AgentEventPublisher::publishModelUploadStarted(
    const String &fileName)
{
    JsonDocument document;

    document["type"] =
        "MODEL_UPLOAD_STARTED";

    document["fileName"] =
        fileName;

    String payload;
    serializeJson(
        document,
        payload);

    publish(payload);
}

void AgentEventPublisher::publishModelUploadCompleted(
    const String &fileName)
{
    JsonDocument document;

    document["type"] =
        "MODEL_UPLOAD_COMPLETED";

    document["fileName"] =
        fileName;

    String payload;
    serializeJson(
        document,
        payload);

    publish(payload);
}

void AgentEventPublisher::publishModelUploadFailed(
    const String &fileName,
    const char *reason)
{
    JsonDocument document;

    document["type"] =
        "MODEL_UPLOAD_FAILED";

    document["fileName"] =
        fileName;

    document["reason"] =
        reason;

    String payload;
    serializeJson(
        document,
        payload);

    publish(payload);
}

void AgentEventPublisher::publish(
    const String &payload)
{
    String eventTopic =
        String(Config::EDOL_AGENT_TOPIC_PREFIX) +
        ConfigManager::getEdolAgentId() +
        "/events";

    mqttClient_.publish(
        eventTopic.c_str(),
        payload.c_str());

    Serial.println("Agent event publish attempted");
}
