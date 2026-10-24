#pragma once
#include <Arduino.h>

// Do not let HTTP receive Content-Length bytes until FTP completion is verified.
class DeferredTailStream : public Stream
{
public:
    explicit DeferredTailStream(Stream &output) : output_(output) {}
    size_t write(uint8_t value) override { return write(&value, 1); }
    size_t write(const uint8_t *data, size_t size) override
    {
        if (!size) return 0;
        if (held_ && output_.write(tail_) != 1) return 0;
        held_ = false;
        if (size > 1 && output_.write(data, size - 1) != size - 1) return 0;
        tail_ = data[size - 1];
        held_ = true;
        return size;
    }
    bool commit() { if (!held_) return false; held_ = false; return output_.write(tail_) == 1; }
    int available() override { return 0; }
    int read() override { return -1; }
    int peek() override { return -1; }
    void flush() override {}
private:
    Stream &output_;
    uint8_t tail_ = 0;
    bool held_ = false;
};
