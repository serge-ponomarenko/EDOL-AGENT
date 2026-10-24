#include "SecureTransport.h"

#if EDOL_AGENT_SECURE_BUILD
#include "IpSanIdentity.h"
#include <mbedtls/version.h>

#if MBEDTLS_VERSION_NUMBER != 0x021C0700
#error "Review IP SAN verification and callback lifecycle before changing the audited mbedTLS 2.28.7 SDK"
#endif

void SecureTransport::Client::stop()
{
#if EDOL_AGENT_SMOKE_BUILD
    if (!flagsKnown_ && _lastError == MBEDTLS_ERR_X509_CERT_VERIFY_FAILED && sslclient != nullptr)
    {
        const uint32_t flags = mbedtls_ssl_get_verify_result(&sslclient->ssl_ctx);
        if (flags != 0 && flags != UINT32_MAX)
        {
            flags_ = flags;
            flagsKnown_ = true;
        }
    }
#endif
    SocketTlsClient::stop();
}

int SecureTransport::Client::connect(const char *host, uint16_t port)
{
    // Preserve the invalid descriptor sentinel across the SDK's zeroing cleanup.
    stop();
    _lastError = 0;
#if EDOL_AGENT_SMOKE_BUILD
    resetDiagnostics();
#endif
    const bool ipIdentity = IpSanIdentity::parse(host, expectedIp_);
    if (ipIdentity)
        mbedtls_ssl_set_verify(&sslclient->ssl_ctx, IpSanIdentity::verify, expectedIp_);
    else
    {
        if (host == nullptr || *host == '\0' || IpSanIdentity::numericHost(host))
        {
            _lastError = MBEDTLS_ERR_X509_CERT_VERIFY_FAILED;
            return 0;
        }
        mbedtls_ssl_set_verify(&sslclient->ssl_ctx, nullptr, nullptr);
    }
#if EDOL_AGENT_SMOKE_BUILD
    ipIdentity_ = ipIdentity;
    mbedtls_ssl_set_verify(&sslclient->ssl_ctx, inspectCertificate, this);
    Serial.printf("SMOKE TLS connect host=%s port=%u\n", host, port);
    reportMemory("before-handshake");
#endif
    // The original host is still passed to set_hostname; VERIFY_REQUIRED remains enabled.
    const int connected = WiFiClientSecure::connect(host, port);
#if EDOL_AGENT_SMOKE_BUILD
    reportMemory("after-handshake");
#endif
    return connected;
}

int SecureTransport::Client::connect(const char *host, uint16_t port, int32_t timeout)
{
    _timeout = timeout;
    return connect(host, port);
}

int SecureTransport::Client::connect(IPAddress address, uint16_t port)
{
    const String host = address.toString();
    return connect(host.c_str(), port);
}

int SecureTransport::Client::connect(IPAddress address, uint16_t port, int32_t timeout)
{
    _timeout = timeout;
    return connect(address, port);
}
#endif
