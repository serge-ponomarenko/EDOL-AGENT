#pragma once

#include <cstdint>
#include <cstring>
#include <mbedtls/x509_crt.h>

namespace IpSanIdentity
{
// Accept canonical IPv4 only: no shorthand, octal, wildcard or DNS interpretation.
inline bool parse(const char *host, unsigned char (&address)[4])
{
    if (host == nullptr) return false;
    const char *cursor = host;
    for (unsigned part = 0; part < 4; ++part)
    {
        const char *first = cursor;
        unsigned value = 0, digits = 0;
        while (*cursor >= '0' && *cursor <= '9')
        {
            if (++digits > 3) return false;
            value = value * 10 + static_cast<unsigned>(*cursor++ - '0');
        }
        if (digits == 0 || value > 255 || (digits > 1 && *first == '0')) return false;
        address[part] = static_cast<unsigned char>(value);
        if (part < 3) { if (*cursor++ != '.') return false; }
        else if (*cursor != '\0') return false;
    }
    return true;
}

inline bool numericHost(const char *host)
{
    if (host == nullptr || *host == '\0') return false;
    for (; *host; ++host) if ((*host < '0' || *host > '9') && *host != '.') return false;
    return true;
}

// mbedTLS 2.28.7 retains raw SAN entries, but its native name matcher ignores iPAddress.
// Replace only the leaf's name decision; preserve every trust/time/usage/profile flag.
inline int verify(void *expected, mbedtls_x509_crt *certificate, int depth, uint32_t *flags)
{
    if (depth != 0) return 0;
    bool matches = false;
    if (expected != nullptr && certificate != nullptr)
    {
        for (const mbedtls_x509_sequence *san = &certificate->subject_alt_names; san != nullptr; san = san->next)
        {
            if (san->buf.tag == (MBEDTLS_ASN1_CONTEXT_SPECIFIC | MBEDTLS_X509_SAN_IP_ADDRESS)
                && san->buf.len == 4 && san->buf.p != nullptr
                && std::memcmp(san->buf.p, expected, 4) == 0)
            {
                matches = true;
                break;
            }
        }
    }
    if (matches) *flags &= ~static_cast<uint32_t>(MBEDTLS_X509_BADCERT_CN_MISMATCH);
    else *flags |= MBEDTLS_X509_BADCERT_CN_MISMATCH;
    return 0;
}
}
