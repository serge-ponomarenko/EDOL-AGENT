#pragma once

#include "../network/WiFiManager.h"
#include "BambuMqttClient.h"
#include "EdolMqttClient.h"
#include "../agent/AgentService.h"

class HeartbeatService
{
public:
    HeartbeatService(
        WiFiManager &wifiManager,
        BambuMqttClient &bambuMqttClient,
        EdolMqttClient &edolMqttClient,
        AgentService &agentService);

    void loop();

private:
    WiFiManager &wifiManager_;
    BambuMqttClient &bambuMqttClient_;
    EdolMqttClient &edolMqttClient_;
    AgentService& agentService_;

    uint32_t lastHeartbeatTime_ = 0;
};