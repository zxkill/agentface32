#pragma once
#include <Arduino.h>

// On-device settings menu and full-screen status panels (Wi-Fi setup, OTA).
// Hold A to open the menu. Inside: A = next item, B/C or hold A = select.
void menuBegin();
// Returns true when the menu consumed this frame's button events.
bool menuHandleButtons();
// Chooses which panel (if any) replaces the face.
void menuLoop();
bool menuIsOpen();
// Applies brightness/sound/language changes to the running device.
void menuApplySettings();
