#pragma once
#include <Arduino.h>

struct UiGlyph {
  uint16_t codepoint;
  uint32_t offset;
  uint8_t width;
  uint8_t height;
  uint8_t advance;
};

struct UiFont {
  const uint8_t* bitmap;
  const UiGlyph* glyphs;
  uint16_t glyphCount;
  uint8_t lineHeight;
};

extern const UiFont UI_FONT_SMALL;
extern const UiFont UI_FONT_HEADER;
extern const UiFont UI_FONT_BIG;
