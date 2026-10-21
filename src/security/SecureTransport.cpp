#include "SecureTransport.h"

#if EDOL_AGENT_SECURE_BUILD
#include <Arduino.h>
#include <time.h>

namespace
{
bool clockReady()
{
    return time(nullptr) >= static_cast<time_t>(EDOL_AGENT_BUILD_EPOCH - 86400);
}
}

bool SecureTransport::synchronizeClock()
{
    if (clockReady()) return true;
    configTime(0, 0, "pool.ntp.org", "time.cloudflare.com");
    uint32_t started = millis();
    while (!clockReady() && millis() - started < 15000) delay(100);
    return clockReady();
}

bool SecureTransport::prepare(WiFiClientSecure &client)
{
    if (!clockReady()) return false;
    client.setCACert(EDOL_AGENT_ROOT_CA);
    client.setHandshakeTimeout(10);
    client.setTimeout(10000);
    return true;
}
#endif
