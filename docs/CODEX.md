# Codex integration

Codex supports lifecycle hooks from `hooks.json` / `config.toml`. Command hooks receive the event JSON on stdin. AgentFace32's example simply pipes that JSON through `curl` to the ESP32.

Official reference: https://learn.chatgpt.com/docs/hooks

## Setup

1. Find the ESP32 IP address.
2. Copy `integrations/codex/hooks.example.json` to your Codex hooks location (commonly `~/.codex/hooks.json`), or merge its `hooks` object with your existing file.
3. Replace every `DEVICE_IP` with the device IP.
4. Restart Codex.
5. Open `/hooks`, review the new command hooks and mark them trusted when prompted.

Non-managed Codex hooks are intentionally subject to trust review before execution.

The example has Unix/macOS `command` and Windows `commandWindows` forms. Both use `curl` and send stdin unchanged to:

```text
http://DEVICE_IP/hook/codex
```

## PermissionRequest and auto-approval

`PermissionRequest` means Codex is entering an approval path. In some setups a permission policy or automation can resolve that path shortly after the event is emitted, so immediately showing NEEDS YOU can create a noisy false alert. Payloads whose `permission_mode` is explicitly `dontAsk` or `bypassPermissions` are ignored by AgentFace32. Other requests use a configurable grace period:

```cpp
#define CODEX_PERMISSION_GRACE_MS 10000UL
```

Behavior:

1. `PermissionRequest` arrives → no visible alert yet.
2. Any normal follow-up Codex event during the next 10 seconds cancels the pending alert.
3. If Codex remains quiet for the full grace period, AgentFace32 changes Codex to NEEDS YOU and emits the attention cue.

If you prefer a shorter/longer delay, override it in `include/config.local.h`.

## Typical mapping

| Codex event | AgentFace32 |
| --- | --- |
| `UserPromptSubmit` | start timer, READING |
| `PreToolUse Bash` | RUNNING + command |
| `PreToolUse apply_patch` | EDITING + affected file when detectable |
| read/search/MCP tools | READING/THINKING |
| `PostToolUse` | THINKING |
| `SubagentStart/Stop` | THINKING |
| `PermissionRequest` | delayed NEEDS YOU |
| `Interrupt` | CANCELLED |
| `Stop` | DONE |
| `SessionEnd` | SLEEP |

## Host requirement

The supplied hook uses `curl`. Modern Windows, macOS and most Linux distributions include it. If your environment does not, replace the command hook with any program that reads stdin and POSTs the same JSON to AgentFace32.
