#pragma once

#include <Arduino.h>

class FactoryResetDetector
{
public:
    bool shouldFactoryReset();
    void markBootSuccessful();

private:
    static constexpr uint32_t RESET_THRESHOLD = 5;
};