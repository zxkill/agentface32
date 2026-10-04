#pragma once
#include "config.h"

// Hardware profile is selected by PlatformIO build flags.
// Exactly one PROFILE_* macro should be defined per environment.

#if defined(PROFILE_M5STACK_BASIC)
  #define DISPLAY_BACKEND_M5 1
  #define AUDIO_BACKEND_M5 1
  #define HARDWARE_HAS_BUTTON_A 1
  #define HARDWARE_HAS_BUTTON_B 1
  #define HARDWARE_HAS_BUTTON_C 1
  #define HARDWARE_PROFILE_NAME "M5Stack Basic"

#elif defined(PROFILE_ESP32_SSD1306)
  #define DISPLAY_BACKEND_U8G2 1
  #define DISPLAY_DRIVER_SSD1306 1
  #define HARDWARE_PROFILE_NAME "ESP32 + SSD1306 128x64"

#elif defined(PROFILE_ESP32_SH1106)
  #define DISPLAY_BACKEND_U8G2 1
  #define DISPLAY_DRIVER_SH1106 1
  #define HARDWARE_PROFILE_NAME "ESP32 + SH1106 128x64"

#elif defined(PROFILE_LILYGO_TDISPLAY)
  #define DISPLAY_BACKEND_TFT_ESPI 1
  #define HARDWARE_PROFILE_NAME "LILYGO T-Display"
  #define HARDWARE_HAS_BUTTON_A 1
  #define HARDWARE_HAS_BUTTON_C 1
  #ifndef BUTTON_A_PIN
    #define BUTTON_A_PIN 35
  #endif
  #ifndef BUTTON_C_PIN
    #define BUTTON_C_PIN 0
  #endif

#elif defined(PROFILE_ESP32_ST7789)
  #define DISPLAY_BACKEND_TFT_ESPI 1
  #define HARDWARE_PROFILE_NAME "ESP32 + ST7789"

#else
  #error "No hardware profile selected. Use one of the PlatformIO environments from platformio.ini."
#endif

// Generic boards can opt into external buttons from config.local.h or build flags.
#if !defined(PROFILE_M5STACK_BASIC)
  #ifndef BUTTON_A_PIN
    #define BUTTON_A_PIN -1
  #endif
  #ifndef BUTTON_B_PIN
    #define BUTTON_B_PIN -1
  #endif
  #ifndef BUTTON_C_PIN
    #define BUTTON_C_PIN -1
  #endif
  #if BUTTON_A_PIN >= 0
    #define HARDWARE_HAS_BUTTON_A 1
  #endif
  #if BUTTON_B_PIN >= 0
    #define HARDWARE_HAS_BUTTON_B 1
  #endif
  #if BUTTON_C_PIN >= 0
    #define HARDWARE_HAS_BUTTON_C 1
  #endif
#endif

// Audio backends for generic boards.
#define AGENT_AUDIO_NONE 0
#define AGENT_AUDIO_PIEZO 1
#define AGENT_AUDIO_MAX98357 2

#ifndef AGENT_AUDIO_BACKEND
  #if defined(AUDIO_BACKEND_M5)
    #define AGENT_AUDIO_BACKEND 99
  #else
    #define AGENT_AUDIO_BACKEND AGENT_AUDIO_NONE
  #endif
#endif
