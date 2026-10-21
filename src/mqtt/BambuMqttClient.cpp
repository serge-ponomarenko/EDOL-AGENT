#include "BambuMqttClient.h"

#include <Arduino.h>

#include "../config/ConfigManager.h"

BambuMqttClient *BambuMqttClient::instance_ =
    nullptr;

BambuMqttClient::BambuMqttClient()
    : mqttClient_(secureClient_)
{
    mqttClient_.setBufferSize(8192);

    lastMessageReceivedTime_ = millis();

    instance_ = this;
}

void BambuMqttClient::setMessageHandler(
    MessageHandler handler)
{
    messageHandler_ = handler;
}

bool BambuMqttClient::connect()
{
    secureClient_.setInsecure();

    mqttHost_ =
        ConfigManager::getBambuPrinterHost();

    mqttClient_.setServer(
        mqttHost_.c_str(),
        ConfigManager::getBambuMqttPort());

    mqttClient_.setCallback(
        mqttCallback);

    Serial.println("Connecting to Bambu MQTT...");
    uint32_t startTime = millis();

    bool connected =
        mqttClient_.connect(
            ConfigManager::getEdolAgentId().c_str(),
            "bblp",
            ConfigManager::getBambuAccessCode().c_str());

    if (!connected)
    {
        Serial.printf(
            "MQTT connect failed. rc=%d\n",
            mqttClient_.state());
        Serial.printf(
            "Bambu MQTT connect failed after %lu ms\n",
            millis() - startTime);
        return false;
    }

    String topic =
        String("device/") + ConfigManager::getBambuPrinterSerial() + "/report";

    mqttClient_.subscribe(topic.c_str());

    Serial.println("Connected to Bambu MQTT");
    lastMessageReceivedTime_ = millis();

    Serial.printf(
        "Bambu MQTT connected after %lu ms\n",
        millis() - startTime);
    return true;
}

void BambuMqttClient::loop()
{
    mqttClient_.loop();
}

bool BambuMqttClient::publish(
    const char *topic,
    const uint8_t *payload,
    unsigned int length)
{
    if (!isConnected())
    {
        return false;
    }

    return mqttClient_.publish(
        topic,
        payload,
        length,
        false);
}

void BambuMqttClient::mqttCallback(
    char *topic,
    byte *payload,
    unsigned int length)
{
    if (instance_ == nullptr)
    {
        return;
    }

    if (instance_->messageHandler_ == nullptr)
    {
        return;
    }

    instance_->lastMessageReceivedTime_ = millis();

    instance_->messageHandler_(
        topic,
        payload,
        length);
}

bool BambuMqttClient::isConnected()
{
    return mqttClient_.connected();
}

bool BambuMqttClient::isHealthy()
{
    if (!isConnected())
    {
        return false;
    }

    return millis() - lastMessageReceivedTime_ < Config::BAMBU_MESSAGE_TIMEOUT_MS;
}

bool BambuMqttClient::requiresReconnect()
{
    return !isConnected();

}

void BambuMqttClient::reconnectIfNeeded()
{
    if (!requiresReconnect())
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
        "Bambu MQTT unhealthy. Reconnecting...");

    mqttClient_.disconnect();

    connect();
}

void BambuMqttClient::disconnect()
{
    mqttClient_.disconnect();

    secureClient_.stop();
}
