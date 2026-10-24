#include "CommandProcessor.h"

#include <Arduino.h>
#include <ArduinoJson.h>

#include "../config/ConfigManager.h"

CommandProcessor::CommandProcessor(
    BambuMqttClient &bambuMqttClient,
    AgentService &agentService, BulkTransferTask &transfers)
    : bambuMqttClient_(bambuMqttClient),
      agentService_(agentService), transfers_(transfers)
{
}

void CommandProcessor::initialize()
{
    printerCommandTopic_ =
        String(Config::EDOL_AGENT_TOPIC_PREFIX) +
        ConfigManager::getEdolAgentId() +
        "/commands/printer";

    agentCommandTopic_ =
        String(Config::EDOL_AGENT_TOPIC_PREFIX) +
        ConfigManager::getEdolAgentId() +
        "/commands/agent";

    bambuRequestTopic_ =
        String("device/") +
        ConfigManager::getBambuPrinterSerial() +
        "/request";
}

void CommandProcessor::process(
    const char *topic,
    const uint8_t *payload,
    unsigned int length)
{
    if (strcmp(
            topic,
            printerCommandTopic_.c_str()) == 0)
    {
        processPrinterCommand(
            payload,
            length);

        return;
    }

    if (strcmp(
            topic,
            agentCommandTopic_.c_str()) == 0)
    {
        processAgentCommand(
            payload,
            length);

        return;
    }

    Serial.println("Unknown command topic");
}

void CommandProcessor::processPrinterCommand(
    const uint8_t *payload,
    unsigned int length)
{
    bool success =
        bambuMqttClient_.publish(
            bambuRequestTopic_.c_str(),
            payload,
            length);

    Serial.printf(
        "Printer command forwarded: %s\n",
        success ? "SUCCESS" : "FAILED");
}

void CommandProcessor::processAgentCommand(
    const uint8_t *payload,
    unsigned int length)
{
    JsonDocument document;

    DeserializationError error =
        deserializeJson(
            document,
            payload,
            length);

    if (error)
    {
        Serial.printf(
            "Agent command JSON parse failed: %s\n",
            error.c_str());

        return;
    }

    const char *command = document["command"];

    if (command == nullptr)
    {
        Serial.println(
            "Agent command missing 'command' field");

        return;
    }

    JsonVariant payloadNode =
        document["payload"];

    if (strcmp(
            command,
            "UPLOAD_MODEL") == 0)
    {
        processUploadModelCommand(
            payloadNode);

        return;
    }

    if (strcmp(
            command,
            "UPLOAD_SNAPSHOT") == 0)
    {
        processUploadSnapshotCommand(
            payloadNode);

        return;
    }

    if (strcmp(
            command,
            "ENABLE_SNAPSHOT_SCHEDULER") == 0)
    {
        processEnableSnapshotSchedulerCommand();

        return;
    }

    if (strcmp(
            command,
            "DISABLE_SNAPSHOT_SCHEDULER") == 0)
    {
        processDisableSnapshotSchedulerCommand();

        return;
    }

    Serial.println("Unknown agent command");
}

void CommandProcessor::processUploadModelCommand(
    JsonVariant payloadNode)
{
    const char *fileName =
        payloadNode["fileName"];

    if (fileName == nullptr)
    {
        Serial.println(
            "UPLOAD_MODEL missing fileName");

        return;
    }

    Serial.println("UPLOAD_MODEL request");

    bool success =
        transfers_.request(
            fileName);

    Serial.printf(
        "UPLOAD_MODEL %s\n",
        success ? "QUEUED" : "REJECTED");
}

void CommandProcessor::processUploadSnapshotCommand(
    JsonVariant payloadNode)
{
    (void)payloadNode;

    Serial.println(
        "UPLOAD_SNAPSHOT request");

    bool success =
        transfers_.request();

    Serial.printf(
        "UPLOAD_SNAPSHOT %s\n",
        success ? "QUEUED" : "REJECTED");
}

void CommandProcessor::processEnableSnapshotSchedulerCommand()
{
    agentService_.enableSnapshotScheduler();
    Serial.println("ENABLE_SNAPSHOT_SCHEDULER SUCCESS");
}

void CommandProcessor::processDisableSnapshotSchedulerCommand()
{
    agentService_.disableSnapshotScheduler();
    Serial.println("DISABLE_SNAPSHOT_SCHEDULER SUCCESS");
}
