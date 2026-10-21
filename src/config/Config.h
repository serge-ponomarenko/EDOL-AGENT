#pragma once

#include <Arduino.h>
#include "../security/SecureConfig.h"

namespace Config {

// ======== RECONNECT SETTINGS =======================================
constexpr uint32_t WIFI_RECONNECT_INTERVAL_MS = 5000;
constexpr uint32_t MQTT_RECONNECT_INTERVAL_MS = 5000;
constexpr uint32_t BAMBU_MESSAGE_TIMEOUT_MS = 60000;
constexpr uint32_t HEARTBEAT_INTERVAL_MS = 30000;
static constexpr uint32_t CAMERA_SNAPSHOT_INTERVAL_MS = 15000;


// ======== WIFI SETTINGS ============================================

constexpr const char* WIFI_SSID = "";
constexpr const char* WIFI_PASSWORD = "";


// ======== PRINTER SETTINGS =========================================
// ---------- FTPS ---------------------------------------------------

constexpr const char* BAMBU_PRINTER_HOST = "";
constexpr uint16_t FTPS_PORT = 990;
constexpr const char* FTPS_USER = "bblp";
constexpr const char* FTPS_DIRECTORY = "/";

// ---------- MQTT ---------------------------------------------------
constexpr uint16_t BAMBU_MQTT_PORT = 8883;
constexpr const char* BAMBU_PRINTER_SERIAL = "";
constexpr const char* BAMBU_ACCESS_CODE = "";


// ===================================================================
// ======== CORE SETTINGS ============================================

constexpr const char* CORE_HOST = "";
#if EDOL_AGENT_SECURE_BUILD
constexpr uint16_t CORE_PORT = 443;
#else
constexpr uint16_t CORE_PORT = 8080;
#endif

constexpr const char* CORE_UPLOAD_PATH = "/api/agent/upload";

// ======== EDOL MQTT SETTINGS =======================================

constexpr const char* EDOL_MQTT_HOST = "";
constexpr uint16_t EDOL_MQTT_PORT = 1783;

constexpr const char* EDOL_AGENT_ID = "";
constexpr const char* EDOL_AGENT_PASSWORD = "";
constexpr const char* EDOL_AGENT_TOPIC_PREFIX = "edol/agents/";

}
