#include <esp_attr.h>
#include <esp_log.h>
#include <freertos/FreeRTOS.h>
#include <stdarg.h>
#include "logger.h"
#include "version.h"

#ifndef LOG_BUFFER_SIZE
  #define LOG_BUFFER_SIZE 16384
#endif

static constexpr uint32_t kMagicHead = 0x4146334CUL;  // "AF3L"
static constexpr uint32_t kMagicTail = 0x4C33464AUL;

struct LogStore {
  uint32_t head;
  uint32_t end;
  char data[LOG_BUFFER_SIZE];
  uint32_t tail;
};

// Not zeroed at startup: survives panic, watchdog and esp_restart().
static __NOINIT_ATTR LogStore g_store;
static portMUX_TYPE g_mux = portMUX_INITIALIZER_UNLOCKED;
static vprintf_like_t g_prevVprintf = nullptr;
static bool g_ready = false;

static void appendRaw(const char* s, size_t n) {
  if (!g_ready || !n) return;
  portENTER_CRITICAL(&g_mux);
  for (size_t i = 0; i < n; ++i) {
    g_store.data[g_store.end % LOG_BUFFER_SIZE] = s[i];
    ++g_store.end;
  }
  portEXIT_CRITICAL(&g_mux);
}

// Mirrors ESP-IDF component logs (Wi-Fi driver, core dump checks, ...) into
// the ring buffer while keeping their normal UART output.
static int idfVprintf(const char* fmt, va_list ap) {
  char line[192];
  va_list copy;
  va_copy(copy, ap);
  int n = vsnprintf(line, sizeof(line), fmt, copy);
  va_end(copy);
  if (n > 0) appendRaw(line, min((size_t)n, sizeof(line) - 1));
  return g_prevVprintf ? g_prevVprintf(fmt, ap) : vprintf(fmt, ap);
}

static void logV(char level, const char* tag, const char* fmt, va_list ap) {
  char line[256];
  const uint32_t ms = millis();
  int n = snprintf(line, sizeof(line), "[%6lu.%03lu] %c %s: ",
                   (unsigned long)(ms / 1000), (unsigned long)(ms % 1000), level, tag);
  if (n < 0) return;
  n = min(n, (int)sizeof(line) - 2);
  int m = vsnprintf(line + n, sizeof(line) - n - 1, fmt, ap);
  if (m > 0) n = min(n + m, (int)sizeof(line) - 2);
  line[n++] = '\n';
  Serial.write((const uint8_t*)line, n);
  appendRaw(line, n);
}

void logInfo(const char* tag, const char* fmt, ...) {
  va_list ap; va_start(ap, fmt); logV('I', tag, fmt, ap); va_end(ap);
}

void logWarn(const char* tag, const char* fmt, ...) {
  va_list ap; va_start(ap, fmt); logV('W', tag, fmt, ap); va_end(ap);
}

void logError(const char* tag, const char* fmt, ...) {
  va_list ap; va_start(ap, fmt); logV('E', tag, fmt, ap); va_end(ap);
}

void loggerBegin(const char* resetReason, uint32_t bootCount, bool coldBoot) {
  const bool valid = !coldBoot && g_store.head == kMagicHead && g_store.tail == kMagicTail;
  if (!valid) {
    g_store.head = kMagicHead;
    g_store.tail = kMagicTail;
    g_store.end = 0;
  }
  g_ready = true;

  char banner[128];
  int n = snprintf(banner, sizeof(banner), "\n===== boot #%lu reset=%s fw=%s %s =====\n",
                   (unsigned long)bootCount, resetReason, AGENT_COMPANION_VERSION,
                   valid ? "(log kept)" : "(log cleared)");
  if (n > 0) {
    Serial.write((const uint8_t*)banner, n);
    appendRaw(banner, n);
  }
  g_prevVprintf = esp_log_set_vprintf(idfVprintf);
}

uint32_t loggerEnd() {
  portENTER_CRITICAL(&g_mux);
  const uint32_t end = g_store.end;
  portEXIT_CRITICAL(&g_mux);
  return end;
}

uint32_t loggerOldest() {
  const uint32_t end = loggerEnd();
  return end > LOG_BUFFER_SIZE ? end - LOG_BUFFER_SIZE : 0;
}

size_t loggerCapacity() { return LOG_BUFFER_SIZE; }

size_t loggerRead(uint32_t* from, char* out, size_t maxLen) {
  portENTER_CRITICAL(&g_mux);
  const uint32_t end = g_store.end;
  const uint32_t oldest = end > LOG_BUFFER_SIZE ? end - LOG_BUFFER_SIZE : 0;
  uint32_t pos = *from;
  if (pos < oldest || pos > end) pos = oldest;
  size_t n = 0;
  while (pos < end && n < maxLen) {
    out[n++] = g_store.data[pos % LOG_BUFFER_SIZE];
    ++pos;
  }
  portEXIT_CRITICAL(&g_mux);
  *from = pos;
  return n;
}
