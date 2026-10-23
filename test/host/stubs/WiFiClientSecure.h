#pragma once
#include "WiFiClient.h"
#if EDOL_AGENT_SMOKE_BUILD
#include <mbedtls/x509.h>
struct FakeTlsContext { uint32_t verifyFlags = 0; };
struct sslclient_context { FakeTlsContext ssl_ctx; };
inline uint32_t mbedtls_ssl_get_verify_result(FakeTlsContext *context) { return context->verifyFlags; }
extern int fakeTlsError;
extern uint32_t fakeVerifyFlags;
#endif
class IPAddress {};
class WiFiClientSecure : public WiFiClient
{
public:
#if EDOL_AGENT_SMOKE_BUILD
    bool connect(const char *, uint16_t)
    {
        _lastError = fakeTlsError;
        storage_.ssl_ctx.verifyFlags = fakeVerifyFlags;
        if (_lastError < 0) { stop(); return false; }
        socketState.connected = true;
        return true;
    }
    virtual void stop()
    {
        storage_.ssl_ctx.verifyFlags = 0;
        WiFiClient::stop();
    }
    int lastError(char *, size_t) { return _lastError; }
protected:
    sslclient_context storage_;
    sslclient_context *sslclient = &storage_;
    int _lastError = 0;
public:
#else
    int lastError(char *, size_t) { return -1; }
#endif
    void setCACert(const char *) {}
    void setHandshakeTimeout(unsigned long) {}
    void setTimeout(unsigned long) {}
};
