#pragma once
#include "ProvisioningResult.h"

class AgentMqttEnrollmentClient
{
public:
    static bool provision();
    static ProvisioningResult provisionDetailed();
};
