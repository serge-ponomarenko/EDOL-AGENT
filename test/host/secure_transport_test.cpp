#include <cassert>
#include <iostream>
#include <time.h>
#include <esp_sntp.h>
#include "security/ClockSyncState.h"
#include "security/SecureTransport.h"
#include "config/ConfigManager.h"

FakeSerial Serial;
FakeSocket socketState;
uint32_t fakeMillis = 0;
int fakeTlsError = 0;
uint32_t fakeVerifyFlags = 0;
unsigned ntpAttempts = 0;
bool ntpReplies = false;
sntp_sync_time_cb_t notification = nullptr;

String ConfigManager::getCoreHost() { return "core.example.invalid"; }
uint16_t ConfigManager::getCorePort() { return 443; }

void sntp_set_time_sync_notification_cb(sntp_sync_time_cb_t callback) { notification = callback; }
void configTime(long, int, const char *, const char *)
{
    ++ntpAttempts;
    if (ntpReplies)
    {
        struct timeval value{static_cast<long>(time(nullptr)), 0};
        notification(&value);
    }
}

int main()
{
    ClockSyncState policy;
    constexpr int64_t build = 1792756800;
    assert(!policy.ready(build + 3600, 0, build)); // Plausible RTC is not SNTP evidence.
    policy.record(build, 1000);
    assert(policy.ready(build + 2, 3000, build));
    assert(!policy.ready(build + 3600, 3000, build)); // Forward jump.
    assert(!policy.ready(build - 3600, 3000, build)); // Backward jump.
    ClockSyncState stale;
    stale.record(build - 86401, 0);
    assert(!stale.ready(build - 86401, 0, build));
    ClockSyncState rollover;
    rollover.record(build, UINT32_MAX - 499);
    assert(rollover.ready(build + 1, 500, build));

    class InspectClient : public SecureTransport::Client
    {
    public:
        FakeTlsContext &context() { return sslclient->ssl_ctx; }
    } client;
    InspectClient other;
    assert(client.connect("192.0.2.1", 443));
    assert(other.connect("192.0.2.2", 443, 5000));
    assert(client.context().verify != nullptr && other.context().verify != nullptr);
    assert(client.context().context != other.context().context);
    unsigned char matchingIp[] = {192, 0, 2, 1};
    mbedtls_x509_crt leaf;
    leaf.subject_alt_names.buf.tag = MBEDTLS_ASN1_CONTEXT_SPECIFIC | MBEDTLS_X509_SAN_IP_ADDRESS;
    leaf.subject_alt_names.buf.len = 4;
    leaf.subject_alt_names.buf.p = matchingIp;
    leaf.raw.len = 4;
    leaf.raw.p = matchingIp;
    uint32_t nameFlags = MBEDTLS_X509_BADCERT_CN_MISMATCH;
    client.context().verify(client.context().context, &leaf, 0, &nameFlags);
    assert(nameFlags == 0);
    other.context().verify(other.context().context, &leaf, 0, &nameFlags);
    assert(nameFlags == MBEDTLS_X509_BADCERT_CN_MISMATCH);
    assert(Serial.output.find("peer_leaf_sha256=abab") != std::string::npos);
    assert(client.connect("core.example.invalid", 443));
    nameFlags = MBEDTLS_X509_BADCERT_CN_MISMATCH;
    client.context().verify(client.context().context, &leaf, 0, &nameFlags);
    assert(nameFlags == MBEDTLS_X509_BADCERT_CN_MISMATCH); // DNS retains native verification.
    assert(!client.connect("192.0.2.001", 443));
    assert(!SecureTransport::prepare(client));
    assert(!SecureTransport::synchronizeClock());
    assert(ntpAttempts == 1 && fakeMillis == 15000);
    assert(!SecureTransport::prepare(client));
    assert(Serial.output.find("ntp=timeout") != std::string::npos);
    ntpReplies = true;
    assert(SecureTransport::synchronizeClock());
    assert(ntpAttempts == 2);
    assert(SecureTransport::synchronizeClock());
    assert(ntpAttempts == 2); // Reuse only a confirmed clock, not merely its lower bound.

    // Upload abort -> prepare -> connect -> finish/abort may repeat on one client.
    // Enrollment HTTPClient.end() and explicit cleanup may also both call stop().
    for (unsigned attempt = 0; attempt < 25; ++attempt)
    {
        client.stop();
        client.stop();
        assert(SecureTransport::prepare(client));
        assert(client.rootCA != nullptr && std::strcmp(client.rootCA, EDOL_AGENT_ROOT_CA) == 0);
        assert(client.timeoutSeconds == 10);
        assert(client.connect("192.0.2.1", 443));
        nameFlags = MBEDTLS_X509_BADCERT_CN_MISMATCH | MBEDTLS_X509_BADCERT_NOT_TRUSTED;
        client.context().verify(client.context().context, &leaf, 0, &nameFlags);
        assert(nameFlags == MBEDTLS_X509_BADCERT_NOT_TRUSTED);
        client.stop();
    }
    assert(fakeForeignSocketCloses() == 0 && fakeInvalidSocketOptions() == 0);

    for (uint32_t flag : {uint32_t(MBEDTLS_X509_BADCERT_EXPIRED), uint32_t(MBEDTLS_X509_BADCERT_FUTURE),
                          uint32_t(MBEDTLS_X509_BADCERT_CN_MISMATCH), uint32_t(MBEDTLS_X509_BADCERT_NOT_TRUSTED)})
    {
        assert(SecureTransport::prepare(client));
        fakeTlsError = MBEDTLS_ERR_X509_CERT_VERIFY_FAILED;
        fakeVerifyFlags = flag;
        assert(!client.connect("core.example.invalid", 443));
        assert(client.flagsKnown() && client.verificationFlags() == flag);
        client.stop(); // Repeated cleanup must not erase the failed handshake evidence.
        assert(client.verificationFlags() == flag);
        Serial.output.clear();
        SecureTransport::reportFailure(client, "enrollment", "core.example.invalid", 443);
        assert(Serial.output.find("verify_flags=0x") != std::string::npos);
        assert(Serial.output.find("utc_epoch=") != std::string::npos);
        assert(Serial.output.find("ca_pem_sha256=offline-fixture-not-a-real-CA") != std::string::npos);
    }
    assert(SecureTransport::prepare(client));
    fakeVerifyFlags = UINT32_MAX;
    assert(!client.connect("core.example.invalid", 443));
    assert(!client.flagsKnown());
    Serial.output.clear();
    SecureTransport::reportFailure(client, "probe", "core.example.invalid", 443);
    assert(Serial.output.find("verify_flags=unavailable") != std::string::npos);
    assert(Serial.output.find("verify_flags=0x00000000") == std::string::npos);
    fakeTlsError = 0;
    fakeVerifyFlags = 0;
    Serial.output.clear();
    SecureTransport::probeCore();
    assert(Serial.output.find("handshake=verified no_http_request=1") != std::string::npos);
    assert(socketState.headers.empty() && socketState.body.empty());
    std::cout << "Clock synchronization and retained TLS flags passed (fake SNTP/TLS; no hardware acceptance)\n";
}
