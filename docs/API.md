# HTTP API

AgentFace32 runs a small HTTP server on port 80 after Wi-Fi connects.

The API is designed for trusted LAN use and currently has no authentication.

## `GET /health`

Returns firmware/hardware/network diagnostics plus both agent contexts.

```bash
curl http://DEVICE_IP/health
```

Useful fields include:

- firmware version and hardware profile
- IP/RSSI/free heap/minimum free heap
- previous reset reason and boot counter
- current view mode
- Claude/Codex state, detail, timer and tool counts

## `GET /state`

Smaller runtime snapshot of both agent contexts and the focused view.

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

## `POST /anim`

Manually set an agent state.

Parameters:

- `agent`: `claude` (default) or `codex`
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
curl -X POST "http://DEVICE_IP/view?mode=codex"
curl -X POST "http://DEVICE_IP/view?mode=both"
curl -X POST "http://DEVICE_IP/view?mode=auto"
```

`GET /view` returns the current mode.

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

Or skip lifecycle semantics entirely and drive `/anim` directly.
