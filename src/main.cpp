#include <Arduino.h>

#include "network/WiFiManager.h"
#include "ftps/FtpsClient.h"
#include "http/HttpUploadClient.h"
#include "agent/AgentService.h"
#include "command/CommandProcessor.h"
#include "mqtt/BambuMqttClient.h"
#include "mqtt/EdolMqttClient.h"
#include "mqtt/AgentEventPublisher.h"
#include "config/Config.h"
#include "mqtt/MessageBridge.h"
#include "mqtt/HeartbeatService.h"
#include "camera/CameraClient.h"
#include "led/LedService.h"
#include "config/ConfigManager.h"
#include "setup/SetupPortal.h"
#include "setup/FactoryResetDetector.h"

LedService ledService;
WiFiManager wifiManager;
FtpsClient ftpsClient;
HttpUploadClient uploadClient;
CameraClient cameraClient;

BambuMqttClient bambuMqttClient;
EdolMqttClient edolMqttClient;
AgentEventPublisher eventPublisher(
    edolMqttClient);

AgentService agentService(
    ftpsClient,
    uploadClient,
    cameraClient,
    eventPublisher);

CommandProcessor commandProcessor(
    bambuMqttClient,
    agentService);

MessageBridge messageBridge(
    edolMqttClient,
    ledService);

HeartbeatService heartbeatService(
    wifiManager,
    bambuMqttClient,
    edolMqttClient,
    agentService);

SetupPortal setupPortal;
FactoryResetDetector factoryResetDetector;
bool setupMode = false;

void handleBambuMessage(
    const char *topic,
    const byte *payload,
    unsigned int length)
{
    messageBridge.forward(
        topic,
        payload,
        length);
}

void handleEdolCommand(
    const char *topic,
    const uint8_t *payload,
    unsigned int length)
{
    commandProcessor.process(
        topic,
        payload,
        length);
}

void setup()
{
    Serial.begin(115200);

    ConfigManager::begin();
    ledService.begin();
    commandProcessor.initialize();

    if (factoryResetDetector.shouldFactoryReset())
    {
        Serial.println();
        Serial.println("==================================");
        Serial.println("FACTORY RESET");
        Serial.println("==================================");

        ledService.startFastBlink();

        uint32_t endTime =
            millis() + 3000;

        while (millis() < endTime)
        {
            ledService.process();
            delay(10);
        }

        ConfigManager::clearAll();

        ESP.restart();
    }

    if (!ConfigManager::isProvisioned())
    {
        ledService.setOn();
        setupMode = true;
        setupPortal.begin();
        return;
    }

    delay(1000);

    if (!wifiManager.connect())
    {
        return;
    }

    bambuMqttClient.setMessageHandler(handleBambuMessage);
    edolMqttClient.setMessageHandler(handleEdolCommand);

    bambuMqttClient.connect();
    edolMqttClient.connect();
}

void loop()
{
    static bool bootMarkedSuccessful = false;

    if (!bootMarkedSuccessful &&
        millis() > 15000)
    {
        factoryResetDetector.markBootSuccessful();
        bootMarkedSuccessful = true;

        Serial.println(
            "Boot marked successful");
    }

    if (setupMode)
    {
        setupPortal.loop();
        ledService.process();
        return;
    }

    wifiManager.maintainConnection();

    static bool mqttDisconnectedForWifiLoss = false;

    if (wifiManager.connectionLost())
    {
        Serial.println("WiFi lost. Disconnecting MQTT clients...");
        bambuMqttClient.disconnect();
        edolMqttClient.disconnect();
        mqttDisconnectedForWifiLoss = true;
    }

    if (wifiManager.isConnected())
    {
        if (mqttDisconnectedForWifiLoss)
        {
            static uint32_t wifiRecoveryTime = millis();

            if (wifiRecoveryTime == 0)
            {
                wifiRecoveryTime = millis();
            }

            if (millis() - wifiRecoveryTime < 5000)
            {
                return;
            }

            mqttDisconnectedForWifiLoss = false;
            wifiRecoveryTime = 0;
        }

        // bambuMqttClient.reconnectIfNeeded();
        if (!bambuMqttClient.isConnected())
        {
            static uint32_t lastAttempt = 0;
            if (millis() - lastAttempt > 30000)
            {
                lastAttempt = millis();
                bambuMqttClient.connect();
            }
        }
        edolMqttClient.reconnectIfNeeded();

        bambuMqttClient.loop();
        edolMqttClient.loop();

        agentService.processSnapshotScheduler();

        heartbeatService.loop();
    }

    ledService.process();

    delay(10);
}
