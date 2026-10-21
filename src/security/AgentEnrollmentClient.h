#pragma once

class AgentEnrollmentClient
{
public:
    // No retries: an uncertain/lost response requires a new owner-issued pairing code.
    static bool enroll(const char *code);
};
