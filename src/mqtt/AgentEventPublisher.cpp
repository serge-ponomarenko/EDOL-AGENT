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
    const unsigned next = produced_.load();
    if (next - consumed_.load() >= 3 || payload.length() > 2048)
    {
        Serial.println("Agent event queue capacity exceeded");
        return;
    }
    pending_[next % 3] = payload;
    createdAt_[next % 3] = millis();
    attempts_[next % 3] = 0;
    produced_.store(next + 1);
}

void AgentEventPublisher::drain()
{
    // MQTT has exactly one owner: the main loop. Event retries never replay printer commands.
    while (consumed_.load() != produced_.load())
    {
        unsigned index = consumed_.load();
        const unsigned slot = index % 3;
        if (millis() - createdAt_[slot] >= 60000)
        {
            pending_[slot] = "";
            consumed_.store(index + 1);
            Serial.println("Agent event expired without delivery confirmation");
            continue;
        }
        if (!mqttClient_.isConnected()) return;
        if (attempts_[slot] && millis() - lastAttempt_[slot] < 1000) return;
        String eventTopic = String(Config::EDOL_AGENT_TOPIC_PREFIX)
            + ConfigManager::getEdolAgentId() + "/events";
        bool sent = mqttClient_.publish(eventTopic.c_str(), pending_[slot].c_str());
        lastAttempt_[slot] = millis();
        ++attempts_[slot];
        if (!sent && attempts_[slot] < 3) return;
        pending_[slot] = "";
        consumed_.store(index + 1);
        Serial.printf("Agent event publish: %s\n", sent ? "SUCCESS" : "FAILED");
    }
}
