# Claude Code integration

Claude Code supports lifecycle hooks in `~/.claude/settings.json` (all projects) and `.claude/settings.json` (one project). For most events it can send the hook JSON directly to an HTTP endpoint, which makes AgentFace32 dependency-free on the host.

Official reference: https://code.claude.com/docs/en/hooks

## Recommended setup

1. Find the IP shown by AgentFace32 or call `/health`.
2. Copy `integrations/claude-code/settings.example.json`.
3. Replace every `DEVICE_IP` with the ESP32 address, for example `192.168.1.50`.
4. Merge the `hooks` object into your existing `~/.claude/settings.json` rather than overwriting unrelated settings.
5. Restart Claude Code and inspect `/hooks`.

The example intentionally starts with `UserPromptSubmit` rather than `SessionStart`. Current Claude Code only supports `command` and `mcp_tool` handlers for `SessionStart`; direct `type: "http"` is not accepted there. The first prompt therefore becomes the first visible event, with no helper script required.

## Why HTTP hooks

Claude Code POSTs the normal lifecycle JSON body directly to:

```text
http://DEVICE_IP/hook/claude
```

AgentFace32 responds with `{}` and never makes permission decisions for Claude. It is an observer only.

Typical mapping:

| Claude event | AgentFace32 |
| --- | --- |
| `UserPromptSubmit` | start timer, READING |
| `PreToolUse Read/Grep/Glob/WebFetch/WebSearch` | READING + detail |
| `PreToolUse Edit/Write/NotebookEdit` | EDITING + file |
| `PreToolUse Bash/PowerShell` | RUNNING + command |
| `PostToolBatch` | THINKING |
| `SubagentStart/Stop` | THINKING |
| `PermissionRequest` | NEEDS YOU |
| selected `Notification` types | NEEDS YOU |
| `PostToolUseFailure` / `StopFailure` | ERROR |
| `PermissionDenied` | CANCELLED |
| `Stop` | DONE + duration/tool count |
| `SessionEnd` | SLEEP |

Informational Claude notifications such as auth success are ignored to avoid false attention alerts.

## If your organization uses HTTP hook allowlists

Claude Code can restrict hook destinations with `allowedHttpHookUrls`. If your settings or managed policy defines an allowlist, add your device URL there as allowed by your organization's policy.

## Test manually

```bash
curl -X POST http://DEVICE_IP/hook/claude \
  -H 'Content-Type: application/json' \
  -d '{"hook_event_name":"UserPromptSubmit"}'
```

Then:

```bash
curl -X POST http://DEVICE_IP/hook/claude \
  -H 'Content-Type: application/json' \
  -d '{"hook_event_name":"PreToolUse","tool_name":"Read","tool_input":{"file_path":"src/main.cpp"}}'
```
