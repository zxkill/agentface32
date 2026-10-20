# Architecture

AgentFace32 is intentionally host-light. There is no background daemon between the coding agent and the ESP32.

```mermaid
flowchart LR
    CC[Claude Code] -->|HTTP hook JSON| H1[/hook/claude]
    CX[Codex] -->|command hook + curl| H2[/hook/codex]
    CUSTOM[Other tool/script] -->|REST| REST[/anim]
    H1 --> PARSER[Agent event parser]
    H2 --> PARSER
    REST --> STATE[Agent contexts]
    PARSER --> STATE
    STATE --> C1[Claude context]
    STATE --> C2[Codex context]
    C1 --> UI[Display backend]
    C2 --> UI
    UI --> M5[M5GFX]
    UI --> OLED[U8g2]
    UI --> TFT[TFT_eSPI]
    STATE --> AUDIO[Optional audio backend]
```

## Core modules

- `agent.cpp` — provider registry (`claude`, `codex`), hook normalization, task timers, tool counts, permission grace logic.
- `state.cpp` / `translations.cpp` — stable machine states and English/Russian labels.
- `settings.cpp` — NVS-backed runtime settings: Wi-Fi, hostname, language, sound, brightness, enabled providers, admin password. `config.h` values are only defaults.
- `wifi_manager.cpp` — station connection, setup access point + captive DNS, fallback AP, mDNS.
- `http_api.cpp` — hook/API server, settings API, OTA upload, logs, crash data.
- `web_page.cpp` — the self-contained settings page served at `/`.
- `ota.cpp` — firmware writes and rollback confirmation.
- `logger.cpp` — log ring buffer in no-init RAM (survives software resets).
- `diagnostics.cpp` — reset reason, core dump summary, scheduled restarts.
- `menu.cpp` — on-device settings menu and status panels (Wi-Fi setup, OTA).
- `view.cpp` — backend-independent view selection (provider / All / Auto).
- `hardware.cpp` — buttons/basic board abstraction.
- `face_m5.cpp` — full 320×240 M5Stack UI.
- `face_oled.cpp` — 128×64 monochrome UI.
- `face_tft.cpp` — generic TFT_eSPI color UI.
- `audio.cpp` — M5 speaker, piezo, MAX98357A and no-audio backends.

## Adding a provider

1. Add an `AgentId` value and bump `AGENT_COUNT` in `agent.h`.
2. Add `{key, name}` to `kProviders` in `agent.cpp`. `/hook/<key>`, settings, the menu, the web page and the All view pick it up.
3. If its hook payload differs from Claude/Codex, normalize it in `agentHandleHookJson`.
4. Optionally give it a brand color in each `face_*.cpp`.

## Independent agent contexts

Providers never overwrite one another. Each context stores:

- state and detail
- timestamp of last event
- active task start time
- current/last tool count
- previous task duration
- pending Codex permission timing

The UI chooses which context to render; switching views does not change the contexts.

## Compile-time hardware profiles

`platformio.ini` selects `PROFILE_*` build flags. `hardware_profile.h` maps a profile to display/audio/button capabilities. Only the selected display backend defines the `face*()` functions, keeping one application layer across multiple boards.
