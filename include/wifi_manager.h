#pragma once
#include <Arduino.h>

// Station connection from saved settings plus a setup access point with a
// captive portal. The AP starts on first boot (no saved network), on request
// (device menu / web UI) or when the saved network stays unreachable.
void wifiManagerBegin();
void wifiManagerLoop();

bool wifiConnected();
String wifiIp();                 // station IP, empty when not connected
bool wifiSetupActive();
void wifiStartSetup();           // keeps the station connection if there is one
void wifiStopSetup();            // ignored when no network is saved yet
String wifiSetupSsid();
const char* wifiSetupPassword();
String wifiSetupIp();
uint8_t wifiSetupClients();
