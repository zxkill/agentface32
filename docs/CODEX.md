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

## T3 Code / IDE sessions (app-server) and hook trust

Codex stores the approved hash of every non-managed hook in
`$CODEX_HOME/config.toml` (`[hooks.state.'<key>']`). Editing `hooks.json` —
even just adding a `commandWindows` variant — changes that hash and marks the
hook **modified**.

The interactive Codex TUI asks you to review modified hooks, but clients that
use `codex app-server` (T3 Code, IDE extensions) do not show that prompt: they
silently skip modified/untrusted hooks. `codex exec --dangerously-bypass-hook-trust`
masks the problem for one CLI run only, which makes this easy to misdiagnose.

Check the effective state:

```bash
python tools/codex_trust_hooks.py --check
```

If hooks show `modified`, re-approve the current definitions:

```bash
python tools/codex_trust_hooks.py            # updates existing hooks.state hashes
python tools/codex_trust_hooks.py --dry-run  # show the pending change
```

The tool asks `codex app-server` for the current hashes and writes them into the
existing `hooks.state` entries, keeping a timestamped backup of `config.toml`.
It never creates trust for a hook Codex has not already discovered. After it
runs, **restart T3 Code or start a new Codex thread** so the app-server reloads
the config.

The example has Unix/macOS `command` and Windows `commandWindows` forms. Both use `curl` and send stdin unchanged to:

```text
http://DEVICE_IP/hook/codex
```

## PermissionRequest and auto-approval

`PermissionRequest` means Codex is entering an approval path. In some setups a permission policy or automation can resolve that path shortly after the event is emitted, so immediately showing NEEDS YOU can create a noisy false alert. Payloads whose `permission_mode` is explicitly `dontAsk`, `bypassPermissions`, `autoApproved`, or `approved` are ignored by AgentFace32. Other requests use a configurable grace period:

```cpp
#define CODEX_PERMISSION_GRACE_MS 10000UL
```

**Improved behavior (v0.6.1+):**

1. `PermissionRequest` arrives → no visible alert yet.
2. **ANY** follow-up event from Codex (including repeated `PermissionRequest` with different tools or `permission_mode`) **cancels the pending alert** and resets the grace window.
3. Explicit `PermissionGranted` / `PermissionApproved` events are recognized and immediately clear the pending state.
4. Explicit `PermissionDenied` / `PermissionRejected` events are recognized and also clear the pending state.
5. If Codex remains quiet for the full grace period, AgentFace32 changes Codex to NEEDS YOU and emits the attention cue.

This handles slow auto-approval paths more reliably: if Codex keeps sending events (even more `PermissionRequest` events), the timer keeps refreshing until the approval is resolved or the grace period finally expires.

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
