#pragma once
#include <Arduino.h>
#include <M5GFX.h>
#include "ui_font_data.h"

int uiTextWidth(const UiFont& font, const String& text);
void uiDrawText(M5Canvas& canvas, const UiFont& font, const String& text,
                int x, int y, uint16_t color, uint16_t bgColor,
                int maxWidth = -1);
void uiDrawTextCentered(M5Canvas& canvas, const UiFont& font, const String& text,
                        int centerX, int y, uint16_t color, uint16_t bgColor,
                        int maxWidth = -1);
