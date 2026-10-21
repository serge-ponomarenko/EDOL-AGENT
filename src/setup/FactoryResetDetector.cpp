#include "FactoryResetDetector.h"

#include "../config/ConfigManager.h"

bool FactoryResetDetector::shouldFactoryReset()
{
    ConfigManager::begin();

    uint32_t bootCount =
        ConfigManager::getBootCount();

    bootCount++;

    ConfigManager::setBootCount(
        bootCount);

    return bootCount >= RESET_THRESHOLD;
}

void FactoryResetDetector::markBootSuccessful()
{
    ConfigManager::setBootCount(0);
}