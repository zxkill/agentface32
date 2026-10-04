# Changelog

All notable public changes are documented here.

## [0.6.0] - 2026-10-04

### First public release

- Claude Code and Codex multi-agent monitoring.
- Independent per-agent state, details, timers and tool counters.
- Claude / Codex / Both / Auto views.
- Expressive M5Stack Basic UI with English/Russian localization.
- Compact SSD1306/SH1106 UI and generic ST7789/T-Display color UI.
- Hardware profiles for M5Stack Basic, ESP32 DevKit, ESP32-S3 and LILYGO T-Display.
- Optional M5 speaker, piezo and MAX98357A audio backends.
- 10-second Codex permission grace period to suppress false auto-approval alerts, with `dontAsk`/`bypassPermissions` suppression.
- Wi-Fi reconnect recovery starts the HTTP API even when the initial boot connection fails.
- Local HTTP API and direct lifecycle hook integrations.
- Reset diagnostics and GitHub Actions build matrix.
