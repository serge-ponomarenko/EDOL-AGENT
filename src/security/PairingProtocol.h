#pragma once

#include <cstring>

namespace PairingProtocol
{
inline bool validCode(const char *value)
{
    if (value == nullptr || std::strlen(value) != 12) return false;
    const char *alphabet = "0123456789ABCDEFGHJKMNPQRSTVWXYZ";
    for (size_t index = 0; index < 12; ++index)
    {
        char character = value[index];
        if (character >= 'a' && character <= 'z') character -= 'a' - 'A';
        if (std::strchr(alphabet, character) == nullptr) return false;
    }
    return true;
}

inline bool validIdentity(const char *id, const char *secret)
{
    if (id == nullptr || secret == nullptr || std::strlen(id) != 36 || std::strlen(secret) != 43) return false;
    for (size_t index = 0; index < 36; ++index)
    {
        char character = id[index];
        bool separator = index == 8 || index == 13 || index == 18 || index == 23;
        bool hex = (character >= '0' && character <= '9') || (character >= 'a' && character <= 'f')
            || (character >= 'A' && character <= 'F');
        if (separator ? character != '-' : !hex) return false;
    }
    for (size_t index = 0; index < 43; ++index)
    {
        char character = secret[index];
        if (!((character >= '0' && character <= '9') || (character >= 'a' && character <= 'z')
            || (character >= 'A' && character <= 'Z') || character == '_' || character == '-')) return false;
    }
    return true;
}

inline bool validHost(const char *host)
{
    if (host == nullptr || host[0] == '\0' || std::strlen(host) > 253) return false;
    for (const char *position = host; *position != '\0'; ++position)
    {
        char character = *position;
        if (!((character >= '0' && character <= '9') || (character >= 'a' && character <= 'z')
            || (character >= 'A' && character <= 'Z') || character == '-' || character == '.')) return false;
    }
    return true;
}
}
