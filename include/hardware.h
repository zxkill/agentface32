#pragma once
#include <Arduino.h>

void hardwareBegin();
void hardwareUpdate();
void hardwareSetDisplayBrightness(uint8_t value);

bool hardwareButtonAClicked();
bool hardwareButtonBClicked();
bool hardwareButtonCClicked();
bool hardwareButtonCHeld();
