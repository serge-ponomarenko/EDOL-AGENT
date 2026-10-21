#include "HeartbeatService.h"

#include <Arduino.h>

#include "../config/Config.h"
#include "../config/ConfigManager.h"

HeartbeatService::HeartbeatService(
    WiFiManager &wifiManager,
    BambuMqttClient &bambuMqttClient,
    EdolMqttClient &edolMqttClient,
    AgentService &agentService)
    : wifiManager_(wifiManager),
      bambuMqttClient_(bambuMqttClient),
      edolMqttClient_(edolMqttClient),
      agentService_(agentService)
{
}

void HeartbeatService::loop()
{
    if (millis() - lastHeartbeatTime_ < Config::HEARTBEAT_INTERVAL_MS)
    {
        return;
    }

    lastHeartbeatTime_ = millis();

    String payload =
        String("{") +
        "\"agentId\":\"" + ConfigManager::getEdolAgentId().c_str() + "\"," +
        "\"printerSerial\":\"" + ConfigManager::getBambuPrinterSerial().c_str() + "\"," +
        "\"wifiConnected\":" + (wifiManager_.isConnected() ? "true" : "false") + "," +
        "\"bambuConnected\":" + (bambuMqttClient_.isConnected() ? "true" : "false") + "," +
        "\"bambuHealthy\":" + (bambuMqttClient_.isHealthy() ? "true" : "false") + "," +
        "\"cameraHealthy\":" + (agentService_.isCameraHealthy() ? "true" : "false") + "," +
        "\"edolConnected\":" + (edolMqttClient_.isConnected() ? "true" : "false") + "}";

    String topic =
        String(Config::EDOL_AGENT_TOPIC_PREFIX) +
        ConfigManager::getEdolAgentId() +
        "/heartbeat";

    edolMqttClient_.publish(
        topic.c_str(),
        payload.c_str());
}