#pragma once
#include <cstddef>
#include <cstring>
// Diagnostics-only fake, not a cryptographic verification implementation.
inline int mbedtls_sha256_ret(const unsigned char *, size_t, unsigned char output[32], int)
{
    std::memset(output, 0xAB, 32);
    return 0;
}
