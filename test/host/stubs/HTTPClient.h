#pragma once
#include "Arduino.h"
#include <limits>
#include <vector>

struct FakeSocket
{
    bool connectOk = true;
    bool connected = false;
    bool hasResponse = true;
    size_t writeLimit = std::numeric_limits<size_t>::max();
    unsigned stops = 0;
    unsigned drains = 0;
    String status = "HTTP/1.1 200 OK\r";
    std::string headers;
    std::vector<uint8_t> body;
};

extern FakeSocket socketState;

class WiFiClient
{
public:
    bool connect(const char *, uint16_t)
    {
        socketState.connected = socketState.connectOk;
        return socketState.connected;
    }
    template <typename... Args> size_t printf(const char *format, Args... args)
    {
        char buffer[2048];
        int size = std::snprintf(buffer, sizeof(buffer), format, args...);
        socketState.headers = buffer;
        return size;
    }
    size_t write(const uint8_t *data, size_t size)
    {
        auto count = std::min(size, socketState.writeLimit);
        socketState.body.insert(socketState.body.end(), data, data + count);
        return count;
    }
    void stop() { socketState.connected = false; ++socketState.stops; }
    bool connected() const { return socketState.connected; }
    int available() const { return socketState.hasResponse ? 1 : 0; }
    String readStringUntil(char)
    {
        socketState.hasResponse = false;
        return socketState.status;
    }
    int read() { ++socketState.drains; socketState.hasResponse = false; return -1; }
};

enum followRedirects_t { HTTPC_DISABLE_FOLLOW_REDIRECTS };
struct FakeHttpState
{
    bool beginOk = true;
    int status = 200;
    std::string response;
    std::string url;
    std::string request;
    int posts = 0;
    int ends = 0;
    int redirects = -1;
};
extern FakeHttpState httpState;

class HTTPClient
{
public:
    bool begin(WiFiClient &, const String &url) { httpState.url = url.c_str(); return httpState.beginOk; }
    void setReuse(bool) { }
    void setConnectTimeout(int) { }
    void setTimeout(int) { }
    void setFollowRedirects(followRedirects_t value) { httpState.redirects = value; }
    void addHeader(const char *, const char *) { }
    int POST(const String &request) { ++httpState.posts; httpState.request = request.c_str(); return httpState.status; }
    int writeToStream(Stream *stream)
    {
        return static_cast<int>(stream->write(reinterpret_cast<const uint8_t *>(httpState.response.data()), httpState.response.size()));
    }
    void end() { ++httpState.ends; }
};
