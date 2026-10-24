#include "SecureTransport.h"

#if EDOL_AGENT_SECURE_BUILD
#include "IpSanIdentity.h"
#include <mbedtls/version.h>

#if MBEDTLS_VERSION_NUMBER != 0x021C0700
#error "Review IP SAN verification and callback lifecycle before changing the audited mbedTLS 2.28.7 SDK"
#endif

int SecureTransport::Client::connect(const char *host, uint16_t port)
{
    // Base cleanup preserves CA configuration, but resets the previous session callback.
    WiFiClientSecure::stop();
    _lastError = 0;
#if EDOL_AGENT_SMOKE_BUILD
    resetDiagnostics();
#endif
    if (IpSanIdentity::parse(host, expectedIp_))
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
    // The original host is still passed to set_hostname; VERIFY_REQUIRED remains enabled.
    return WiFiClientSecure::connect(host, port);
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
