#include "SecureTransport.h"

#if EDOL_AGENT_SECURE_BUILD
#include <Arduino.h>
#include <time.h>
#include <esp_sntp.h>
#include "ClockSyncState.h"
#if EDOL_AGENT_SMOKE_BUILD
#include "PairingProtocol.h"
#include "../config/ConfigManager.h"
#include <mbedtls/x509.h>
#endif

namespace
{
ClockSyncState clockState;
bool ntpStarted = false;

void synchronizedTime(struct timeval *value)
{
    clockState.record(static_cast<int64_t>(value->tv_sec), millis());
}

bool clockReady()
{
    return clockState.ready(static_cast<int64_t>(time(nullptr)), millis(), EDOL_AGENT_BUILD_EPOCH);
}
}

bool SecureTransport::synchronizeClock()
{
    if (clockReady()) return true;
    sntp_set_time_sync_notification_cb(synchronizedTime);
    ntpStarted = true;
    configTime(0, 0, "pool.ntp.org", "time.cloudflare.com");
    uint32_t started = millis();
    while (!clockReady() && millis() - started < 15000) delay(100);
    const bool ready = clockReady();
    Serial.printf("Secure clock ntp=%s utc_epoch=%lld\n", ready ? "synchronized" : "timeout",
                  static_cast<long long>(time(nullptr)));
    return ready;
}

bool SecureTransport::prepare(Client &client)
{
    if (!clockReady()) return false;
#if EDOL_AGENT_SMOKE_BUILD
    client.resetDiagnostics();
#endif
    client.setCACert(EDOL_AGENT_ROOT_CA);
    client.setHandshakeTimeout(10);
    client.setTimeout(10000);
    return true;
}

#if EDOL_AGENT_SMOKE_BUILD
void SecureTransport::Client::stop()
{
    if (!flagsKnown_ && _lastError == MBEDTLS_ERR_X509_CERT_VERIFY_FAILED && sslclient != nullptr)
    {
        const uint32_t flags = mbedtls_ssl_get_verify_result(&sslclient->ssl_ctx);
        if (flags != 0 && flags != UINT32_MAX)
        {
            flags_ = flags;
            flagsKnown_ = true;
        }
    }
    WiFiClientSecure::stop();
}

void SecureTransport::reportClock(const char *stage)
{
    Serial.printf("SMOKE clock stage=%s utc_epoch=%lld build_epoch=%ld ntp_started=%u ntp_observed=%u ready=%u ca_pem_sha256=%s\n",
                  stage, static_cast<long long>(time(nullptr)), EDOL_AGENT_BUILD_EPOCH,
                  ntpStarted, clockState.observed(), clockReady(), EDOL_AGENT_ROOT_CA_SHA256);
}

void SecureTransport::reportFailure(Client &client, const char *stage, const char *host, uint16_t port)
{
    char ignored[1];
    const int error = client.lastError(ignored, sizeof(ignored));
    reportClock(stage);
    if (!client.flagsKnown())
    {
        Serial.printf("SMOKE TLS stage=%s host=%s port=%u error=%d verify_flags=unavailable\n", stage, host, port, error);
        return;
    }
    const uint32_t flags = client.verificationFlags();
    Serial.printf("SMOKE TLS stage=%s host=%s port=%u error=%d verify_flags=0x%08lx expired=%u future=%u name=%u trust=%u\n",
                  stage, host, port, error, static_cast<unsigned long>(flags),
                  (flags & MBEDTLS_X509_BADCERT_EXPIRED) != 0, (flags & MBEDTLS_X509_BADCERT_FUTURE) != 0,
                  (flags & MBEDTLS_X509_BADCERT_CN_MISMATCH) != 0, (flags & MBEDTLS_X509_BADCERT_NOT_TRUSTED) != 0);
}

void SecureTransport::probeCore()
{
    const String host = ConfigManager::getCoreHost();
    const uint16_t port = ConfigManager::getCorePort();
    if (!PairingProtocol::validHost(host.c_str()) || port == 0)
    {
        Serial.println("SMOKE TLS target invalid");
        return;
    }
    Client client;
    if (!synchronizeClock() || !prepare(client))
    {
        reportClock("probe-clock");
        return;
    }
    if (client.connect(host.c_str(), port))
    {
        reportClock("probe");
        Serial.printf("SMOKE TLS host=%s port=%u handshake=verified no_http_request=1\n", host.c_str(), port);
    }
    else reportFailure(client, "probe", host.c_str(), port);
    client.stop();
}
#endif
#endif
