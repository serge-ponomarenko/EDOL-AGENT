#include "HttpUploadClient.h"

#include <Arduino.h>
#include <HTTPClient.h>

#include "../config/ConfigManager.h"

static String urlEncode(const char *value)
{
    String encoded;

    while (*value)
    {
        uint8_t c = static_cast<uint8_t>(*value);

        if (
            (c >= 'a' && c <= 'z') ||
            (c >= 'A' && c <= 'Z') ||
            (c >= '0' && c <= '9') ||
            c == '-' ||
            c == '_' ||
            c == '.' ||
            c == '~')
        {
            encoded += static_cast<char>(c);
        }
        else
        {
            char buffer[4];

            snprintf(
                buffer,
                sizeof(buffer),
                "%%%02X",
                c);

            encoded += buffer;
        }

        value++;
    }

    return encoded;
}

bool HttpUploadClient::begin(
    const char *fileName,
    size_t contentLength)
{
    if (!connect(contentLength))
    {
        return false;
    }

    String encodedFileName = urlEncode(fileName);

    client_.printf(
        "POST %s?fileName=%s HTTP/1.1\r\n"
        "Host: %s:%u\r\n"
        "X-Agent-Id: %s\r\n"
        "Content-Type: application/octet-stream\r\n"
        "Content-Length: %u\r\n"
        "Connection: close\r\n"
        "\r\n",
        ConfigManager::getCoreUploadPath().c_str(),
        encodedFileName.c_str(),
        ConfigManager::getCoreHost().c_str(),
        ConfigManager::getCorePort(),
        ConfigManager::getEdolAgentId().c_str(),
        static_cast<unsigned>(contentLength));

    return true;
}

bool HttpUploadClient::beginSnapshotUpload(
    const char *agentId,
    size_t contentLength)
{
    if (!connect(contentLength))
    {
        return false;
    }

    client_.printf(
        "POST /api/agent/camera/snapshot HTTP/1.1\r\n"
        "Host: %s:%u\r\n"
        "X-Agent-Id: %s\r\n"
        "Content-Type: image/jpeg\r\n"
        "Content-Length: %u\r\n"
        "Connection: close\r\n"
        "\r\n",
        ConfigManager::getCoreHost().c_str(),
        ConfigManager::getCorePort(),
        agentId,
        static_cast<unsigned>(contentLength));

    return true;
}

bool HttpUploadClient::connect(size_t contentLength)
{
    abort();
    uploadedBytes_ = 0;
    contentLength_ = contentLength;
    writeFailed_ = false;

    if (!client_.connect(
            ConfigManager::getCoreHost().c_str(),
            ConfigManager::getCorePort()))
    {
        Serial.println("HTTP connect failed");
        abort();
        return false;
    }

    return true;
}

void HttpUploadClient::abort()
{
    client_.stop();
}

size_t HttpUploadClient::write(
    const uint8_t *buffer,
    size_t size)
{
    size_t written =
        client_.write(
            buffer,
            size);

    if (written != size)
    {
        writeFailed_ = true;

        Serial.printf(
            "HTTP write failed: requested=%u written=%u\n",
            static_cast<unsigned>(size),
            static_cast<unsigned>(written));
    }

    uploadedBytes_ += written;

    return written;
}

size_t HttpUploadClient::write(uint8_t byte)
{
    return write(&byte, 1);
}

bool HttpUploadClient::finish(
    uint16_t expectedStatusCode)
{
    Serial.printf(
        "Uploaded %u bytes\n",
        uploadedBytes_);

    if (writeFailed_ || uploadedBytes_ != contentLength_)
    {
        client_.stop();
        return false;
    }

    uint32_t startTime = millis();

    while (!client_.available())
    {
        if (!client_.connected())
        {
            Serial.println(
                "HTTP response connection closed");

            client_.stop();

            return false;
        }

        if (millis() - startTime > 10000)
        {
            Serial.println(
                "HTTP response timeout");

            client_.stop();

            return false;
        }

        delay(10);
    }

    String statusLine =
        client_.readStringUntil('\n');

    statusLine.trim();

    int firstSpace =
        statusLine.indexOf(' ');

    if (firstSpace < 0)
    {
        client_.stop();

        return false;
    }

    int secondSpace =
        statusLine.indexOf(
            ' ',
            firstSpace + 1);

    String statusCodeString;

    if (secondSpace < 0)
    {
        statusCodeString =
            statusLine.substring(
                firstSpace + 1);
    }
    else
    {
        statusCodeString =
            statusLine.substring(
                firstSpace + 1,
                secondSpace);
    }

    uint16_t statusCode =
        static_cast<uint16_t>(statusCodeString.toInt());

    while (client_.available())
    {
        client_.read();
    }

    client_.stop();

    Serial.printf(
        "HTTP parsed status=%u expected=%u\n",
        statusCode,
        expectedStatusCode);

    if (statusCode != expectedStatusCode)
    {
        Serial.printf(
            "Unexpected HTTP status: %u\n",
            statusCode);

        return false;
    }

    Serial.println(
        "HTTP finish SUCCESS");

    return true;
}

bool HttpUploadClient::hasError() const
{
    return writeFailed_;
}
