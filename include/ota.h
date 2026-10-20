#pragma once
#include <Arduino.h>

// Firmware update over Wi-Fi (HTTP POST /update) with automatic rollback:
// a freshly flashed image stays "pending verify" until it has run for a while
// with the network up. If it crashes or is reset before that, the bootloader
// boots the previous firmware again.
void otaBegin();
void otaLoop();

bool otaStart(size_t expectedSize, const String& md5);
bool otaWrite(const uint8_t* data, size_t len);
bool otaFinish();
void otaAbort();
bool otaInProgress();
uint8_t otaProgress();          // 0..100, needs a known expected size
const String& otaLastError();

bool otaPendingVerify();        // running image not confirmed yet
const char* otaRunningPartition();
bool otaRolledBack();           // a previous update was rejected
