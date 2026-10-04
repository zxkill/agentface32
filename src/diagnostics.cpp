#include <Arduino.h>
#include <esp_system.h>
#include "diagnostics.h"

RTC_DATA_ATTR static uint32_t rtcBootCount = 0;
static const char* g_resetReason = "unknown";

static const char* resetReasonName(esp_reset_reason_t reason) {
  switch (reason) {
    case ESP_RST_POWERON:   return "power_on";
    case ESP_RST_EXT:       return "external";
    case ESP_RST_SW:        return "software";
    case ESP_RST_PANIC:     return "panic";
    case ESP_RST_INT_WDT:   return "interrupt_watchdog";
    case ESP_RST_TASK_WDT:  return "task_watchdog";
    case ESP_RST_WDT:       return "watchdog";
    case ESP_RST_DEEPSLEEP: return "deep_sleep";
    case ESP_RST_BROWNOUT:  return "brownout";
    case ESP_RST_SDIO:      return "sdio";
    case ESP_RST_UNKNOWN:
    default:                return "unknown";
  }
}

void diagnosticsBegin() {
  ++rtcBootCount;
  g_resetReason = resetReasonName(esp_reset_reason());
  Serial.printf("[diag] boot=%lu last_reset=%s free_heap=%u\n",
                (unsigned long)rtcBootCount,
                g_resetReason,
                (unsigned)ESP.getFreeHeap());
}

const char* diagnosticsResetReason() { return g_resetReason; }
uint32_t diagnosticsBootCount() { return rtcBootCount; }
