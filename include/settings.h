#pragma once
#include <Arduino.h>
#include "agent.h"

// Runtime settings persisted in NVS. Compile-time values from config.h are
// only defaults for a device that has never been configured.
struct DeviceSettings {
  String wifiSsid;
  String wifiPassword;
  String hostname;
  String adminPassword;   // protects settings/OTA/logs when non-empty
  bool russian = false;
  bool sound = false;
  bool autoUpdate = true;
  uint8_t brightness = 70; // percent, 10..100
  uint32_t providerMask = 0;
};

void settingsBegin();
const DeviceSettings& settings();

bool settingsHasWifi();
bool settingsProviderEnabled(AgentId agent);
uint8_t settingsEnabledProviderCount();

// Setters validate, persist immediately and return false on invalid input.
bool settingsSetWifi(const String& ssid, const String& password);
bool settingsSetHostname(const String& hostname);
void settingsSetAdminPassword(const String& password);
void settingsSetRussian(bool russian);
void settingsSetSound(bool enabled);
void settingsSetAutoUpdate(bool enabled);
void settingsSetBrightness(uint8_t percent);
// Refuses to disable the last enabled provider.
bool settingsSetProviderEnabled(AgentId agent, bool enabled);
