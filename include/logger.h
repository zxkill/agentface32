#pragma once
#include <Arduino.h>

// Log ring buffer kept in RAM that is not cleared on software resets, so the
// lines leading up to a panic, watchdog reset or OTA reboot can be read over
// Wi-Fi after the device comes back. A power cycle clears it.
//
// Every byte has an absolute offset; readers pass the offset they already have
// to fetch only new data (GET /logs?since=N).

void loggerBegin(const char* resetReason, uint32_t bootCount, bool coldBoot);

void logInfo(const char* tag, const char* fmt, ...) __attribute__((format(printf, 2, 3)));
void logWarn(const char* tag, const char* fmt, ...) __attribute__((format(printf, 2, 3)));
void logError(const char* tag, const char* fmt, ...) __attribute__((format(printf, 2, 3)));

uint32_t loggerEnd();     // absolute offset after the newest byte
uint32_t loggerOldest();  // absolute offset of the oldest retained byte
size_t loggerCapacity();

// Copies bytes starting at *from (clamped to the oldest retained byte) and
// advances *from. Returns the number of bytes copied.
size_t loggerRead(uint32_t* from, char* out, size_t maxLen);
