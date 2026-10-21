#include "LedService.h"

void LedService::begin()
{
    pinMode(LED_PIN, OUTPUT);
    digitalWrite(LED_PIN, HIGH);
}

void LedService::blink()
{
    digitalWrite(LED_PIN, LOW);

    blinkActive_ = true;
    blinkEndMillis_ = millis() + BLINK_DURATION_MS;
}

void LedService::setOn()
{
    digitalWrite(
        LED_PIN,
        LOW);
}

void LedService::setOff()
{
    digitalWrite(
        LED_PIN,
        HIGH);
}

void LedService::startFastBlink()
{
    fastBlinkEnabled_ = true;
}

void LedService::stopFastBlink()
{
    fastBlinkEnabled_ = false;

    digitalWrite(
        LED_PIN,
        HIGH);
}

void LedService::process()
{
    if (fastBlinkEnabled_)
    {
        if (millis() - lastFastBlinkMillis_ >= FAST_BLINK_INTERVAL_MS)
        {
            lastFastBlinkMillis_ = millis();

            fastBlinkState_ =
                !fastBlinkState_;

            digitalWrite(
                LED_PIN,
                fastBlinkState_ ? LOW : HIGH);
        }

        return;
    }

    if (!blinkActive_)
    {
        return;
    }

    if (millis() < blinkEndMillis_)
    {
        return;
    }

    digitalWrite(LED_PIN, HIGH);

    blinkActive_ = false;
}