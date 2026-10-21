#pragma once

#include <WiFiClientSecure.h>

#include <functional>

class CameraClient
{
public:
    bool connect();
    bool authenticate();
    size_t readSnapshotSize();
    void disconnect();

    bool streamSnapshot(
        uint32_t payloadSize,
        std::function<bool(const uint8_t *, size_t)> handler);

private:
    WiFiClientSecure client_;

    bool readFully(
        uint8_t *buffer,
        size_t size);
};