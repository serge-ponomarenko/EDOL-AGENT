#pragma once
#include <cstdint>

namespace FtpReply
{
inline int code(const char *reply)
{
    if (reply == nullptr || reply[0] < '1' || reply[0] > '5'
        || reply[1] < '0' || reply[1] > '9' || reply[2] < '0' || reply[2] > '9') return -1;
    return (reply[0] - '0') * 100 + (reply[1] - '0') * 10 + reply[2] - '0';
}

// RFC 1123: human-readable PASV text and parentheses are not a fixed prefix.
inline bool passive(const char *reply, uint8_t (&tuple)[6])
{
    if (code(reply) != 227) return false;
    const char *cursor = reply + 3;
    while (*cursor && (*cursor < '0' || *cursor > '9')) ++cursor;
    for (unsigned part = 0; part < 6; ++part)
    {
        unsigned value = 0, digits = 0;
        while (*cursor >= '0' && *cursor <= '9')
        {
            if (++digits > 3) return false;
            value = value * 10 + static_cast<unsigned>(*cursor++ - '0');
        }
        if (digits == 0 || value > 255) return false;
        tuple[part] = static_cast<uint8_t>(value);
        if (part < 5 && *cursor++ != ',') return false;
    }
    if (*cursor != '\0' && *cursor != ')' && *cursor != ' ' && *cursor != '.') return false;
    return tuple[4] != 0 || tuple[5] != 0;
}
}
