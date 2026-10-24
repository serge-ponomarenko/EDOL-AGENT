#pragma once
#include <Arduino.h>
#include <vector>

struct FtpSocketFixture
{
    bool connectOk = true, connected = false, closeWithBufferedData = false;
    bool negativeRead = false, negativeEof = false;
    std::string input, commands;
    size_t position = 0;
};
extern FtpSocketFixture ftpControl, ftpData;
extern unsigned ftpSocketCount;
struct sslclient_context { int socket = -1; };
class IPAddress
{
public:
    IPAddress() = default;
    IPAddress(int, int, int, int) {}
};
class WiFiClientSecure
{
    FtpSocketFixture &fixture_;
protected:
    sslclient_context context_;
    sslclient_context *sslclient = &context_;
public:
    WiFiClientSecure() : fixture_(ftpSocketCount++ % 2 == 0 ? ftpControl : ftpData) {}
    void setInsecure() {}
    void setTimeout(unsigned long) {}
    void setHandshakeTimeout(unsigned long) {}
    bool connect(const char *, uint16_t) { fixture_.connected = fixture_.connectOk; return fixture_.connectOk; }
    bool connect(IPAddress, uint16_t)
    {
        fixture_.connected = fixture_.connectOk && !fixture_.closeWithBufferedData;
        return fixture_.connectOk;
    }
    bool connected() const { return fixture_.connected; }
    int available() const { if (fixture_.negativeEof && fixture_.position == fixture_.input.size()) return -1; return static_cast<int>(fixture_.input.size() - fixture_.position); }
    int read()
    {
        if (fixture_.negativeRead) return -1;
        return available() ? static_cast<unsigned char>(fixture_.input[fixture_.position++]) : -1;
    }
    int read(uint8_t *buffer, size_t size)
    {
        if (fixture_.negativeRead) return -1;
        const size_t count = std::min(size, fixture_.input.size() - fixture_.position);
        std::memcpy(buffer, fixture_.input.data() + fixture_.position, count);
        fixture_.position += count;
        if (fixture_.position == fixture_.input.size()) fixture_.connected = false;
        return static_cast<int>(count);
    }
    size_t write(const uint8_t *buffer, size_t size)
    {
        fixture_.commands.append(reinterpret_cast<const char *>(buffer), size);
        return size;
    }
    void flush() {}
    virtual void stop() { fixture_.connected = false; sslclient->socket = 0; }
};
