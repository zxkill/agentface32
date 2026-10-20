# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project

AgentFace32 — ESP32 firmware (PlatformIO, Arduino framework) that renders coding-agent lifecycle hooks from Claude Code and Codex into an expressive desk display. The AI does not run on the ESP32; the device receives small JSON events over LAN via HTTP.

## Build

All profiles are built with PlatformIO. CI (`.github/workflows/build.yml`) builds every environment on each push/PR.

```bash
pio run -e m5stack-basic              # reference hardware, build only
pio run -e m5stack-basic -t upload    # flash to device
pio run --all-environments            # build every profile (CI does this)
```

Seven environments: `m5stack-basic`, `esp32dev-ssd1306`, `esp32dev-ssd1306-max98357`, `esp32dev-sh1106`, `esp32s3-ssd1306`, `lilygo-t-display`, `esp32dev-st7789`. After any change, build the affected environment **and** `m5stack-basic` (the reference profile) before finishing.

## Local development

1. First flash over USB (`pio run -e <env> -t upload`). On first boot the device opens the `AgentFace32-XXXX` access point (password `agentface`); configure Wi-Fi at `http://192.168.4.1`. Optionally `include/config.local.h` (gitignored) supplies default credentials.
2. Check the display (or `/health`) for the IP address. Verify with `curl http://DEVICE_IP/health`.
3. Test hooks: `curl -X POST "http://DEVICE_IP/hook/claude" -H 'Content-Type: application/json' -d '{"hook_event_name":"Stop"}'`

## Flashing and debugging over Wi-Fi

Set `AGENTFACE_HOST=<device ip>` (and `AGENTFACE_PASSWORD` if an admin password is set). No cable needed after the first flash:

```bash
pio run -e m5stack-basic -t ota                        # build + upload + wait + verify
python tools/agentface.py ota .pio/build/m5stack-basic/firmware.bin
python tools/agentface.py logs            # log ring buffer, survives crashes/restarts
python tools/agentface.py logs -f         # follow
python tools/agentface.py crash --env m5stack-basic    # core dump summary + decoded backtrace
python tools/agentface.py health
```

`tools/agentface.py ota` exits non-zero if the device does not come back or rolled back to the previous slot (a new image is confirmed after ~20 s of healthy running; a crash before that reverts it). After a rollback or crash, read `logs` and `crash` before retrying. The M5 partition table changed in v0.7 (two OTA slots + coredump); devices on older firmware need one USB flash.

## Architecture overview

```
┌──────────────┐      POST JSON       ┌──────────────────┐
│ Claude Code  │ ──────────────────▶  │                  │
│ Codex        │  /hook/claude /codex │  ESP32 firmware  │
│ Other tools  │ ──────────────────▶  │                  │
└──────────────┘                      └────────┬─────────┘
          HTTP hooks                          │
                                              ▼
                                    ┌──────────────────┐
                                    │ agent.cpp         │
                                    │  Normalize hooks  │
                                    │  Task timers      │
                                    │  Tool counts      │
                                    │  Permission grace │
                                    └────────┬─────────┘
                                             │
                        ┌────────────────────┼────────────────────┐
                        ▼                    ▼                    ▼
              ┌─────────────────┐  ┌─────────────────┐  ┌─────────────────┐
              │ Claude context  │  │ Codex context   │  │ Face view mode  │
              │ (independent)   │  │ (independent)   │  │ claude/codex/   │
              │                 │  │                 │  │ both/auto       │
              └────────┬────────┘  └────────┬────────┘  └────────┬────────┘
                       │                    │                     │
                       └────────────────────┼─────────────────────┘
                                            ▼
                              ┌──────────────────────────┐
                              │ face_*.cpp (renderer)    │
                              │   face_m5  → M5GFX 320×240│
                              │   face_oled → U8g2 128×64 │
                              │   face_tft  → TFT_eSPI    │
                              └──────────────────────────┘
                                            ▼
                              ┌──────────────────────────┐
                              │ hardware_profile.h       │
                              │ hardware.cpp (buttons)   │
                              │ audio.cpp (speaker/piezo)│
                              └──────────────────────────┘
```

**Key modules:**

- `src/agent.cpp` — event normalization, task timers, tool counts, Codex permission grace. Do not put board-specific code here.
- `src/http_api.cpp` — LAN REST/hook transport (`/hook/<provider>`, `/anim`, `/view`, `/health`, `/state`), settings API, `/update`, `/logs`, `/api/crash`, `/coredump`.
- `src/settings.cpp` — NVS runtime settings (Wi-Fi, providers, brightness, language, sound, admin password).
- `src/wifi_manager.cpp` — STA connection, setup AP + captive portal, fallback AP.
- `src/ota.cpp` / `src/logger.cpp` — OTA with rollback confirmation; log ring buffer in no-init RAM.
- `src/menu.cpp` / `src/view.cpp` — on-device menu and panels; shared view-mode logic used by all renderers.
- `src/state.cpp` / `src/translations.cpp` — state enum labels (EN/RU).
- `src/face_m5.cpp` — M5Stack Basic rich renderer (M5GFX, 320x240).
- `src/face_oled.cpp` — U8g2 128x64 monochrome renderer.
- `src/face_tft.cpp` — TFT_eSPI color renderer for ST7789 panels.
- `src/audio.cpp` — compile-time audio backends (M5 speaker, piezo, MAX98357A, none).
- `src/hardware.cpp` — button polling abstraction.
- `src/diagnostics.cpp` — reset reason, core dump summary, scheduled restarts.
- `include/hardware_profile.h` + `platformio.ini` — compile-time profile selection via `PROFILE_*` macros.
- `include/config.local.h` — user secrets/local overrides; never commit.

**Compile-time profile selection:** Each `PROFILE_*` flag in `platformio.ini` controls which display backend, audio backend, and button layout are compiled. Only the selected backend's `face*()` functions are defined. `hardware_profile.h` maps profiles to capabilities.

**Independent agent contexts:** providers (registry in `agent.cpp`, `AGENT_COUNT` in `agent.h`) never overwrite each other; disabled providers are hidden and their hooks ignored. Each stores state, detail, timestamps, task timer, tool count, and pending permission timing. The UI chooses which context to render; switching views does not alter contexts.

## Before finishing a change

1. Build the affected environment(s), then preferably all environments.
2. Update docs for new pins/events/configuration.
3. Never include real Wi-Fi credentials or user-specific IP addresses in committed files.
4. Use `logInfo/logWarn/logError` (`logger.h`) instead of `Serial.print*` so messages reach `/logs`.
5. Follow `AGENTS.md` for module boundary rules.

## CI

GitHub Actions runs `pio run -e <env>` for all seven environments on every push/PR. No test framework is used — the CI is a pure build-matrix gate.
