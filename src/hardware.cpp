#include <Arduino.h>
#include "hardware.h"
#include "hardware_profile.h"
#include "config.h"

#if defined(PROFILE_M5STACK_BASIC)
  #include <M5Unified.h>

void hardwareBegin() {
  auto cfg = M5.config();
  cfg.serial_baudrate = 115200;
  cfg.internal_spk = true;
  M5.begin(cfg);
}
void hardwareUpdate() { M5.update(); }
void hardwareSetDisplayBrightness(uint8_t value) { M5.Display.setBrightness(value); }
bool hardwareButtonAClicked() { return M5.BtnA.wasClicked(); }
bool hardwareButtonBClicked() { return M5.BtnB.wasClicked(); }
bool hardwareButtonCClicked() { return M5.BtnC.wasClicked(); }
bool hardwareButtonCHeld() { return M5.BtnC.wasHold(); }

#else

struct ButtonState {
  int pin = -1;
  bool stablePressed = false;
  bool lastRawPressed = false;
  uint32_t changedAt = 0;
  uint32_t pressedAt = 0;
  bool click = false;
  bool hold = false;
  bool holdSent = false;
};

static ButtonState g_a, g_b, g_c;

static void beginButton(ButtonState& b, int pin) {
  b.pin = pin;
  if (pin >= 0) pinMode(pin, INPUT_PULLUP);
}

static void updateButton(ButtonState& b) {
  if (b.pin < 0) return;
  const uint32_t now = millis();
  const bool rawPressed = digitalRead(b.pin) == LOW;
  b.click = false;
  b.hold = false;

  if (rawPressed != b.lastRawPressed) {
    b.lastRawPressed = rawPressed;
    b.changedAt = now;
  }
  if (now - b.changedAt < 28) return;

  if (rawPressed != b.stablePressed) {
    b.stablePressed = rawPressed;
    if (rawPressed) {
      b.pressedAt = now;
      b.holdSent = false;
    } else {
      if (!b.holdSent && now - b.pressedAt < 850) b.click = true;
    }
  }
  if (b.stablePressed && !b.holdSent && now - b.pressedAt >= 850) {
    b.hold = true;
    b.holdSent = true;
  }
}

void hardwareBegin() {
  Serial.begin(115200);
  beginButton(g_a, BUTTON_A_PIN);
  beginButton(g_b, BUTTON_B_PIN);
  beginButton(g_c, BUTTON_C_PIN);
}
void hardwareUpdate() {
  updateButton(g_a);
  updateButton(g_b);
  updateButton(g_c);
}
void hardwareSetDisplayBrightness(uint8_t value) {
  (void)value; // Backlight is handled by the TFT renderer; OLED has no PWM backlight.
}
bool hardwareButtonAClicked() { const bool v = g_a.click; g_a.click = false; return v; }
bool hardwareButtonBClicked() { const bool v = g_b.click; g_b.click = false; return v; }
bool hardwareButtonCClicked() { const bool v = g_c.click; g_c.click = false; return v; }
bool hardwareButtonCHeld() { const bool v = g_c.hold; g_c.hold = false; return v; }

#endif
