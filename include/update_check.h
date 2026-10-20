#pragma once
#include <Arduino.h>

// GitHub release updates: the device checks the public releases API, compares
// the tag with its own version and can download the matching OTA asset.
enum class UpdateStage : uint8_t { Checking, Downloading };
typedef void (*UpdateProgressCallback)(UpdateStage stage, uint8_t percent);

void updateBegin();
void updateLoop();          // auto-check (if enabled) after Wi-Fi is up

bool updateCheckNow();      // blocking HTTPS request, a few seconds
bool updateInstallNow();    // blocking download + flash, restarts on success

bool updateAvailable();
const String& updateLatestVersion();
const String& updateLastError();
const char* updateStateName();

// Optional UI hook, used by the device menu to draw download progress.
void updateSetProgressCallback(UpdateProgressCallback callback);
