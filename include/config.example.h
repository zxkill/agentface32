#pragma once

// Copy this file to include/config.local.h and edit the copy.
// config.local.h is intentionally ignored by Git.

// Everything below is optional. Wi-Fi, hostname, language, sound, brightness
// and providers are normally set on the device (setup access point / web UI
// at http://DEVICE_IP/ / hold A for the menu) and stored in flash. These
// values are only defaults for a device that has not been configured yet.
#define WIFI_SSID "MyWiFi"
#define WIFI_PASSWORD "change-me"
#define DEVICE_HOSTNAME "agentface32"

// Supported UI languages: "en", "ru"
#define UI_LANGUAGE "en"

// Setup access point "<prefix>-XXXX" and its WPA2 password (8+ characters).
#define SETUP_AP_PREFIX "AgentFace32"
#define SETUP_AP_PASSWORD "agentface"
// Open the setup AP when the saved network is unreachable for this long.
#define WIFI_SETUP_FALLBACK_MS 45000UL

// M5Stack Basic can play the bundled short speech clips.
// Generic profiles use event tones unless you add your own audio backend.
#define VOICE_ENABLED_BY_DEFAULT false
#define SPEAKER_VOLUME 80
#define DISPLAY_BRIGHTNESS 180   // 0..255, default for the brightness setting

// Delay before Codex PermissionRequest becomes a real attention alert.
// The firmware now cancels the pending alert on ANY follow-up event from Codex
// (including repeated PermissionRequest), not just non-PermissionRequest events.
// This handles slow auto-approval paths more reliably.
// Default: 10000UL (10 seconds). Reduce for faster reaction, increase if your
// Codex environment routinely spends longer in its approval path.
#define CODEX_PERMISSION_GRACE_MS 10000UL

// Post-grace delay: additional wait after grace period before showing Attention
// for a Codex permission request. Set to 0 for immediate alert after grace.
#define CODEX_PERMISSION_POST_GRACE_MS 8000UL

// Optional external buttons for generic ESP32 profiles. Use -1 to disable.
// Active-low buttons should connect the GPIO to GND when pressed.
#define BUTTON_A_PIN -1   // sound on/off, hold: settings menu
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
