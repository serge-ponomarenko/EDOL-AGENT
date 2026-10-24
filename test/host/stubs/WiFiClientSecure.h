#pragma once
#include "WiFiClient.h"
#include <mbedtls/x509_crt.h>
using FakeVerifyCallback = int (*)(void *, mbedtls_x509_crt *, int, uint32_t *);
struct FakeTlsContext { uint32_t verifyFlags = 0; FakeVerifyCallback verify = nullptr; void *context = nullptr; };
struct sslclient_context { FakeTlsContext ssl_ctx; int socket = -1; };
inline unsigned &fakeForeignSocketCloses() { static unsigned count = 0; return count; }
inline unsigned &fakeInvalidSocketOptions() { static unsigned count = 0; return count; }
inline uint32_t mbedtls_ssl_get_verify_result(FakeTlsContext *context) { return context->verifyFlags; }
inline void mbedtls_ssl_set_verify(FakeTlsContext *ssl, FakeVerifyCallback verify, void *context)
{ ssl->verify = verify; ssl->context = context; }
#if EDOL_AGENT_SMOKE_BUILD
extern int fakeTlsError;
extern uint32_t fakeVerifyFlags;
#endif
class IPAddress { public: String toString() const { return "192.0.2.1"; } };
class WiFiClientSecure : public WiFiClient
{
public:
    virtual int connect(const char *host, uint16_t port)
    {
        storage_.socket = 3;
#if EDOL_AGENT_SMOKE_BUILD
        (void)host;
        (void)port;
        _lastError = fakeTlsError;
        storage_.ssl_ctx.verifyFlags = fakeVerifyFlags;
        if (_lastError < 0) { stop(); return false; }
        socketState.connected = true;
        return true;
#else
        return WiFiClient::connect(host, port);
#endif
    }
    virtual int connect(const char *host, uint16_t port, int32_t) { return connect(host, port); }
    virtual int connect(IPAddress address, uint16_t port) { return connect(address.toString().c_str(), port); }
    virtual int connect(IPAddress address, uint16_t port, int32_t) { return connect(address, port); }
    virtual void stop()
    {
        if (storage_.socket == 0) ++fakeForeignSocketCloses();
        storage_.ssl_ctx = FakeTlsContext{};
        // Exact SDK stop_ssl_socket() behavior: memset loses the -1 sentinel.
        storage_.socket = 0;
        WiFiClient::stop();
    }
    int lastError(char *, size_t) { return _lastError; }
protected:
    sslclient_context storage_;
    sslclient_context *sslclient = &storage_;
    int _lastError = 0;
    int _timeout = 0;
public:
    const char *rootCA = nullptr;
    unsigned timeoutSeconds = 0;
    void setInsecure() {}
    void setCACert(const char *certificate) { rootCA = certificate; }
    void setHandshakeTimeout(unsigned long) {}
    void setTimeout(unsigned long seconds) {
        timeoutSeconds = static_cast<unsigned>(seconds);
        if (storage_.socket == 0) ++fakeInvalidSocketOptions();
    }
};
