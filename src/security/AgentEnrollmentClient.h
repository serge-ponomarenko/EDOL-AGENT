#pragma once
#include "ProvisioningResult.h"

class AgentEnrollmentClient
{
public:
    // No retries: an uncertain/lost response requires a new owner-issued pairing code.
    static bool enroll(const char *code);
    static ProvisioningResult enrollDetailed(const char *code);
};
