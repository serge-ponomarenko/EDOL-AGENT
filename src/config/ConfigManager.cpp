#include "ConfigManager.h"
#if EDOL_AGENT_SECURE_BUILD
#include "../security/AgentCredentialStore.h"
#endif

Preferences ConfigManager::preferences;
bool ConfigManager::isInitialized = false;

void ConfigManager::begin()
{
    if (!isInitialized)
    {
        preferences.begin("edol_config", false);
        isInitialized = true;
    }
}

void ConfigManager::clearAll()
{
    begin();
    preferences.clear();
#if EDOL_AGENT_SECURE_BUILD
    AgentCredentialStore::clear();
#endif
}

bool ConfigManager::isProvisioned()
{
    begin();

    bool provisioned =
        preferences.isKey("wf_ssid") &&
        preferences.isKey("wf_pass") &&
        preferences.isKey("edol_id") &&
        preferences.isKey("edol_pass");
#if EDOL_AGENT_SECURE_BUILD
    AgentIdentity identity;
    provisioned = preferences.isKey("wf_ssid") && preferences.isKey("wf_pass")
        && preferences.isKey("bambu_host") && preferences.isKey("bambu_ser") && preferences.isKey("bambu_code")
        && AgentCredentialStore::load(identity);
#endif

    Serial.printf(
        "Provisioned=%s\n",
        provisioned ? "true" : "false");

    return provisioned;
}

uint32_t ConfigManager::getBootCount()
{
    begin();

    return preferences.getUInt(
        "boot_cnt",
        0);
}

void ConfigManager::setBootCount(
    uint32_t bootCount)
{
    begin();

    preferences.putUInt(
        "boot_cnt",
        bootCount);
}

String ConfigManager::readStringWithFallback(
    const char *key,
    const char *fallbackValue)
{
    begin();

    if (!preferences.isKey(key))
    {
        return String(fallbackValue);
    }

    return preferences.getString(key);
}

uint16_t ConfigManager::readUInt16WithFallback(
    const char *key,
    uint16_t fallbackValue)
{
    begin();

    if (!preferences.isKey(key))
    {
        return fallbackValue;
    }

    return static_cast<uint16_t>(
        preferences.getUInt(key));
}

String ConfigManager::getWifiSSID()
{
    return readStringWithFallback("wf_ssid", Config::WIFI_SSID);
}

String ConfigManager::getWifiPassword()
{
    return readStringWithFallback("wf_pass", Config::WIFI_PASSWORD);
}

String ConfigManager::getEdolAgentId()
{
    return readStringWithFallback("edol_id", Config::EDOL_AGENT_ID);
}

String ConfigManager::getEdolAgentPassword()
{
    return readStringWithFallback("edol_pass", Config::EDOL_AGENT_PASSWORD);
}

String ConfigManager::getBambuPrinterSerial()
{
    return readStringWithFallback("bambu_ser", Config::BAMBU_PRINTER_SERIAL);
}

String ConfigManager::getBambuAccessCode()
{
    return readStringWithFallback("bambu_code", Config::BAMBU_ACCESS_CODE);
}

String ConfigManager::getBambuPrinterHost()
{
    return readStringWithFallback("bambu_host", Config::BAMBU_PRINTER_HOST);
}

String ConfigManager::getEdolMqttHost()
{
    return readStringWithFallback("edol_m_host", Config::EDOL_MQTT_HOST);
}

uint16_t ConfigManager::getEdolMqttPort()
{
    return readUInt16WithFallback("edol_m_port", Config::EDOL_MQTT_PORT);
}

uint16_t ConfigManager::getBambuMqttPort()
{
    return readUInt16WithFallback("bambu_m_port", Config::BAMBU_MQTT_PORT);
}

String ConfigManager::getCoreHost()
{
    return readStringWithFallback("core_host", Config::CORE_HOST);
}

uint16_t ConfigManager::getCorePort()
{
    return readUInt16WithFallback("core_port", Config::CORE_PORT);
}

String ConfigManager::getCoreUploadPath()
{
    return readStringWithFallback("core_path", Config::CORE_UPLOAD_PATH);
}

uint16_t ConfigManager::getFtpsPort()
{
    return readUInt16WithFallback("ftps_port", Config::FTPS_PORT);
}

String ConfigManager::getFtpsUser()
{
    return readStringWithFallback("ftps_user", Config::FTPS_USER);
}

String ConfigManager::getFtpsDirectory()
{
    return readStringWithFallback("ftps_dir", Config::FTPS_DIRECTORY);
}

void ConfigManager::setWifiSettings(const String &ssid, const String &password)
{
    begin();
    preferences.putString("wf_ssid", ssid);
    preferences.putString("wf_pass", password);
}

void ConfigManager::setEdolSettings(const String &agentId, const String &agentPassword)
{
    begin();
    preferences.putString("edol_id", agentId);
    preferences.putString("edol_pass", agentPassword);
}

void ConfigManager::setBambuSettings(const String &serial, const String &accessCode, const String &host)
{
    begin();
    preferences.putString("bambu_ser", serial);
    preferences.putString("bambu_code", accessCode);
    preferences.putString("bambu_host", host);
}

void ConfigManager::setAdvancedSettings(
    const String &edolMqttHost,
    uint16_t edolMqttPort,
    uint16_t bambuMqttPort,
    const String &coreHost,
    uint16_t corePort,
    const String &coreUploadPath,
    uint16_t ftpsPort,
    const String &ftpsUser,
    const String &ftpsDir)
{
    begin();
    preferences.putString("edol_m_host", edolMqttHost);
    preferences.putUInt("edol_m_port", edolMqttPort);
    preferences.putUInt("bambu_m_port", bambuMqttPort);
    preferences.putString("core_host", coreHost);
    preferences.putUInt("core_port", corePort);
    preferences.putString("core_path", coreUploadPath);
    preferences.putUInt("ftps_port", ftpsPort);
    preferences.putString("ftps_user", ftpsUser);
    preferences.putString("ftps_dir", ftpsDir);
}
