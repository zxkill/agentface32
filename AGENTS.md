# AgentFace32 contributor map

This file is for coding agents working in the repository.

## Project goal

A local ESP32 desk display that observes Claude Code and Codex lifecycle hooks and renders expressive, useful multi-agent state without a host daemon.

## Architecture boundaries

- `src/agent.cpp`: provider event normalization, task timers, tool counts, Codex permission grace. Do not put board-specific code here.
- `src/http_api.cpp`: LAN REST/hook transport. Unknown lifecycle events should remain non-fatal.
- `src/settings.cpp`: NVS runtime settings; `config.h` macros are only first-boot defaults.
- `src/wifi_manager.cpp`: station + setup AP/captive portal. `src/ota.cpp`: OTA + rollback confirmation.
- `src/logger.cpp`: use `logInfo/logWarn/logError` instead of `Serial.print*` so logs reach `/logs`.
- `src/menu.cpp` / `src/view.cpp`: device menu/panels and view logic shared by all renderers; backends only draw.
- `src/face_m5.cpp`: M5Stack Basic rich renderer.
- `src/face_oled.cpp`: U8g2 128x64 renderer.
- `src/face_tft.cpp`: TFT_eSPI renderer.
- `src/audio.cpp`: compile-time audio backend.
- `include/hardware_profile.h` + `platformio.ini`: hardware selection.
- `include/config.local.h`: user secrets/local wiring overrides; never commit it.

## Compatibility priorities

1. Do not break `m5stack-basic` (reference hardware).
2. Keep every PlatformIO environment buildable.
3. Maintain both English and Russian UI.
4. Keep provider contexts independent.
5. Do not turn observer hooks into permission decisions.

## Before finishing a change

Build the affected environment(s), then preferably all environments from the README. Update docs for new pins/events/configuration. Never include real Wi-Fi credentials or user-specific IP addresses.
