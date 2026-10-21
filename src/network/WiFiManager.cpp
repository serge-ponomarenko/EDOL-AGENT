#include "WiFiManager.h"

#include <Arduino.h>
#include <WiFi.h>

#include "../config/Config.h"
#include "../config/ConfigManager.h"

bool WiFiManager::connect()
{
    String ssid = ConfigManager::getWifiSSID();
    Serial.println("Connecting to WiFi");

    WiFi.mode(WIFI_STA);
    WiFi.begin(ssid.c_str(), ConfigManager::getWifiPassword().c_str());

    uint32_t start = millis();

    while (WiFi.status() != WL_CONNECTED && millis() - start < 20000)
    {
        delay(500);
        Serial.print(".");
    }

    Serial.println();

    if (WiFi.status() != WL_CONNECTED)
    {
        Serial.println("WiFi connection failed");
        return false;
    }

    Serial.println("WiFi connected");

    return true;
}

bool WiFiManager::isConnected()
{
    return WiFi.status() == WL_CONNECTED;
}

bool WiFiManager::connectionLost()
{
    static bool wasConnected = true;

    bool connected = isConnected();

    if (wasConnected && !connected)
    {
        wasConnected = false;
        return true;
    }

    if (connected)
    {
        wasConnected = true;
    }

    return false;
}

void WiFiManager::maintainConnection()
{
    if (isConnected())
    {
        return;
    }

    static uint32_t lastReconnectAttempt = 0;

    if (millis() - lastReconnectAttempt < Config::WIFI_RECONNECT_INTERVAL_MS)
    {
        return;
    }

    lastReconnectAttempt = millis();

    Serial.println("WiFi disconnected. Reconnecting...");

    WiFi.disconnect();

    WiFi.begin(
        ConfigManager::getWifiSSID().c_str(),
        ConfigManager::getWifiPassword().c_str());
}
