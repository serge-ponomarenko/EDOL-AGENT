#pragma once
#include <cstddef>
#include <Arduino.h>
#if defined(ARDUINO_ARCH_ESP32)
#include <mbedtls/ssl_internal.h>
#endif

// A necessary record-buffer floor, not a claim about signature/handshake peaks.
// The audited SDK has 16 KiB records in each direction and no dynamic buffers.
inline bool bulkRecordBudget(size_t freeHeap, size_t largestBlock, size_t recordSize)
{
    return largestBlock >= recordSize && freeHeap >= 2 * recordSize;
}

inline bool bulkAdmission(const char *stage)
{
#if defined(ARDUINO_ARCH_ESP32)
    const size_t recordSize = std::max(static_cast<size_t>(MBEDTLS_SSL_IN_BUFFER_LEN),
                                     static_cast<size_t>(MBEDTLS_SSL_OUT_BUFFER_LEN));
#else
    const size_t recordSize = 16384;
#endif
    const size_t freeHeap = ESP.getFreeHeap(), largest = ESP.getMaxAllocHeap();
    const bool accepted = bulkRecordBudget(freeHeap, largest, recordSize);
#if EDOL_AGENT_SMOKE_BUILD
    Serial.printf("Bulk admission: stage=%s free_heap=%u largest_block=%u record_floor=%u accepted=%u\n",
                  stage,
                  static_cast<unsigned>(freeHeap), static_cast<unsigned>(largest),
                  static_cast<unsigned>(recordSize), accepted);
#else
    (void)stage;
    if (!accepted) Serial.println("Bulk transfer rejected: TLS record memory unavailable");
#endif
    return accepted;
}
