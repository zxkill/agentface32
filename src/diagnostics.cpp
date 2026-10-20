#include <Arduino.h>
#include <esp_system.h>
#include <esp_core_dump.h>
#include "diagnostics.h"
#include "logger.h"

// RTC_DATA_ATTR is re-initialized on every boot except deep-sleep wakeups, so
// keep the counter in no-init RTC memory guarded by a magic value instead.
static constexpr uint32_t kBootMagic = 0xB007C0DEUL;
RTC_NOINIT_ATTR static uint32_t rtcBootMagic;
RTC_NOINIT_ATTR static uint32_t rtcBootCount;
static const char* g_resetReason = "unknown";
static bool g_coldBoot = true;
static uint32_t g_restartAt = 0;
static bool g_restartPending = false;

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
  const esp_reset_reason_t reason = esp_reset_reason();
  g_resetReason = resetReasonName(reason);
  g_coldBoot = reason == ESP_RST_POWERON || reason == ESP_RST_UNKNOWN;
  if (g_coldBoot || rtcBootMagic != kBootMagic) {
    rtcBootMagic = kBootMagic;
    rtcBootCount = 0;
  }
  ++rtcBootCount;
}

const char* diagnosticsResetReason() { return g_resetReason; }
uint32_t diagnosticsBootCount() { return rtcBootCount; }
bool diagnosticsColdBoot() { return g_coldBoot; }

void diagnosticsScheduleRestart(uint32_t delayMs, const char* reason) {
  logInfo("system", "restart in %lu ms: %s", (unsigned long)delayMs, reason);
  g_restartAt = millis() + delayMs;
  g_restartPending = true;
}

bool diagnosticsRestartPending() { return g_restartPending; }

void diagnosticsLoop() {
  if (g_restartPending && (int32_t)(millis() - g_restartAt) >= 0) {
    delay(50);
    ESP.restart();
  }
}

#if CONFIG_ESP_COREDUMP_ENABLE_TO_FLASH && CONFIG_ESP_COREDUMP_DATA_FORMAT_ELF

static const char* exceptionCauseName(uint32_t cause) {
  switch (cause) {
    case 0:  return "IllegalInstruction";
    case 2:  return "InstructionFetchError";
    case 3:  return "LoadStoreError";
    case 6:  return "IntegerDivideByZero";
    case 9:  return "LoadStoreAlignment";
    case 20: return "InstFetchProhibited";
    case 28: return "LoadProhibited";
    case 29: return "StoreProhibited";
    default: return "other";
  }
}

bool diagnosticsCoreDumpInfo(size_t* address, size_t* size) {
  return esp_core_dump_image_get(address, size) == ESP_OK;
}

void diagnosticsCrashJson(JsonDocument& doc) {
  doc["last_reset"] = g_resetReason;
  doc["boot_count"] = rtcBootCount;
  size_t addr = 0, size = 0;
  if (!diagnosticsCoreDumpInfo(&addr, &size)) {
    doc["coredump"] = false;
    return;
  }
  doc["coredump"] = true;
  doc["coredump_size"] = size;

  esp_core_dump_summary_t* summary = new esp_core_dump_summary_t();
  if (esp_core_dump_get_summary(summary) == ESP_OK) {
    char hex[12];
    doc["task"] = summary->exc_task;
    snprintf(hex, sizeof(hex), "0x%08lx", (unsigned long)summary->exc_pc);
    doc["pc"] = hex;
#if defined(__XTENSA__)
    doc["exc_cause"] = summary->ex_info.exc_cause;
    doc["exc_cause_name"] = exceptionCauseName(summary->ex_info.exc_cause);
    snprintf(hex, sizeof(hex), "0x%08lx", (unsigned long)summary->ex_info.exc_vaddr);
    doc["exc_vaddr"] = hex;
#endif
    JsonArray bt = doc["backtrace"].to<JsonArray>();
    for (uint32_t i = 0; i < summary->exc_bt_info.depth && i < 16; ++i) {
      snprintf(hex, sizeof(hex), "0x%08lx", (unsigned long)summary->exc_bt_info.bt[i]);
      bt.add(hex);
    }
    doc["backtrace_corrupted"] = summary->exc_bt_info.corrupted;
    // Prefix of the crashing build's ELF SHA-256 (compare with firmware.elf).
    char sha[sizeof(summary->app_elf_sha256) + 1];
    memcpy(sha, summary->app_elf_sha256, sizeof(summary->app_elf_sha256));
    sha[sizeof(summary->app_elf_sha256)] = 0;
    doc["app_elf_sha256"] = sha;
  } else {
    doc["summary_error"] = "core dump present but unreadable";
  }
  delete summary;
}

bool diagnosticsCoreDumpErase() { return esp_core_dump_image_erase() == ESP_OK; }

void diagnosticsReportCrash() {
  size_t addr = 0, size = 0;
  if (!diagnosticsCoreDumpInfo(&addr, &size)) return;
  JsonDocument doc;
  diagnosticsCrashJson(doc);
  String bt;
  for (JsonVariant v : doc["backtrace"].as<JsonArray>()) {
    if (bt.length()) bt += ' ';
    bt += v.as<const char*>();
  }
  logWarn("crash", "stored core dump: task=%s pc=%s cause=%s backtrace=%s",
          doc["task"] | "?", doc["pc"] | "?", doc["exc_cause_name"] | "?", bt.c_str());
  logWarn("crash", "details: GET /api/crash, raw dump: GET /coredump");
}

#else

bool diagnosticsCoreDumpInfo(size_t*, size_t*) { return false; }
void diagnosticsCrashJson(JsonDocument& doc) {
  doc["last_reset"] = g_resetReason;
  doc["boot_count"] = rtcBootCount;
  doc["coredump"] = false;
}
bool diagnosticsCoreDumpErase() { return false; }
void diagnosticsReportCrash() {}

#endif
