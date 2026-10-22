#include "AgentCredentialStore.h"
#include "PairingProtocol.h"
#include <Preferences.h>

namespace
{
constexpr const char *NAMESPACE = "edol_agent_sec";
}

bool AgentCredentialStore::load(AgentIdentity &identity)
{
    identity.id = "";
    identity.secret = "";
    Preferences preferences;
    if (!preferences.begin(NAMESPACE, true)) return false;
    bool ready = preferences.getBool("ready", false);
    String id = preferences.getString("id", "");
    String secret = preferences.getString("secret", "");
    preferences.end();
    if (!ready || !PairingProtocol::validIdentity(id.c_str(), secret.c_str())) return false;
    identity.id = id;
    identity.secret = secret;
    return true;
}

bool AgentCredentialStore::save(const char *id, const char *secret)
{
    if (!PairingProtocol::validIdentity(id, secret)) return false;
    Preferences preferences;
    if (!preferences.begin(NAMESPACE, false)) return false;
    // Invalidate before replacing either value; interrupted writes never become a usable mixed identity.
    bool saved = preferences.putBool("ready", false) == 1
        && preferences.putBool("mqtt_ready", false) == 1
        && preferences.putBool("mqtt_tried", false) == 1
        && preferences.putString("id", id) == std::strlen(id)
        && preferences.putString("secret", secret) == std::strlen(secret)
        && preferences.putBool("ready", true) == 1;
    preferences.end();
    return saved;
}

bool AgentCredentialStore::loadMqtt(String &secret)
{
    secret = "";
    AgentIdentity identity;
    if (!load(identity)) return false;
    Preferences preferences;
    if (!preferences.begin(NAMESPACE, true)) return false;
    bool ready = preferences.getBool("mqtt_ready", false);
    String value = preferences.getString("mqtt_secret", "");
    preferences.end();
    if (!ready || !PairingProtocol::validIdentity(identity.id.c_str(), value.c_str())) return false;
    secret = value;
    return true;
}

bool AgentCredentialStore::beginMqttProvisioning()
{
    Preferences preferences;
    if (!preferences.begin(NAMESPACE, false)) return false;
    bool fresh = !preferences.getBool("mqtt_tried", false)
        && preferences.putBool("mqtt_tried", true) == 1;
    preferences.end();
    return fresh;
}

bool AgentCredentialStore::saveMqtt(const char *secret)
{
    AgentIdentity identity;
    if (!load(identity) || !PairingProtocol::validIdentity(identity.id.c_str(), secret)) return false;
    Preferences preferences;
    if (!preferences.begin(NAMESPACE, false)) return false;
    bool saved = preferences.putBool("mqtt_ready", false) == 1
        && preferences.putString("mqtt_secret", secret) == std::strlen(secret)
        && preferences.putBool("mqtt_ready", true) == 1;
    preferences.end();
    return saved;
}

bool AgentCredentialStore::clear()
{
    Preferences preferences;
    if (!preferences.begin(NAMESPACE, false)) return false;
    bool cleared = preferences.clear();
    preferences.end();
    return cleared;
}
