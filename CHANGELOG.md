# Changelog

All notable public changes are documented here.

## [0.9.0] - 2026-10-20

### Updates from GitHub

- The device checks a small `update.json` manifest in the repository, compares
  the published version with its own and offers the matching OTA asset.
- Check manually from the settings menu or the web page; auto-check is on by
  default (15 s after boot, then every 24 h) and can be disabled.
- One-press install from the device menu or the web UI: HTTPS download into the
  inactive OTA slot, progress screen, restart and the existing rollback
  protection.
- TLS uses pinned GitHub root CAs (USERTrust ECC + ISRG Root X1); no insecure
  fallback.
- Release images for all seven hardware profiles are attached to GitHub
  releases; `docs/RELEASING.md` and `tools/make_update_manifest.py` cover the
  local release flow.

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
- The opencode plugin also completes a task on `session.step.ended` with
  `finish: "stop"`, because T3 Code can keep one execution open across turns
  and never send `session.execution.succeeded` per answer.
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
