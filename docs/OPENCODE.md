# opencode setup

opencode has a plugin API (not shell hooks). The bundled plugin subscribes to
session, prompt and tool events and forwards them to `/hook/opencode`.

The same file supports **opencode V2** (`setup(ctx)` with tool/session/event
hooks) and **V1** from 1.18.29 (`server()` hooks). opencode V2 loads local
plugins from `.opencode/plugins/` and `~/.config/opencode/plugins/`.

## 1. Install the plugin

Per project:

```bash
mkdir -p .opencode/plugins
cp integrations/opencode/agentface32.js .opencode/plugins/
```

Globally for all projects:

```bash
mkdir -p ~/.config/opencode/plugins
cp integrations/opencode/agentface32.js ~/.config/opencode/plugins/
```

Alternatively list the file/package in `opencode.json`. Use the key that
matches your opencode major version:

```jsonc
// opencode V2
{
  "$schema": "https://opencode.ai/config.json",
  "plugins": [{ "package": "./integrations/opencode/agentface32.js" }]
}

// opencode V1
{
  "$schema": "https://opencode.ai/config.json",
  "plugin": ["./integrations/opencode/agentface32.js"]
}
```

Restart opencode after adding the plugin. If you use the desktop app or the
background service, the already running server keeps its old plugin list:
quit it or run `opencode run --standalone ...` once to start a fresh server.

## 2. Point the plugin at the device

The default is `http://agentface32.local`. Override it in the environment that
starts opencode:

```bash
export AGENTFACE_URL=http://192.168.1.50
opencode
```

The plugin sends requests fire-and-forget with a 1.5 s timeout. If the device
is offline, opencode is not delayed or interrupted.

## 3. Verify

```bash
curl http://DEVICE_IP/health   # opencode_seen / opencode_state
curl http://DEVICE_IP/logs     # lines starting with "opencode"
```

Enable **OpenCode** on the device settings page (`/`) first; disabled
providers acknowledge and ignore hooks.

## Event mapping

| opencode event / hook | AgentFace32 state |
| --- | --- |
| `session.created` | online / welcome (`SubagentStart` for child sessions) |
| `session.deleted` | sleep (`SubagentStop` for child sessions) |
| `session.execution.started` | new task |
| `tool.execute.before` | reading / editing / running, depending on the tool |
| `tool.execute.after` | processing result |
| `permission.asked` | needs attention |
| `permission.replied` | permission granted / denied |
| `session.execution.succeeded` | done |
| `session.execution.failed` | error |
| `session.execution.interrupted` | cancelled |
| `session.step.ended` with `finish: "stop"` | done (per-turn fallback) |
| `session.compaction.started` / `.ended` | compacting context |

opencode V2 does **not** emit `session.idle` (it exists in the schema but is
never published). Turn completion is normally the `session.execution.*`
terminal event, so the plugin reports **done** when a root session's execution
succeeds, fails or is interrupted. Subagent sessions are recognised by
`session.created` / `parentID`, so their completion does not end the task
early.

Some clients (T3 Code) keep one long-lived execution open across user turns,
so `session.execution.succeeded` may not arrive when a single answer is ready.
The plugin therefore also treats a final `session.step.ended` with
`finish: "stop"` as **done**. This is a per-turn signal and clears the display
even when the surrounding session stays open.

V1 uses `chat.message` (new task), `session.idle` (done), `session.error` and
`session.compacted`; the same plugin maps those as well.

The plugin sends canonical PascalCase event names (`hook_event_name`), so the
firmware treats opencode like its other providers while still accepting
opencode's lowercase tool ids (`bash`, `read`, `edit`, `grep`, `webfetch`).

## Troubleshooting

- **No events.** Make sure the file ends in `.js` or `.ts` inside a `plugins/`
  directory and restart opencode. Check the opencode log for plugin load
  errors.
- **Device unreachable by name.** Use an IP address in `AGENTFACE_URL`; some
  systems do not resolve `.local` names inside sandboxed processes.
- **Events arrive but the face does not change.** Confirm the provider is
  enabled and look at `GET /logs` on the device.
