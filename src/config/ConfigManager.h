#pragma once

#include <Arduino.h>
#include <Preferences.h>
#include "Config.h"

class ConfigManager
{
private:
    static Preferences preferences;
    static bool isInitialized;

    static String readStringWithFallback(const char *key, const char *fallbackValue);
    static uint16_t readUInt16WithFallback(const char *key, uint16_t fallbackValue);

public:
    static void begin();
    static void clearAll();

    static bool isProvisioned();

    static uint32_t getBootCount();
    static void setBootCount(uint32_t bootCount);

    static String getWifiSSID();
    static String getWifiPassword();
    static String getEdolAgentId();
    static String getEdolAgentPassword();
    static String getBambuPrinterSerial();
    static String getBambuAccessCode();
    static String getBambuPrinterHost();

    static String getEdolMqttHost();
    static uint16_t getEdolMqttPort();
    static uint16_t getBambuMqttPort();
    static String getCoreHost();
    static uint16_t getCorePort();
    static String getCoreUploadPath();
    static uint16_t getFtpsPort();
    static String getFtpsUser();
    static String getFtpsDirectory();

    static void setWifiSettings(const String &ssid, const String &password);
    static void setEdolSettings(const String &agentId, const String &agentPassword);
    static void setBambuSettings(const String &serial, const String &accessCode, const String &host);
    static void setAdvancedSettings(
        const String &edolMqttHost,
        uint16_t edolMqttPort,
        uint16_t bambuMqttPort,
        const String &coreHost,
        uint16_t corePort,
        const String &coreUploadPath,
        uint16_t ftpsPort,
        const String &ftpsUser,
        const String &ftpsDir);
};