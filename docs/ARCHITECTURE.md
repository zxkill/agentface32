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

- `agent.cpp` — normalizes Claude/Codex hook payloads, task timers, tool counts, permission grace logic.
- `state.cpp` / `locale.cpp` — stable machine states and English/Russian labels.
- `http_api.cpp` — hook/API server.
- `hardware.cpp` — buttons/basic board abstraction.
- `face_m5.cpp` — full 320×240 M5Stack UI.
- `face_oled.cpp` — 128×64 monochrome UI.
- `face_tft.cpp` — generic TFT_eSPI color UI.
- `audio.cpp` — M5 speaker, piezo, MAX98357A and no-audio backends.
- `diagnostics.cpp` — reset reason and boot diagnostics.

## Two independent agent contexts

Claude and Codex never overwrite one another. Each context stores:

- state and detail
- timestamp of last event
- active task start time
- current/last tool count
- previous task duration
- pending Codex permission timing

The UI chooses which context to render; switching views does not change the contexts.

## Compile-time hardware profiles

`platformio.ini` selects `PROFILE_*` build flags. `hardware_profile.h` maps a profile to display/audio/button capabilities. Only the selected display backend defines the `face*()` functions, keeping one application layer across multiple boards.
