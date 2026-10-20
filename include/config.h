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
// Wi-Fi setup access point (first boot, menu "Wi-Fi setup", or fallback when
// the saved network is unreachable). SSID is "<prefix>-XXXX" from the MAC.
#ifndef SETUP_AP_PREFIX
  #define SETUP_AP_PREFIX "AgentFace32"
#endif
// WPA2 password for the setup AP (8+ characters). Shown on the display.
#ifndef SETUP_AP_PASSWORD
  #define SETUP_AP_PASSWORD "agentface"
#endif
// Start the setup AP when the saved network has been unreachable this long.
#ifndef WIFI_SETUP_FALLBACK_MS
  #define WIFI_SETUP_FALLBACK_MS 45000UL
#endif

#ifndef CODEX_PERMISSION_GRACE_MS
  #define CODEX_PERMISSION_GRACE_MS 10000UL
#endif

// After the initial grace period, wait this long before showing Attention
// for a Codex permission request. During this post-grace window the device
// stays quiet; if auto-approval resolves during this time, no alert fires.
// If the permission is still unresolved after grace + post-grace, Attention
// is shown. Override in config.local.h if needed.
#ifndef CODEX_PERMISSION_POST_GRACE_MS
  #define CODEX_PERMISSION_POST_GRACE_MS 8000UL
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
