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
#include "security/SecureTransport.h"

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

#if EDOL_AGENT_SMOKE_BUILD
// Explicit lab-only USB entry points into the production upload services.
void processSmokeConsole()
{
    static char command[128];
    static size_t used = 0;
    static bool overflow = false;
    while (Serial.available())
    {
        const char value = static_cast<char>(Serial.read());
        if (value == '\r') continue;
        if (value != '\n')
        {
            if (used + 1 < sizeof(command) && !overflow) command[used++] = value;
            else overflow = true;
            continue;
        }
        command[used] = '\0';
        if (overflow) Serial.println("SMOKE command rejected: too long");
        else if (strcmp(command, "STATUS") == 0)
            Serial.printf("SMOKE wifi=%u setup=%u\n", wifiManager.isConnected(), setupMode);
        else if (strcmp(command, "TIME") == 0)
        {
            SecureTransport::synchronizeClock();
            SecureTransport::reportClock("console");
        }
        else if (strcmp(command, "TLS") == 0) SecureTransport::probeCore();
        else if (strcmp(command, "SETUP") == 0)
        {
            bambuMqttClient.disconnect();
            edolMqttClient.disconnect();
            setupMode = true;
            setupPortal.begin();
            Serial.println("SMOKE existing setup portal opened; credentials preserved");
        }
        else if (setupMode || !wifiManager.isConnected())
            Serial.println("SMOKE upload unavailable: setup or WiFi offline");
        else if (strcmp(command, "SNAPSHOT") == 0)
            Serial.printf("SMOKE snapshot=%s\n", agentService.uploadSnapshot() ? "PASS" : "FAIL");
        else if (strncmp(command, "MODEL ", 6) == 0)
        {
            const char *name = command + 6;
            bool valid = *name != '\0' && *name != '.';
            for (const char *cursor = name; *cursor; ++cursor)
                valid = valid && ((*cursor >= 'a' && *cursor <= 'z') || (*cursor >= 'A' && *cursor <= 'Z')
                    || (*cursor >= '0' && *cursor <= '9') || *cursor == '.' || *cursor == '_' || *cursor == '-');
            if (valid) Serial.printf("SMOKE model=%s\n", agentService.uploadModel(name) ? "PASS" : "FAIL");
            else Serial.println("SMOKE model rejected: use a plain ASCII basename");
        }
        else Serial.println("SMOKE commands: STATUS, TIME, TLS, SNAPSHOT, MODEL <basename>, SETUP");
        used = 0;
        overflow = false;
    }
}
#endif

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
#if EDOL_AGENT_SMOKE_BUILD
    Serial.println("SMOKE lab console enabled; uploads require explicit USB commands");
#endif

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

    // Register callbacks even if initial Wi-Fi or time synchronization fails; loop retries later.
    bambuMqttClient.setMessageHandler(handleBambuMessage);
    edolMqttClient.setMessageHandler(handleEdolCommand);
    delay(1000);

    if (!wifiManager.connect())
    {
        return;
    }
#if EDOL_AGENT_SECURE_BUILD
    if (!SecureTransport::synchronizeClock())
    {
        Serial.println("Secure agent requires synchronized time");
        return;
    }
#endif

    bambuMqttClient.connect();
    edolMqttClient.connect();
}

void loop()
{
#if EDOL_AGENT_SMOKE_BUILD
    processSmokeConsole();
#endif
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
