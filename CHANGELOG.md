# Changelog

All notable public changes are documented here.

## [0.8.0] - 2026-10-20

### More providers

- Added **opencode**, **Cline** and **Cursor** as independent providers next to
  Claude Code and Codex (`/hook/opencode`, `/hook/cline`, `/hook/cursor`).
- Per-provider payload normalization: Cursor camelCase hooks, Cline
  `hookName`/`preToolUse.parameters`, and canonical events from the bundled
  opencode plugin.
- Cross-provider tool-name mapping (`Shell`, `execute_command`, `bash`, …) for
  the same reading/editing/running states.
- All-enabled view now lays out a card per provider; brand colors for the new
  agents.
- Integrations: `integrations/opencode` plugin (one file for opencode V2 and
  V1 1.18.29+), `integrations/cline` installer, `integrations/cursor` hooks +
  forwarder.
- `tools/codex_trust_hooks.py` re-approves edited Codex hooks for app-server
  clients (T3 Code, IDE extensions), which otherwise skip `modified` hooks
  silently. Documented in `docs/CODEX.md` and troubleshooting.
- Documentation: `docs/OPENCODE.md`, `docs/CLINE.md`, `docs/CURSOR.md`,
  updated API/architecture/README in both languages.

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
