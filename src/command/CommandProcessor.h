#pragma once

#include "../mqtt/BambuMqttClient.h"
#include "../agent/AgentService.h"

#include <ArduinoJson.h>

class CommandProcessor
{
public:
    CommandProcessor(
        BambuMqttClient &bambuMqttClient,
        AgentService &agentService);

        void initialize();

    void process(
        const char *topic,
        const uint8_t *payload,
        unsigned int length);

private:
    void processPrinterCommand(
        const uint8_t *payload,
        unsigned int length);

    void processAgentCommand(
        const uint8_t *payload,
        unsigned int length);

    void processUploadModelCommand(
        JsonVariant payloadNode);

    void processUploadSnapshotCommand(
        JsonVariant payloadNode);

    void processEnableSnapshotSchedulerCommand();

    void processDisableSnapshotSchedulerCommand();

    BambuMqttClient &bambuMqttClient_;
    AgentService &agentService_;

    String printerCommandTopic_;
    String agentCommandTopic_;
    String bambuRequestTopic_;
};