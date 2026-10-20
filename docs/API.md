# HTTP API

AgentFace32 runs a small HTTP server on port 80, both on your LAN and on the
setup access point (`http://192.168.4.1`).

The API is designed for trusted LAN use. Hooks, `/health`, `/state`, `/anim`
and `/view` are always open. Once an **admin password** is set in the web UI,
the settings page, `/api/*`, `/update`, `/logs` and `/coredump` require HTTP
Basic auth (user `admin`):

```bash
curl -u admin:PASSWORD http://DEVICE_IP/logs
```

`GET /help` prints a short endpoint list.

## `GET /health`

Returns firmware/hardware/network diagnostics plus every provider context.

```bash
curl http://DEVICE_IP/health
```

Useful fields include:

- firmware version and hardware profile
- IP/RSSI/free heap/minimum free heap
- previous reset reason and boot counter
- current view mode
- per-provider state, detail, timer and tool counts (`claude_*`, `codex_*`,
  `opencode_*`, `cline_*`, `cursor_*`, including `<provider>_enabled`)
- `app_partition`, `ota_pending_verify`, `ota_rolled_back`, `build`
- `coredump` (a crash dump is stored), `log_bytes`, `setup_active`

## `GET /state`

Smaller runtime snapshot of all provider contexts and the focused view.

## `POST /hook/claude`

Accepts a Claude Code lifecycle JSON payload.

```bash
curl -X POST http://DEVICE_IP/hook/claude \
  -H 'Content-Type: application/json' \
  -d '{"hook_event_name":"Stop"}'
```

`POST /hook` is kept as an alias for Claude-compatible integrations.

## `POST /hook/codex`

Accepts a Codex lifecycle JSON payload.

## `POST /hook/opencode`, `/hook/cline`, `/hook/cursor`

Each provider has its own endpoint and payload dialect:

| Provider | Event field | Notes |
| --- | --- | --- |
| `claude` | `hook_event_name` (PascalCase) | native HTTP hooks |
| `codex` | `hook_event_name` (PascalCase) | permission grace handling |
| `opencode` | `hook_event_name` | sent by `integrations/opencode/agentface32.js` |
| `cline` | `hookName` | `preToolUse.parameters`, `postToolUse.success` |
| `cursor` | `hook_event_name` (camelCase) | `stop.status` maps to done/abort/error |

The firmware normalizes provider-specific event and tool names into the same
internal states. Unknown events are acknowledged and ignored. Tool parameters
are read from `tool_input`, `preToolUse.parameters` (Cline) or `args`, so a
custom integration can send either shape.

Hooks for a provider that is disabled in settings return
`{"ok":true,"ignored":"provider disabled"}` and do not change the display.

## `POST /anim`

Manually set an agent state.

Parameters:

- `agent`: `claude` (default), `codex`, `opencode`, `cline` or `cursor`
- `name`: `idle`, `welcome`, `reading`, `thinking`, `typing`, `editing`, `running`, `attention`, `done`, `ring`, `error`, `abort`, `cancelled`, `sleep`

```bash
curl -X POST "http://DEVICE_IP/anim?agent=codex&name=running"
```

Read the current state with GET:

```bash
curl "http://DEVICE_IP/anim?agent=codex"
```

## `POST /view`

Switch display mode:

```bash
curl -X POST "http://DEVICE_IP/view?mode=claude"
curl -X POST "http://DEVICE_IP/view?mode=cursor"
curl -X POST "http://DEVICE_IP/view?mode=both"
curl -X POST "http://DEVICE_IP/view?mode=auto"
```

`GET /view` returns the current mode. Only enabled providers can be selected;
`both` (alias `all`) needs at least two enabled providers and shows a compact
card for every enabled agent.

## Settings: `GET/POST /api/settings`

`GET` returns the current settings. `POST` takes JSON with any subset of
fields; omitted fields are unchanged:

```json
{
  "wifi_ssid": "MyWiFi",
  "wifi_password": "secret123",
  "hostname": "agentface32",
  "language": "ru",
  "sound": true,
  "brightness": 60,
  "providers": {"claude": true, "codex": false, "opencode": true, "cline": true, "cursor": true},
  "admin_password": ""
}
```

Changing Wi-Fi or hostname restarts the device (`"restart": true` in the
response). At least one provider must stay enabled.

```bash
curl -X POST http://DEVICE_IP/api/settings -H 'Content-Type: application/json' -d '{"providers":{"codex":false}}'
```

## Wi-Fi setup

- `GET /api/wifi/scan` — async scan; returns `{"scanning":true}` until results
  are ready, then `{"networks":[{"ssid","rssi","secure"}]}`. `?refresh=1`
  starts a new scan.
- `POST /api/wifi/setup?action=start|stop` — open/close the setup access point
  without rebooting.

## Firmware update: `POST /update`

Multipart upload of `firmware.bin` (field name `firmware`). Optional
`?md5=<hex>` makes the device verify the image. The device restarts after a
successful upload.

```bash
curl -F firmware=@.pio/build/m5stack-basic/firmware.bin http://DEVICE_IP/update
python tools/agentface.py ota .pio/build/m5stack-basic/firmware.bin   # + md5, waits, verifies
pio run -e m5stack-basic -t ota --upload-port DEVICE_IP               # build + upload
```

A new image is confirmed after ~20 s of running with the network up. If it
crashes or is reset before that, the bootloader returns to the previous
firmware and `/health` reports `"ota_rolled_back": true`.

## Logs: `GET /logs`

Plain-text ring buffer (16 KB). It survives panics, watchdog resets, OTA and
software restarts; a power cycle clears it. Each boot starts with a line like
`===== boot #3 reset=panic fw=0.7.0 =====`.

The `X-Log-Next` response header is the offset to pass as `?since=` to fetch
only new lines:

```bash
curl -i "http://DEVICE_IP/logs?since=0"
python tools/agentface.py logs -f
```

## Crash data

- `GET /api/crash` — last reset reason plus a summary of the stored core dump:
  task, PC, exception cause, backtrace addresses.
- `GET /coredump` — raw core dump for `esp_coredump info_corefile -t raw`.
- `POST /api/crash/clear` — erase the stored dump.

Decode the backtrace against the ELF of the build that crashed:

```bash
python tools/agentface.py crash --env m5stack-basic
```

## `POST /api/reboot`

Restarts the device.

## Hook event contract

The parser intentionally ignores unknown events instead of returning an error. This makes the firmware tolerant of agent clients adding lifecycle events over time.

For a custom integration, the smallest useful payload is:

```json
{
  "hook_event_name": "PreToolUse",
  "tool_name": "Read",
  "tool_input": {
    "file_path": "src/main.cpp"
  }
}
```

Provider-native shapes are also accepted without translation:

```json
// Cursor
{"hook_event_name": "preToolUse", "tool_name": "Shell", "tool_input": {"command": "npm test"}}

// Cline
{"hookName": "PreToolUse", "preToolUse": {"toolName": "execute_command", "parameters": {"command": "npm test"}}}
```

Or skip lifecycle semantics entirely and drive `/anim` directly.
