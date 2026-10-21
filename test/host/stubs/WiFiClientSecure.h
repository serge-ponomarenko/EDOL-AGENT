#pragma once
#include "WiFiClient.h"
class IPAddress {};
class WiFiClientSecure : public WiFiClient
{
public:
    int lastError(char *, size_t) { return -1; }
};
