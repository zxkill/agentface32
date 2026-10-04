#pragma once

// Copy this file to include/config.local.h and edit the copy.
// config.local.h is intentionally ignored by Git.

#define WIFI_SSID "MyWiFi"
#define WIFI_PASSWORD "change-me"
#define DEVICE_HOSTNAME "agentface32"

// Supported UI languages: "en", "ru"
#define UI_LANGUAGE "en"

// M5Stack Basic can play the bundled short speech clips.
// Generic profiles use event tones unless you add your own audio backend.
#define VOICE_ENABLED_BY_DEFAULT false
#define SPEAKER_VOLUME 80
#define DISPLAY_BRIGHTNESS 180

// Delay before Codex PermissionRequest becomes a real attention alert.
#define CODEX_PERMISSION_GRACE_MS 10000UL

// Optional external buttons for generic ESP32 profiles. Use -1 to disable.
// Active-low buttons should connect the GPIO to GND when pressed.
#define BUTTON_A_PIN -1   // sound on/off
#define BUTTON_B_PIN -1   // demo current agent
#define BUTTON_C_PIN -1   // view mode; hold for AUTO

// Generic I2C OLED wiring.
#define OLED_SDA_PIN 21
#define OLED_SCL_PIN 22
#define OLED_I2C_ADDRESS 0x3C

// Optional passive piezo/buzzer.
#define PIEZO_PIN 26

// Optional MAX98357A wiring.
#define I2S_BCLK_PIN 27
#define I2S_LRCLK_PIN 26
#define I2S_DATA_PIN 25
