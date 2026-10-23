#pragma once

#include "SecureConfig.h"
#if EDOL_AGENT_SECURE_BUILD
#include <WiFiClientSecure.h>

namespace SecureTransport
{
#if EDOL_AGENT_SMOKE_BUILD
// Capture verifier flags before Arduino's failed-connect stop() frees TLS state.
class Client : public WiFiClientSecure
{
public:
    void resetDiagnostics() { flags_ = 0; flagsKnown_ = false; }
    void stop() override;
    bool flagsKnown() const { return flagsKnown_; }
    uint32_t verificationFlags() const { return flags_; }

private:
    uint32_t flags_ = 0;
    bool flagsKnown_ = false;
};
void reportClock(const char *stage);
void reportFailure(Client &client, const char *stage, const char *host, uint16_t port);
void probeCore();
#else
using Client = WiFiClientSecure;
#endif
bool synchronizeClock();
bool prepare(Client &client);
}
#endif
