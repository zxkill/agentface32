#include "hardware_profile.h"
#if defined(PROFILE_M5STACK_BASIC)
#include <pgmspace.h>
#include "ui_text.h"

static uint32_t nextCodepoint(const String& s, size_t& i) {
  if (i >= s.length()) return 0;
  const uint8_t c = (uint8_t)s[i++];
  if (c < 0x80) return c;
  if ((c & 0xE0) == 0xC0 && i < s.length()) {
    uint32_t cp = (c & 0x1F) << 6;
    cp |= ((uint8_t)s[i++] & 0x3F);
    return cp;
  }
  if ((c & 0xF0) == 0xE0 && i + 1 < s.length()) {
    uint32_t cp = (c & 0x0F) << 12;
    cp |= (((uint8_t)s[i++] & 0x3F) << 6);
    cp |= ((uint8_t)s[i++] & 0x3F);
    return cp;
  }
  while (i < s.length() && (((uint8_t)s[i] & 0xC0) == 0x80)) ++i;
  return '?';
}

static bool findGlyph(const UiFont& font, uint32_t cp, UiGlyph& out) {
  int lo = 0;
  int hi = (int)font.glyphCount - 1;
  while (lo <= hi) {
    const int mid = (lo + hi) / 2;
    UiGlyph g;
    memcpy_P(&g, font.glyphs + mid, sizeof(g));
    if (g.codepoint == cp) { out = g; return true; }
    if (g.codepoint < cp) lo = mid + 1;
    else hi = mid - 1;
  }
  if (cp != '?') return findGlyph(font, '?', out);
  return false;
}

static uint16_t blend565(uint16_t fg, uint16_t bg, uint8_t alpha) {
  if (alpha == 0) return bg;
  if (alpha >= 15) return fg;
  const int fr = (fg >> 11) & 0x1F;
  const int fg6 = (fg >> 5) & 0x3F;
  const int fb = fg & 0x1F;
  const int br = (bg >> 11) & 0x1F;
  const int bg6 = (bg >> 5) & 0x3F;
  const int bb = bg & 0x1F;
  const int ia = 15 - alpha;
  const int r = (fr * alpha + br * ia + 7) / 15;
  const int g = (fg6 * alpha + bg6 * ia + 7) / 15;
  const int b = (fb * alpha + bb * ia + 7) / 15;
  return (uint16_t)((r << 11) | (g << 5) | b);
}

int uiTextWidth(const UiFont& font, const String& text) {
  int width = 0;
  size_t i = 0;
  while (i < text.length()) {
    UiGlyph g;
    if (findGlyph(font, nextCodepoint(text, i), g)) width += g.advance;
  }
  return width;
}

static void drawGlyph(M5Canvas& canvas, const UiFont& font, const UiGlyph& g,
                      int x, int y, uint16_t color, uint16_t bgColor) {
  uint32_t pixelIndex = 0;
  for (uint8_t row = 0; row < g.height; ++row) {
    for (uint8_t col = 0; col < g.width; ++col, ++pixelIndex) {
      const uint8_t packed = pgm_read_byte(font.bitmap + g.offset + (pixelIndex >> 1));
      const uint8_t a = (pixelIndex & 1) ? (packed & 0x0F) : (packed >> 4);
      if (!a) continue;
      canvas.drawPixel(x + col, y + row, blend565(color, bgColor, a));
    }
  }
}

void uiDrawText(M5Canvas& canvas, const UiFont& font, const String& text,
                int x, int y, uint16_t color, uint16_t bgColor, int maxWidth) {
  int cursor = x;
  size_t i = 0;
  while (i < text.length()) {
    UiGlyph g;
    const uint32_t cp = nextCodepoint(text, i);
    if (!findGlyph(font, cp, g)) continue;
    if (maxWidth >= 0 && cursor + g.advance > x + maxWidth) break;
    drawGlyph(canvas, font, g, cursor, y, color, bgColor);
    cursor += g.advance;
  }
}

void uiDrawTextCentered(M5Canvas& canvas, const UiFont& font, const String& text,
                        int centerX, int y, uint16_t color, uint16_t bgColor,
                        int maxWidth) {
  int width = uiTextWidth(font, text);
  if (maxWidth >= 0 && width > maxWidth) width = maxWidth;
  uiDrawText(canvas, font, text, centerX - width / 2, y, color, bgColor, maxWidth);
}

#endif
