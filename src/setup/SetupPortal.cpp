#include "SetupPortal.h"

#include <Arduino.h>
#include <WiFi.h>
#include <DNSServer.h>
#include <WebServer.h>

#include "../config/ConfigManager.h"
#if EDOL_AGENT_SECURE_BUILD
#include "../security/AgentEnrollmentClient.h"
#include "../security/PairingProtocol.h"
#include "../security/AgentCredentialStore.h"
#include "../security/AgentMqttEnrollmentClient.h"
#include "../security/SecureTransport.h"
#include "../mqtt/EdolMqttClient.h"
#include "ProvisioningFlow.h"
#endif

namespace
{
    DNSServer dnsServer;
    WebServer server(80);
#if EDOL_AGENT_SECURE_BUILD
    ProvisioningFlow provisioning;
    EdolMqttClient *setupMqtt = nullptr;
    uint32_t nextStep = 0;
    uint32_t rebootAt = 0;

    class DeviceProvisioningActions : public ProvisioningActions
    {
        ProvisioningResult wifi()
        {
            if (WiFi.status() == WL_CONNECTED) return ProvisioningResult::Success;
            WiFi.begin(ConfigManager::getWifiSSID().c_str(), ConfigManager::getWifiPassword().c_str());
            uint32_t started = millis();
            while (WiFi.status() != WL_CONNECTED && millis() - started < 20000) delay(100);
            return WiFi.status() == WL_CONNECTED ? ProvisioningResult::Success : ProvisioningResult::WifiTimeout;
        }
    public:
        uint32_t completionTime(uint32_t) const override { return millis(); }
        ProvisioningResult run(SetupStage stage, const char *code) override
        {
            if (wifi() != ProvisioningResult::Success) return ProvisioningResult::WifiTimeout;
            switch (stage)
            {
            case SetupStage::Wifi: return ProvisioningResult::Success;
            case SetupStage::Clock:
                return SecureTransport::synchronizeClock() ? ProvisioningResult::Success : ProvisioningResult::ClockTimeout;
            case SetupStage::Tls:
            {
                String host = ConfigManager::getCoreHost();
                if (!PairingProtocol::validHost(host.c_str()) || ConfigManager::getCorePort() == 0)
                    return ProvisioningResult::InvalidEndpoint;
                SecureTransport::Client client;
                if (!SecureTransport::synchronizeClock() || !SecureTransport::prepare(client))
                    return ProvisioningResult::ClockTimeout;
                const bool connected = client.connect(host.c_str(), ConfigManager::getCorePort());
#if EDOL_AGENT_SMOKE_BUILD
                SecureTransport::reportClock("setup-tls");
                if (!connected) SecureTransport::reportFailure(client, "setup-tls", host.c_str(), ConfigManager::getCorePort());
#endif
                client.stop();
                return connected ? ProvisioningResult::Success : ProvisioningResult::TlsFailed;
            }
            case SetupStage::Enrollment: return AgentEnrollmentClient::enrollDetailed(code);
            case SetupStage::MqttCredentials:
            {
                String secret;
                return AgentCredentialStore::loadMqtt(secret) ? ProvisioningResult::Success
                    : AgentMqttEnrollmentClient::provisionDetailed();
            }
            case SetupStage::MqttConnect:
                return setupMqtt != nullptr && setupMqtt->connect() ? ProvisioningResult::Success : ProvisioningResult::BrokerFailed;
            default: return ProvisioningResult::InvalidEndpoint;
            }
        }
    } provisioningActions;

    String escapeHtml(String value)
    {
        value.replace("&", "&amp;");
        value.replace("\"", "&quot;");
        value.replace("<", "&lt;");
        value.replace(">", "&gt;");
        value.replace("'", "&#39;");
        return value;
    }
#endif

    const char *SETUP_PAGE = R"HTML(
<!DOCTYPE html>
<html lang="en">
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
Use hostnames or IP addresses without a scheme prefix.
</p>

<form method="POST" action="/save">

<div class="card">
<h2>WiFi</h2>

<label for="ssid">SSID</label>
<input
    type="text"
    name="ssid" id="ssid"
    value="%SSID%"
    placeholder="MyWiFi">

<label for="password">Password</label>
<input
    type="password"
    name="password" id="password"
    value="%WIFI_PASSWORD%"
    placeholder="WiFi password">
</div>

<div class="card">
<h2>EDOL</h2>

%IDENTITY_FIELDS%
</div>

<div class="card">
<h2>Printer</h2>

<label for="printerHost">Host</label>
<input
    type="text"
    name="printerHost" id="printerHost"
    value="%PRINTER_HOST%"
    placeholder="example.invalid">

<label for="printerSerial">Serial</label>
<input
    type="text"
    name="printerSerial" id="printerSerial"
    value="%PRINTER_SERIAL%"
    placeholder="">

<label for="accessCode">Access Code</label>
<input
    type="password"
    name="accessCode" id="accessCode"
    value="%ACCESS_CODE%"
    placeholder="">

<label for="ftpsDirectory">FTPS Directory</label>
<input type="text" name="ftpsDirectory" id="ftpsDirectory" value="%FTPS_DIRECTORY%">
<small style="color: gray;">
Directory where printer stores model files.
Depending on firmware and printer model it can be:
- root directory - fill /
- cache directory - fill /cache
</small>
</div>

<div class="card">
<h2>EDOL</h2>

<label for="edolMqttHost">EDOL MQTT Host</label>
<input
    type="text"
    name="edolMqttHost" id="edolMqttHost"
    value="%EDOL_MQTT_HOST%"
    placeholder="example.invalid">

<label for="coreHost">Core Host</label>
<input
    type="text"
    name="coreHost" id="coreHost"
    value="%CORE_HOST%"
    placeholder="example.invalid">

</div>

<div class="card">
<details>
<summary><b>Advanced Settings</b></summary>

<label for="edolMqttPort">EDOL MQTT Port</label>
<input type="number" name="edolMqttPort" id="edolMqttPort" value="%EDOL_MQTT_PORT%">

<label for="bambuMqttPort">Bambu MQTT Port</label>
<input type="number" name="bambuMqttPort" id="bambuMqttPort" value="%BAMBU_MQTT_PORT%">

<label for="corePort">Core Port</label>
<input type="number" name="corePort" id="corePort" value="%CORE_PORT%">

<label for="coreUploadPath">Core Upload Path</label>
<input type="text" name="coreUploadPath" id="coreUploadPath" value="%CORE_UPLOAD_PATH%">

<label for="ftpsPort">FTPS Port</label>
<input type="number" name="ftpsPort" id="ftpsPort" value="%FTPS_PORT%">

<label for="ftpsUser">FTPS User</label>
<input type="text" name="ftpsUser" id="ftpsUser" value="%FTPS_USER%">

</details>
</div>

<button type="submit">
Save & Reboot
</button>

</form>

%SETUP_PROGRESS%

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
        "<label for=\"pairingCode\">Pairing code from EDOL</label>"
        "<input type=\"password\" name=\"pairingCode\" id=\"pairingCode\" maxlength=\"12\" minlength=\"12\" "
        "autocomplete=\"off\">"
        "<p>Use a fresh code from the printer's Agent page in EDOL. Leave blank to retain "
        "an existing identity or retry a code still held in this setup session. "
        "The code is never shown or saved to device storage.</p>");
    html.replace("%SETUP_PROGRESS%", R"HTML(
<section class="card" aria-live="polite">
<h2>Setup progress</h2><p id="setup-status">%CURRENT_SETUP_STATUS%</p>
<noscript>Refresh this page to see progress. Do not resubmit the pairing code.</noscript>
<p>Save &amp; Reboot runs Wi-Fi, NTP, TLS, enrollment and MQTT setup automatically.
The open setup network stays available until success; reboot follows completion.</p>
<form method="POST" action="/retry"><button>Retry saved settings</button></form>
</section>
<script>
async function refreshSetup() {
  try {
    const response = await fetch('/status', {cache: 'no-store'});
    if (response.ok) document.getElementById('setup-status').textContent = await response.text();
  } catch { document.getElementById('setup-status').textContent = 'Connection interrupted. Reconnect to the setup Wi-Fi; saved settings are retained.'; }
  setTimeout(refreshSetup, 1000);
}
setTimeout(refreshSetup, 0);
</script>
)HTML");
    html.replace("%CURRENT_SETUP_STATUS%", escapeHtml(String(provisioning.stageName()) + ". "
        + provisioningMessage(provisioning.result())));
#else
    html.replace("%SETUP_PROGRESS%", "");
    html.replace("%IDENTITY_FIELDS%", R"HTML(
<label for="agentId">Agent ID</label>
<input type="text" name="agentId" id="agentId" value="%AGENT_ID%" placeholder="agent-123456">
<label for="agentPassword">Agent Password</label>
<input type="text" name="agentPassword" id="agentPassword" value="%AGENT_PASSWORD%">
)HTML");
#endif

    // Escape secure portal values; retain Home's existing rendering contract.
#if EDOL_AGENT_SECURE_BUILD
    #define SETUP_VALUE(value) escapeHtml(value)
#else
    #define SETUP_VALUE(value) (value)
#endif
    html.replace("%SSID%", SETUP_VALUE(ConfigManager::getWifiSSID()));
    html.replace("%WIFI_PASSWORD%", SETUP_VALUE(ConfigManager::getWifiPassword()));

    html.replace("%AGENT_ID%", ConfigManager::getEdolAgentId());
    html.replace("%AGENT_PASSWORD%", ConfigManager::getEdolAgentPassword());

    html.replace("%PRINTER_HOST%", SETUP_VALUE(ConfigManager::getBambuPrinterHost()));
    html.replace("%PRINTER_SERIAL%", SETUP_VALUE(ConfigManager::getBambuPrinterSerial()));
    html.replace("%ACCESS_CODE%", SETUP_VALUE(ConfigManager::getBambuAccessCode()));

    html.replace("%EDOL_MQTT_HOST%", SETUP_VALUE(ConfigManager::getEdolMqttHost()));
    html.replace("%EDOL_MQTT_PORT%", String(ConfigManager::getEdolMqttPort()));

    html.replace("%BAMBU_MQTT_PORT%", String(ConfigManager::getBambuMqttPort()));

    html.replace("%CORE_HOST%", SETUP_VALUE(ConfigManager::getCoreHost()));
    html.replace("%CORE_PORT%", String(ConfigManager::getCorePort()));
    html.replace("%CORE_UPLOAD_PATH%", SETUP_VALUE(ConfigManager::getCoreUploadPath()));

    html.replace("%FTPS_PORT%", String(ConfigManager::getFtpsPort()));
    html.replace("%FTPS_USER%", SETUP_VALUE(ConfigManager::getFtpsUser()));
    html.replace("%FTPS_DIRECTORY%", SETUP_VALUE(ConfigManager::getFtpsDirectory()));
    #undef SETUP_VALUE

    return html;
}

void SetupPortal::begin(EdolMqttClient *mqtt)
{
#if EDOL_AGENT_SECURE_BUILD
    setupMqtt = mqtt;
    String mqttSecret;
    if (AgentCredentialStore::mqttProvisioningAttempted() && !AgentCredentialStore::loadMqtt(mqttSecret))
        provisioning.restoreMqttFailure();
    WiFi.mode(WIFI_AP_STA);
#else
    (void)mqtt;
    WiFi.mode(WIFI_AP);
#endif

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
#if EDOL_AGENT_SECURE_BUILD
    server.on("/status", HTTP_GET, []() {
        server.sendHeader("Cache-Control", "no-store");
        server.send(200, "text/plain", String(provisioning.stageName()) + ". "
            + provisioningMessage(provisioning.result()));
    });
    server.on("/retry", HTTP_POST, []() {
        provisioning.retry(millis());
        server.sendHeader("Cache-Control", "no-store");
        server.send(200, "text/html", buildSetupPage());
    });
#endif
    server.on("/", HTTP_GET, []()
              { server.sendHeader("Cache-Control", "no-store"); server.send(
                    200,
                    "text/html",
                    buildSetupPage()); });

    server.on("/save", HTTP_POST, []()
              {
#if EDOL_AGENT_SECURE_BUILD
                  String pairingCode = server.arg("pairingCode");
                  AgentIdentity existingIdentity;
                  const bool existing = AgentCredentialStore::load(existingIdentity);
                  if ((pairingCode.length() && !PairingProtocol::validCode(pairingCode.c_str()))
                      || (!pairingCode.length() && !existing && !provisioning.hasCode()))
                  {
                      server.send(400, "text/plain", "Enter a fresh 12-character pairing code. Saved settings are retained; use Back to edit the same form.");
                      return;
                  }
                  const long corePort = server.arg("corePort").toInt();
                  const long mqttPort = server.arg("edolMqttPort").toInt();
                  if (!PairingProtocol::validHost(server.arg("coreHost").c_str())
                      || !PairingProtocol::validHost(server.arg("edolMqttHost").c_str())
                      || corePort < 1 || corePort > 65535 || mqttPort < 1 || mqttPort > 65535
                      || server.arg("ssid").length() == 0
                      || !PairingProtocol::validHost(server.arg("printerHost").c_str())
                      || server.arg("printerSerial").length() == 0 || server.arg("accessCode").length() == 0)
                  {
                      server.send(400, "text/plain", "Enter Wi-Fi SSID, printer host/serial/access code and valid Core/MQTT hosts and ports (1-65535), without scheme prefixes. Use Back to edit the same form.");
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
                  if (setupMqtt != nullptr) setupMqtt->disconnect();
                  WiFi.disconnect();
                  provisioning.start(pairingCode.c_str(), existing, millis());
                  // Wipe our copy; WebServer owns its transient HTTP parsing buffers.
                  volatile char *codeBytes = const_cast<char *>(pairingCode.c_str());
                  for (size_t i = 0; i < pairingCode.length(); ++i) codeBytes[i] = 0;
                  pairingCode = "";
                  nextStep = millis() + 1000;
                  rebootAt = 0;
                  server.sendHeader("Cache-Control", "no-store");
                  server.send(200, "text/html", buildSetupPage());
#else

                  server.send(
                      200,
                      "text/html",
                      "<h1>Configuration saved.</h1><p>Rebooting...</p>");

                  delay(1000);

                  ESP.restart();
#endif
              });

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
        server.sendHeader("Cache-Control", "no-store");
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
#if EDOL_AGENT_SECURE_BUILD
    if (static_cast<int32_t>(millis() - nextStep) >= 0)
    {
        provisioning.tick(provisioningActions, millis());
        // Give the browser a chance to observe each stage before a bounded network call.
        nextStep = millis() + 1000;
    }
    if (provisioning.stage() == SetupStage::Ready)
    {
        if (rebootAt == 0) rebootAt = millis() + 3000;
        if (static_cast<int32_t>(millis() - rebootAt) >= 0) ESP.restart();
    }
#endif
}
