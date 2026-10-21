#pragma once

#include <HTTPClient.h>

class HttpUploadClient : public Stream
{
public:
    bool begin(
        const char *fileName,
        size_t contentLength);

    bool beginSnapshotUpload(
        const char *agentId,
        size_t contentLength);

    size_t write(
        const uint8_t *buffer,
        size_t size);

    bool finish(
        uint16_t expectedStatusCode = 200);

    void abort();

    bool hasError() const;

    size_t write(uint8_t byte) override;

private:
    WiFiClient client_;

    size_t uploadedBytes_ = 0;

    size_t contentLength_ = 0;

    bool writeFailed_ = false;

    bool connect(size_t contentLength);

    int available() override
    {
        return 0;
    }

    int read() override
    {
        return -1;
    }

    int peek() override
    {
        return -1;
    }

    void flush() override
    {
    }
};
