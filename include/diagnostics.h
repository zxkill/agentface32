#pragma once
#include <Arduino.h>
#include <ArduinoJson.h>

void diagnosticsBegin();
// Call once logging is up: reports a stored crash dump from a previous boot.
void diagnosticsReportCrash();
void diagnosticsLoop();

const char* diagnosticsResetReason();
uint32_t diagnosticsBootCount();
bool diagnosticsColdBoot();

// Restart after `delayMs`, letting the HTTP response go out first.
void diagnosticsScheduleRestart(uint32_t delayMs, const char* reason);
bool diagnosticsRestartPending();

// Core dump stored in flash by the panic handler (ELF format).
bool diagnosticsCoreDumpInfo(size_t* address, size_t* size);
void diagnosticsCrashJson(JsonDocument& doc);
bool diagnosticsCoreDumpErase();
