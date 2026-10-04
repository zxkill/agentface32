#pragma once
#include <Arduino.h>

void diagnosticsBegin();
const char* diagnosticsResetReason();
uint32_t diagnosticsBootCount();
