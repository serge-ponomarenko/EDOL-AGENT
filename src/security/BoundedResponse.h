#pragma once

#include <Arduino.h>
#include <cstring>

class BoundedResponse : public Stream
{
public:
    static constexpr size_t LIMIT = 1024;
    size_t write(const uint8_t *bytes, size_t size) override
    {
        if (size > LIMIT - size_)
        {
            failed_ = true;
            return 0;
        }
        std::memcpy(data_ + size_, bytes, size);
        size_ += size;
        data_[size_] = '\0';
        return size;
    }
    size_t write(uint8_t byte) override { return write(&byte, 1); }
    int available() override { return 0; }
    int read() override { return -1; }
    int peek() override { return -1; }
    void flush() override { }
    const char *data() const { return data_; }
    size_t size() const { return size_; }
    bool failed() const { return failed_; }
private:
    char data_[LIMIT + 1] = {};
    size_t size_ = 0;
    bool failed_ = false;
};
