#pragma once

#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>

using byte = uint8_t;

class String
{
public:
    String(const char *value = "") : value_(value) {}
    String(unsigned value) : value_(std::to_string(value)) {}
    const char *c_str() const { return value_.c_str(); }
    size_t length() const { return value_.size(); }
    void trim()
    {
        auto end = value_.find_last_not_of(" \r\n\t");
        value_ = end == std::string::npos ? "" : value_.substr(0, end + 1);
    }
    int indexOf(char c, int start = 0) const
    {
        auto pos = value_.find(c, start);
        return pos == std::string::npos ? -1 : static_cast<int>(pos);
    }
    String substring(int start) const { return value_.substr(start).c_str(); }
    String substring(int start, int end) const { return value_.substr(start, end - start).c_str(); }
    long toInt() const { return std::strtol(c_str(), nullptr, 10); }
    String &operator+=(char c) { value_ += c; return *this; }
    String &operator+=(const char *s) { value_ += s; return *this; }
    friend String operator+(const String &left, const String &right)
    {
        return String((left.value_ + right.value_).c_str());
    }

private:
    std::string value_;
};

class Stream
{
public:
    virtual ~Stream() = default;
    virtual size_t write(uint8_t) = 0;
    virtual size_t write(const uint8_t *, size_t) = 0;
    virtual int available() = 0;
    virtual int read() = 0;
    virtual int peek() = 0;
    virtual void flush() = 0;
};

struct FakeSerial
{
    std::string output;
    template <typename... Args> void printf(const char *format, Args... args)
    {
        char buffer[1024];
        int size = std::snprintf(buffer, sizeof(buffer), format, args...);
        if (size > 0) output.append(buffer, std::min(static_cast<size_t>(size), sizeof(buffer) - 1));
    }
    void println(const char *message) { output += message; output += '\n'; }
    size_t write(uint8_t value) { output += static_cast<char>(value); return 1; }
};

extern FakeSerial Serial;
struct FakeEsp
{
    uint32_t getFreeHeap() const { return 200000; }
    uint32_t getMaxAllocHeap() const { return 100000; }
};
#define ESP FakeEsp()
extern uint32_t fakeMillis;
inline uint32_t millis() { return fakeMillis; }
inline void delay(uint32_t ms) { fakeMillis += ms; }
