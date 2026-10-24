#include <cassert>
#include <iostream>
#include <esp_sntp.h>
#include <time.h>
#include "http/HttpUploadClient.h"
#include "security/AgentEnrollmentClient.h"
#include "security/AgentCredentialStore.h"
#include "security/SecureTransport.h"
#include "config/ConfigManager.h"

FakeSerial Serial;
FakeSocket socketState;
FakeHttpState httpState;
uint32_t fakeMillis = 0;
int fakeTlsError = 0;
uint32_t fakeVerifyFlags = 0;
sntp_sync_time_cb_t notification = nullptr;
bool paired = false;
String ConfigManager::getCoreHost() { return "192.0.2.1"; }
uint16_t ConfigManager::getCorePort() { return 18443; }
void sntp_set_time_sync_notification_cb(sntp_sync_time_cb_t callback) { notification = callback; }
void configTime(long, int, const char *, const char *)
{
    struct timeval value{static_cast<long>(time(nullptr)), 0};
    notification(&value);
}
bool AgentCredentialStore::save(const char *, const char *) { paired = true; return true; }
bool AgentCredentialStore::load(AgentIdentity &identity)
{
    identity.id = "00000000-0000-4000-8000-000000000001";
    identity.secret = "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA";
    return paired;
}

int main()
{
    httpState.response = "{\"agentId\":\"00000000-0000-4000-8000-000000000001\","
                         "\"secret\":\"AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA\"}";
    assert(AgentEnrollmentClient::enroll("0123456789AB"));
    assert(paired && httpState.posts == 1);
    HttpUploadClient upload;
    const uint8_t jpeg[] = {0xFF, 0xD8, 0xFF, 0xD9};
    for (unsigned attempt = 0; attempt < 3; ++attempt)
    {
        socketState = FakeSocket{};
        assert(upload.beginSnapshotUpload("unused", sizeof(jpeg)));
        assert(upload.write(jpeg, sizeof(jpeg)) == sizeof(jpeg));
        assert(upload.finish());
        upload.abort();
    }
    // The same TLS policy must reject all four certificate failures in both paths.
    for (uint32_t flags : {uint32_t(MBEDTLS_X509_BADCERT_NOT_TRUSTED), uint32_t(MBEDTLS_X509_BADCERT_EXPIRED),
                           uint32_t(MBEDTLS_X509_BADCERT_FUTURE), uint32_t(MBEDTLS_X509_BADCERT_CN_MISMATCH)})
    {
        fakeTlsError = MBEDTLS_ERR_X509_CERT_VERIFY_FAILED;
        fakeVerifyFlags = flags;
        int posts = httpState.posts;
        assert(!AgentEnrollmentClient::enroll("0123456789AB"));
        assert(httpState.posts == posts); // Never send the code after a failed handshake.
        socketState = FakeSocket{};
        assert(!upload.beginSnapshotUpload("unused", sizeof(jpeg)));
        assert(socketState.headers.empty() && socketState.body.empty());
        upload.abort();
    }
    fakeTlsError = 0;
    fakeVerifyFlags = 0;
    socketState = FakeSocket{};
    assert(upload.beginSnapshotUpload("unused", sizeof(jpeg)));
    assert(upload.write(jpeg, sizeof(jpeg)) == sizeof(jpeg));
    assert(upload.finish());
    assert(paired); // Network/certificate failures do not clear identity or enter setup.
    assert(fakeForeignSocketCloses() == 0 && fakeInvalidSocketOptions() == 0);
    std::cout << "Enrollment/upload lifecycle, repeated cleanup and certificate failures PASS (fake TLS)\n";
}
