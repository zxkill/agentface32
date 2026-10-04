#pragma once

// Keep personal settings out of Git. Copy config.example.h to config.local.h
// and edit the copy. config.local.h is ignored by .gitignore.
#if __has_include("config.local.h")
  #include "config.local.h"
#endif

#ifndef WIFI_SSID
  #define WIFI_SSID "YOUR_WIFI_SSID"
#endif
#ifndef WIFI_PASSWORD
  #define WIFI_PASSWORD "YOUR_WIFI_PASSWORD"
#endif
#ifndef DEVICE_HOSTNAME
  #define DEVICE_HOSTNAME "agentface32"
#endif

// "en" or "ru". English is the public-project default.
#ifndef UI_LANGUAGE
  #define UI_LANGUAGE "en"
#endif

#ifndef VOICE_ENABLED_BY_DEFAULT
  #define VOICE_ENABLED_BY_DEFAULT false
#endif
#ifndef SPEAKER_VOLUME
  #define SPEAKER_VOLUME 80
#endif
#ifndef DISPLAY_BRIGHTNESS
  #define DISPLAY_BRIGHTNESS 180
#endif

// Codex can emit PermissionRequest even when its policy auto-approves the
// action moments later. We wait before alerting the human to suppress false
// attention notifications. Override this in config.local.h if desired.
#ifndef CODEX_PERMISSION_GRACE_MS
  #define CODEX_PERMISSION_GRACE_MS 10000UL
#endif

#ifndef ONE_SHOT_STATE_MS
  #define ONE_SHOT_STATE_MS 5000UL
#endif

// Generic OLED defaults. Override for your wiring.
#ifndef OLED_SDA_PIN
  #define OLED_SDA_PIN 21
#endif
#ifndef OLED_SCL_PIN
  #define OLED_SCL_PIN 22
#endif
#ifndef OLED_I2C_ADDRESS
  #define OLED_I2C_ADDRESS 0x3C
#endif

// Optional passive piezo/buzzer output.
#ifndef PIEZO_PIN
  #define PIEZO_PIN 26
#endif

// Optional MAX98357A I2S amplifier pins.
#ifndef I2S_BCLK_PIN
  #define I2S_BCLK_PIN 27
#endif
#ifndef I2S_LRCLK_PIN
  #define I2S_LRCLK_PIN 26
#endif
#ifndef I2S_DATA_PIN
  #define I2S_DATA_PIN 25
#endif
