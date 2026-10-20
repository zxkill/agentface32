#include <Update.h>
#include <esp_ota_ops.h>
#include "ota.h"
#include "logger.h"
#include "wifi_manager.h"

// Healthy-run time before a new image is confirmed.
static constexpr uint32_t kVerifyAfterMs = 20000;

static bool g_inProgress = false;
static size_t g_expected = 0;
static size_t g_written = 0;
static String g_error;
static bool g_pendingVerify = false;

// Tell the Arduino core not to confirm the image at startup; otaLoop() does it
// once the firmware has proven itself.
extern "C" bool verifyRollbackLater() { return true; }

void otaBegin() {
  const esp_partition_t* running = esp_ota_get_running_partition();
  esp_ota_img_states_t state;
  g_pendingVerify = running && esp_ota_get_state_partition(running, &state) == ESP_OK &&
                    state == ESP_OTA_IMG_PENDING_VERIFY;
  logInfo("ota", "running from %s%s", running ? running->label : "?",
          g_pendingVerify ? " (new image, pending verification)" : "");
  if (otaRolledBack()) {
    logWarn("ota", "a previous update failed verification and was rolled back");
  }
}

void otaLoop() {
  if (!g_pendingVerify) return;
  if (millis() < kVerifyAfterMs) return;
  if (!wifiConnected() && !wifiSetupActive()) return;
  if (esp_ota_mark_app_valid_cancel_rollback() == ESP_OK) {
    g_pendingVerify = false;
    logInfo("ota", "new firmware confirmed");
  }
}

bool otaStart(size_t expectedSize, const String& md5) {
  if (g_inProgress) Update.abort();
  g_error = "";
  g_expected = expectedSize;
  g_written = 0;
  if (!Update.begin(UPDATE_SIZE_UNKNOWN, U_FLASH)) {
    g_error = Update.errorString();
    logError("ota", "begin failed: %s", g_error.c_str());
    return false;
  }
  if (md5.length() == 32 && !Update.setMD5(md5.c_str())) {
    g_error = "bad md5 argument";
    Update.abort();
    return false;
  }
  g_inProgress = true;
  logInfo("ota", "update started, ~%u bytes%s", (unsigned)expectedSize,
          md5.length() == 32 ? ", md5 check on" : "");
  return true;
}

bool otaWrite(const uint8_t* data, size_t len) {
  if (!g_inProgress) return false;
  if (Update.write(const_cast<uint8_t*>(data), len) != len) {
    g_error = Update.errorString();
    logError("ota", "write failed at %u: %s", (unsigned)g_written, g_error.c_str());
    otaAbort();
    return false;
  }
  g_written += len;
  return true;
}

bool otaFinish() {
  if (!g_inProgress) return false;
  g_inProgress = false;
  if (!Update.end(true)) {
    g_error = Update.errorString();
    logError("ota", "finish failed: %s", g_error.c_str());
    return false;
  }
  logInfo("ota", "update written (%u bytes), reboot required", (unsigned)g_written);
  return true;
}

void otaAbort() {
  if (g_inProgress) Update.abort();
  g_inProgress = false;
}

bool otaInProgress() { return g_inProgress; }

uint8_t otaProgress() {
  if (!g_expected) return 0;
  return (uint8_t)min((size_t)100, (g_written * 100) / g_expected);
}

const String& otaLastError() { return g_error; }
bool otaPendingVerify() { return g_pendingVerify; }

const char* otaRunningPartition() {
  const esp_partition_t* running = esp_ota_get_running_partition();
  return running ? running->label : "?";
}

bool otaRolledBack() { return esp_ota_get_last_invalid_partition() != nullptr; }
