#include "../util/BulkAdmission.h"
#include "CameraClient.h"

#include <Arduino.h>
#include <functional>

#include "../config/Config.h"
#include "../config/ConfigManager.h"

namespace
{
    constexpr uint16_t CAMERA_PORT = 6000;
    constexpr size_t AUTH_PACKET_SIZE = 80;
    static constexpr uint32_t CAMERA_TIMEOUT_MS = 10000;
}

bool CameraClient::connect()
{
    client_.stop();

    if (!bulkAdmission("camera")) return false;
    client_.setHandshakeTimeout(10);
    client_.setTimeout(10);
    client_.setInsecure();

    if (!client_.connect(ConfigManager::getBambuPrinterHost().c_str(), CAMERA_PORT))
    {
        Serial.println("Camera connection failed");
        return false;
    }

    Serial.println("Camera connected");

    return true;
}

bool CameraClient::authenticate()
{
    uint8_t packet[AUTH_PACKET_SIZE] = {0};

    auto writeUint32LE = [](uint8_t *buffer, uint32_t value)
    {
        buffer[0] = value & 0xFF;
        buffer[1] = (value >> 8) & 0xFF;
        buffer[2] = (value >> 16) & 0xFF;
        buffer[3] = (value >> 24) & 0xFF;
    };

    writeUint32LE(packet + 0, 0x40);
    writeUint32LE(packet + 4, 0x3000);
    writeUint32LE(packet + 8, 0);
    writeUint32LE(packet + 12, 0);

    memcpy(packet + 16, "bblp", 4);

    String accessCodeStr = ConfigManager::getBambuAccessCode();
    const char *accessCode = accessCodeStr.c_str();

    size_t passwordLength =
        std::min(
            strlen(accessCode),
            static_cast<size_t>(32));

    memcpy(
        packet + 48,
        accessCode,
        passwordLength);

    size_t written = client_.write(
        packet,
        sizeof(packet));

    client_.flush();

    if (written != sizeof(packet))
    {
        Serial.println("Camera auth packet write failed");
        return false;
    }

    return true;
}

size_t CameraClient::readSnapshotSize()
{
    uint32_t start = millis();

    while (client_.available() == 0)
    {

        if (millis() - start > CAMERA_TIMEOUT_MS)
        {
            Serial.println("Camera response timeout");
            return 0;
        }

        delay(10);
    }

    uint8_t header[16];

    if (!readFully(header, sizeof(header)))
    {
        Serial.println("Failed to read frame header");
        return 0;
    }

    uint32_t payloadSize =
        static_cast<uint32_t>(header[0]) |
        (static_cast<uint32_t>(header[1]) << 8) |
        (static_cast<uint32_t>(header[2]) << 16) |
        (static_cast<uint32_t>(header[3]) << 24);

    return payloadSize;
}

void CameraClient::disconnect()
{
    client_.stop();
}

bool CameraClient::readFully(
    uint8_t *buffer,
    size_t size)
{
    size_t offset = 0;
    uint32_t lastProgress = millis();

    while (offset < size)
    {

        int read =
            client_.read(
                buffer + offset,
                size - offset);

        if (read < 0)
        {
            return false;
        }

        if (read == 0)
        {
            if (!client_.connected() || millis() - lastProgress > CAMERA_TIMEOUT_MS)
            {
                return false;
            }
            delay(10);
            continue;
        }

        offset += read;
        lastProgress = millis();
    }

    return true;
}

bool CameraClient::streamSnapshot(
    uint32_t payloadSize,
    std::function<bool(const uint8_t *, size_t)> handler)
{
    uint32_t remaining = payloadSize;
    if (remaining == 0 || remaining > 256 * 1024) return false;
    const uint32_t started = millis();

    uint8_t buffer[1024];

    while (remaining > 0)
    {
        if (millis() - started >= 30000) return false;
        uint32_t waitStart = millis();

        while (client_.available() == 0)
        {
            if (!client_.connected())
            {
                Serial.println("Camera socket disconnected");
                return false;
            }

            if (millis() - waitStart > CAMERA_TIMEOUT_MS)
            {
                Serial.println("Camera read timeout");
                return false;
            }

            delay(10);
        }

        size_t chunkSize =
            std::min(
                remaining,
                static_cast<uint32_t>(sizeof(buffer)));

        int bytesRead =
            client_.read(
                buffer,
                chunkSize);

        if (bytesRead <= 0)
        {
            Serial.println("JPEG read failed");
            return false;
        }

        if (!handler(buffer, bytesRead))
        {
            return false;
        }

        remaining -= bytesRead;
    }

    return true;
}
