#pragma once

#ifndef EDOL_AGENT_SECURE_BUILD
#define EDOL_AGENT_SECURE_BUILD 0
#endif

#ifndef EDOL_AGENT_SMOKE_BUILD
#define EDOL_AGENT_SMOKE_BUILD 0
#endif

#if EDOL_AGENT_SMOKE_BUILD && !EDOL_AGENT_SECURE_BUILD
#error "The smoke console requires a secure build"
#endif

#if EDOL_AGENT_SECURE_BUILD
#include "edol_agent_trust.h"
#endif
