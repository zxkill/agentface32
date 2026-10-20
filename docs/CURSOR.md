# Cursor setup

Cursor can run shell hooks for agent sessions. AgentFace32 uses them as an
observer: the hook script forwards the payload to the device and always allows
Cursor to continue.

> Hooks are available in current Cursor releases (1.7+). Check
> **Cursor Settings → Hooks** if the menu item is missing, and update Cursor.

## 1. Copy the files

Project-level install (hooks apply only to this repository):

```bash
mkdir -p .cursor/hooks
cp integrations/cursor/agentface32.py .cursor/hooks/
cp integrations/cursor/hooks.json .cursor/hooks.json
```

Global install (all projects):

```bash
mkdir -p ~/.cursor/hooks
cp integrations/cursor/agentface32.py ~/.cursor/hooks/
cp integrations/cursor/hooks.json ~/.cursor/hooks.json
# global hooks run from ~/.cursor, so change the command in hooks.json to:
#   python3 ./hooks/agentface32.py
```

The project variant assumes scripts run from the project root, which is how
Cursor runs project hooks. On Windows use `python` instead of `python3` in
`hooks.json` if that is how Python is installed.

## 2. Point the script at the device

The default address is `http://agentface32.local`. If mDNS does not resolve,
set the device IP before starting Cursor:

```bash
export AGENTFACE_URL=http://192.168.1.50          # macOS / Linux
$env:AGENTFACE_URL = "http://192.168.1.50"        # PowerShell
```

Alternatively edit `DEFAULT_URL` in `agentface32.py`.

## 3. Verify

1. Start a Cursor agent chat.
2. Open `http://DEVICE_IP/health` and look for `cursor_seen: true` and
   `cursor_state`.
3. The Cursor checkbox must be enabled on the settings page (`/`), otherwise
   the firmware acknowledges hooks with `{"ignored":"provider disabled"}`.

Hooks never block or modify Cursor actions: the script prints
`{"permission": "allow"}` for permission hooks and `{}` otherwise, even if the
device is unreachable.

## Event mapping

| Cursor hook | AgentFace32 state |
| --- | --- |
| `sessionStart` | online / welcome |
| `sessionEnd` | sleep |
| `beforeSubmitPrompt` | new task |
| `preToolUse` | reading / editing / running, depending on the tool |
| `postToolUse` | processing result |
| `postToolUseFailure` | error |
| `subagentStart` / `subagentStop` | helper activity |
| `preCompact` | compacting context |
| `stop` (`status: completed/aborted/error`) | done / cancelled / error |

`beforeShellExecution`, `afterShellExecution`, `beforeMCPExecution`,
`afterMCPExecution`, `beforeReadFile`, `afterFileEdit` and the Tab hooks are
intentionally not forwarded: Cursor already emits generic `preToolUse` /
`postToolUse` for the same actions, and forwarding both would double the tool
count. If you want them anyway, add them to `hooks.json`; the firmware ignores
unknown event names safely.

## Troubleshooting

- **Nothing changes on the display.** Check that the script can reach the
  device: `curl http://DEVICE_IP/health`. Then look at `GET /logs` for lines
  starting with `cursor`.
- **`python3: command not found`.** Use `python` in `hooks.json` or install
  Python 3.
- **Cursor slows down.** Keep the hook timeout at 3 seconds; the script itself
  uses a 1.5 s network timeout and never fails the action.
