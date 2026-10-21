#include "EdolMqttClient.h"

#include <Arduino.h>

#include "../config/ConfigManager.h"

EdolMqttClient *EdolMqttClient::instance_ = nullptr;

EdolMqttClient::EdolMqttClient()
    : mqttClient_(wifiClient_)
{
    instance_ = this;
}

void EdolMqttClient::setMessageHandler(
    EdolMessageHandler handler)
{
    messageHandler_ = handler;
}

void EdolMqttClient::mqttCallback(
    char *topic,
    byte *payload,
    unsigned int length)
{
    if (instance_ == nullptr)
    {
        return;
    }

    if (!instance_->messageHandler_)
    {
        return;
    }

    instance_->messageHandler_(
        topic,
        payload,
        length);
}

bool EdolMqttClient::connect()
{
#if EDOL_AGENT_SECURE_BUILD
    // The pairing increment provisions HTTPS identity only. Never send it over legacy MQTT.
    return false;
#else
    String agentId = ConfigManager::getEdolAgentId();
    String agentPass = ConfigManager::getEdolAgentPassword();

    mqttHost_ =
        ConfigManager::getEdolMqttHost();

    mqttClient_.setServer(
        mqttHost_.c_str(),
        ConfigManager::getEdolMqttPort());

    mqttClient_.setBufferSize(8192);

    mqttClient_.setCallback(
        mqttCallback);

    Serial.println("Connecting to EDOL MQTT...");

    bool connected =
        mqttClient_.connect(
            agentId.c_str(),
            agentId.c_str(),
            agentPass.c_str());

    if (!connected)
    {
        Serial.printf(
            "EDOL MQTT connect failed. rc=%d\n",
            mqttClient_.state());

        return false;
    }

    String printerTopic =
        String("edol/agents/") +
        agentId +
        "/commands/printer";

    String agentTopic =
        String("edol/agents/") +
        agentId +
        "/commands/agent";

    mqttClient_.subscribe(printerTopic.c_str());

    mqttClient_.subscribe(agentTopic.c_str());

    Serial.println("Connected to EDOL MQTT");

    return true;
#endif
}

void EdolMqttClient::loop()
{
    mqttClient_.loop();
}

void EdolMqttClient::disconnect()
{
    mqttClient_.disconnect();
    wifiClient_.stop();
}

bool EdolMqttClient::publish(
    const char *topic,
    const uint8_t *payload,
    unsigned int length)
{
    return mqttClient_.publish(
        topic,
        payload,
        length);
}

bool EdolMqttClient::publish(
    const char *topic,
    const char *payload)
{
    return mqttClient_.publish(
        topic,
        payload);
}

bool EdolMqttClient::isConnected()
{
    return mqttClient_.connected();
}

void EdolMqttClient::reconnectIfNeeded()
{
    if (isConnected())
    {
        return;
    }

    static uint32_t lastReconnectAttempt = 0;

    if (millis() - lastReconnectAttempt < Config::MQTT_RECONNECT_INTERVAL_MS)
    {
        return;
    }

    lastReconnectAttempt = millis();

    Serial.println(
        "EDOL MQTT disconnected. Reconnecting...");

    mqttClient_.disconnect();

    connect();
}
