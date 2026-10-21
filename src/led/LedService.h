#pragma once

#include <Arduino.h>

class LedService
{
public:
    void begin();
    void process();

    void blink();

    void setOn();
    void setOff();

    void startFastBlink();
    void stopFastBlink();

private:
    static constexpr uint8_t LED_PIN = 5;
    static constexpr uint32_t BLINK_DURATION_MS = 50;

    bool blinkActive_ = false;
    uint32_t blinkEndMillis_ = 0;

    bool fastBlinkEnabled_ = false;
    bool fastBlinkState_ = false;
    uint32_t lastFastBlinkMillis_ = 0;

    static constexpr uint32_t FAST_BLINK_INTERVAL_MS = 100;
};