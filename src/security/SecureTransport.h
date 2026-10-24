#pragma once

#include "SecureConfig.h"
#if EDOL_AGENT_SECURE_BUILD
#include <WiFiClientSecure.h>

namespace SecureTransport
{
class Client : public WiFiClientSecure
{
public:
    int connect(const char *host, uint16_t port) override;
    int connect(const char *host, uint16_t port, int32_t timeout) override;
    int connect(IPAddress address, uint16_t port) override;
    int connect(IPAddress address, uint16_t port, int32_t timeout) override;
#if EDOL_AGENT_SMOKE_BUILD
    // Capture verifier flags before Arduino's failed-connect stop() frees TLS state.
    void resetDiagnostics() { flags_ = 0; flagsKnown_ = false; }
    void stop() override;
    bool flagsKnown() const { return flagsKnown_; }
    uint32_t verificationFlags() const { return flags_; }
#endif

private:
    unsigned char expectedIp_[4] = {};
#if EDOL_AGENT_SMOKE_BUILD
    uint32_t flags_ = 0;
    bool flagsKnown_ = false;
#endif
};
#if EDOL_AGENT_SMOKE_BUILD
void reportClock(const char *stage);
void reportFailure(Client &client, const char *stage, const char *host, uint16_t port);
void probeCore();
#endif
bool synchronizeClock();
bool prepare(Client &client);
}
#endif
