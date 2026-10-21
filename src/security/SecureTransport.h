#pragma once

#include "SecureConfig.h"
#if EDOL_AGENT_SECURE_BUILD
#include <WiFiClientSecure.h>

namespace SecureTransport
{
bool synchronizeClock();
bool prepare(WiFiClientSecure &client);
}
#endif
