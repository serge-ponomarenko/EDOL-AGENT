#include "SetupPortal.h"

#include <Arduino.h>
#include <WiFi.h>
#include <DNSServer.h>
#include <WebServer.h>

#include "../config/ConfigManager.h"
#if EDOL_AGENT_SECURE_BUILD
#include "../security/AgentEnrollmentClient.h"
#include "../security/PairingProtocol.h"
#endif

namespace
{
    DNSServer dnsServer;
    WebServer server(80);

    const char *SETUP_PAGE = R"HTML(
<!DOCTYPE html>
<html>
<head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width, initial-scale=1">

<title>EDOL Agent Setup</title>

<style>
body {
    font-family: Arial, sans-serif;
    max-width: 700px;
    margin: 0 auto;
    padding: 16px;
    background: #f5f5f5;
}

.card {
    background: white;
    border-radius: 10px;
    padding: 16px;
    margin-bottom: 16px;
    box-shadow: 0 1px 4px rgba(0,0,0,0.1);
}

h1 {
    text-align: center;
}

h2 {
    margin-top: 0;
}

label {
    display: block;
    margin-top: 12px;
    margin-bottom: 4px;
}

input {
    width: 100%;
    padding: 12px;
    font-size: 16px;
    box-sizing: border-box;
}

button {
    width: 100%;
    padding: 14px;
    font-size: 16px;
    margin-top: 12px;
}

details {
    margin-top: 12px;
}

.danger {
    background: #c62828;
    color: white;
    border: none;
}
</style>
</head>

<body>

<h1>EDOL Agent Setup</h1>

<p>
Configure WiFi, printer and EDOL connection settings.
Use IP addresses without http:// prefix.
</p>

<form method="POST" action="/save">

<div class="card">
<h2>WiFi</h2>

<label>SSID</label>
<input
    type="text"
    name="ssid"
    value="%SSID%"
    placeholder="MyWiFi">

<label>Password</label>
<input
    type="text"
    name="password"
    value="%WIFI_PASSWORD%"
    placeholder="WiFi password">
</div>

<div class="card">
<h2>EDOL</h2>

%IDENTITY_FIELDS%
</div>

<div class="card">
<h2>Printer</h2>

<label>Host</label>
<input
    type="text"
    name="printerHost"
    value="%PRINTER_HOST%"
    placeholder="example.invalid">

<label>Serial</label>
<input
    type="text"
    name="printerSerial"
    value="%PRINTER_SERIAL%"
    placeholder="">

<label>Access Code</label>
<input
    type="text"
    name="accessCode"
    value="%ACCESS_CODE%"
    placeholder="">

<label>FTPS Directory</label>
<input type="text" name="ftpsDirectory" value="%FTPS_DIRECTORY%">
<small style="color: gray;">
Directory where printer stores model files.
Depending on firmware and printer model it can be:
- root directory - fill /
- cache directory - fill /cache
</small>
</div>

<div class="card">
<h2>EDOL</h2>

<label>EDOL MQTT Host</label>
<input
    type="text"
    name="edolMqttHost"
    value="%EDOL_MQTT_HOST%"
    placeholder="example.invalid">

<label>Core Host</label>
<input
    type="text"
    name="coreHost"
    value="%CORE_HOST%"
    placeholder="example.invalid">

</div>

<div class="card">
<details>
<summary><b>Advanced Settings</b></summary>

<label>EDOL MQTT Port</label>
<input type="number" name="edolMqttPort" value="%EDOL_MQTT_PORT%">

<label>Bambu MQTT Port</label>
<input type="number" name="bambuMqttPort" value="%BAMBU_MQTT_PORT%">

<label>Core Port</label>
<input type="number" name="corePort" value="%CORE_PORT%">

<label>Core Upload Path</label>
<input type="text" name="coreUploadPath" value="%CORE_UPLOAD_PATH%">

<label>FTPS Port</label>
<input type="number" name="ftpsPort" value="%FTPS_PORT%">

<label>FTPS User</label>
<input type="text" name="ftpsUser" value="%FTPS_USER%">

</details>
</div>

<button type="submit">
Save & Reboot
</button>

</form>

<form method="POST" action="/factory-reset">
<button class="danger" type="submit">
Factory Reset
</button>
</form>

</body>
</html>
)HTML";
}

String buildSetupPage()
{
    String html = SETUP_PAGE;
#if EDOL_AGENT_SECURE_BUILD
    html.replace("%IDENTITY_FIELDS%",
        "<label>Pairing code from EDOL</label>"
        "<input type=\"text\" name=\"pairingCode\" maxlength=\"12\" minlength=\"12\" "
        "autocomplete=\"off\" required>"
        "<p>Generate a new code in your printer's Agent page in EDOL.</p>");
#else
    html.replace("%IDENTITY_FIELDS%", R"HTML(
<label>Agent ID</label>
<input type="text" name="agentId" value="%AGENT_ID%" placeholder="agent-123456">
<label>Agent Password</label>
<input type="text" name="agentPassword" value="%AGENT_PASSWORD%">
)HTML");
#endif

    html.replace("%SSID%", ConfigManager::getWifiSSID());
    html.replace("%WIFI_PASSWORD%", ConfigManager::getWifiPassword());

    html.replace("%AGENT_ID%", ConfigManager::getEdolAgentId());
    html.replace("%AGENT_PASSWORD%", ConfigManager::getEdolAgentPassword());

    html.replace("%PRINTER_HOST%", ConfigManager::getBambuPrinterHost());
    html.replace("%PRINTER_SERIAL%", ConfigManager::getBambuPrinterSerial());
    html.replace("%ACCESS_CODE%", ConfigManager::getBambuAccessCode());

    html.replace("%EDOL_MQTT_HOST%", ConfigManager::getEdolMqttHost());
    html.replace("%EDOL_MQTT_PORT%", String(ConfigManager::getEdolMqttPort()));

    html.replace("%BAMBU_MQTT_PORT%", String(ConfigManager::getBambuMqttPort()));

    html.replace("%CORE_HOST%", ConfigManager::getCoreHost());
    html.replace("%CORE_PORT%", String(ConfigManager::getCorePort()));
    html.replace("%CORE_UPLOAD_PATH%", ConfigManager::getCoreUploadPath());

    html.replace("%FTPS_PORT%", String(ConfigManager::getFtpsPort()));
    html.replace("%FTPS_USER%", ConfigManager::getFtpsUser());
    html.replace("%FTPS_DIRECTORY%", ConfigManager::getFtpsDirectory());

    return html;
}

void SetupPortal::begin()
{
    WiFi.mode(WIFI_AP);

    uint64_t mac =
        ESP.getEfuseMac();

    char apName[32];

    snprintf(
        apName,
        sizeof(apName),
        "EDOL-Agent-%04X",
        static_cast<uint16_t>(mac & 0xFFFF));

    WiFi.softAP(apName);

    dnsServer.start(
        53,
        "*",
        WiFi.softAPIP());

    Serial.println();
    Serial.println("==================================");
    Serial.println("EDOL SETUP MODE");
    Serial.print("SSID: ");
    Serial.println(apName);
    Serial.print("IP: ");
    Serial.println(WiFi.softAPIP());
    Serial.println("==================================");

    registerRoutes();

    server.begin();
}

void SetupPortal::registerRoutes()
{
    server.on("/", HTTP_GET, []()
              { server.send(
                    200,
                    "text/html",
                    buildSetupPage()); });

    server.on("/save", HTTP_POST, []()
              {
#if EDOL_AGENT_SECURE_BUILD
                  String pairingCode = server.arg("pairingCode");
                  if (!PairingProtocol::validCode(pairingCode.c_str()))
                  {
                      server.send(400, "text/plain", "Enter a valid pairing code.");
                      return;
                  }
#endif
                  ConfigManager::setWifiSettings(
                      server.arg("ssid"),
                      server.arg("password"));

#if !EDOL_AGENT_SECURE_BUILD
                  ConfigManager::setEdolSettings(
                      server.arg("agentId"),
                      server.arg("agentPassword"));
#endif

                  ConfigManager::setBambuSettings(
                      server.arg("printerSerial"),
                      server.arg("accessCode"),
                      server.arg("printerHost"));

                  ConfigManager::setAdvancedSettings(
                      server.arg("edolMqttHost"),
                      static_cast<uint16_t>(server.arg("edolMqttPort").toInt()),
                      static_cast<uint16_t>(server.arg("bambuMqttPort").toInt()),
                      server.arg("coreHost"),
                      static_cast<uint16_t>(server.arg("corePort").toInt()),
                      server.arg("coreUploadPath"),
                      static_cast<uint16_t>(server.arg("ftpsPort").toInt()),
                      server.arg("ftpsUser"),
                      server.arg("ftpsDirectory"));

#if EDOL_AGENT_SECURE_BUILD
                  // Retain the existing open setup AP while the station joins the configured network.
                  WiFi.mode(WIFI_AP_STA);
                  WiFi.begin(ConfigManager::getWifiSSID().c_str(), ConfigManager::getWifiPassword().c_str());
                  uint32_t started = millis();
                  while (WiFi.status() != WL_CONNECTED && millis() - started < 20000) delay(100);
                  if (WiFi.status() != WL_CONNECTED || !AgentEnrollmentClient::enroll(pairingCode.c_str()))
                  {
                      server.send(400, "text/plain", "Pairing failed. Check the network and use a fresh EDOL code.");
                      return;
                  }
#endif

                  server.send(
                      200,
                      "text/html",
                      "<h1>Configuration saved.</h1><p>Rebooting...</p>");

                  delay(1000);

                  ESP.restart(); });

    server.on("/factory-reset", HTTP_POST, []()
              {
                  ConfigManager::clearAll();
                  ConfigManager::setBootCount(0);

                  server.send(
                      200,
                      "text/html",
                      "<h1>Factory reset complete.</h1><p>Rebooting...</p>");

                  delay(1000);

                  ESP.restart(); });

    const char *probePaths[] = {
        "/generate_204", "/gen_204", "/success.txt", "/ncsi.txt",
        "/hotspot-detect.html", "/connecttest.txt"};
    auto sendSetupPage = []() {
        server.send(200, "text/html", buildSetupPage());
    };
    for (const char *path : probePaths)
    {
        server.on(path, HTTP_GET, sendSetupPage);
    }
    server.onNotFound(sendSetupPage);
}

void SetupPortal::loop()
{
    dnsServer.processNextRequest();
    server.handleClient();
}
