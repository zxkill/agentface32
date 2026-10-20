# Cline setup

Cline runs executable hook files around task and tool activity. AgentFace32
ships one Python forwarder plus an installer that creates the hook files with
the required names.

> Cline hooks are currently supported on macOS/Linux. Cline does not run hooks
> on Windows yet, so use the device's HTTP API directly there if needed.

## 1. Install the hooks

From the repository root:

```bash
./integrations/cline/install.sh
```

This writes into `.clinerules/hooks/` of the current workspace:

```text
.clinerules/hooks/
  agentface32-hook.py
  TaskStart
  TaskResume
  TaskCancel
  TaskComplete
  UserPromptSubmit
  PreToolUse
  PostToolUse
  PreCompact
```

For all workspaces, install into Cline's global directory instead:

```bash
./integrations/cline/install.sh ~/Documents/Cline/Hooks
```

Cline skips files that are not executable, so the installer runs
`chmod +x` for every hook.

## 2. Enable hooks in Cline

Open the Cline settings in VS Code → **Feature Settings** → enable
**Hooks**. Restart the extension if it was already running.

## 3. Point the hook at the device

The forwarder uses `http://agentface32.local` by default. Set the address in
the environment that launches VS Code:

```bash
export AGENTFACE_URL=http://192.168.1.50
code .
```

Or edit `DEFAULT_URL` in `.clinerules/hooks/agentface32-hook.py`. The hook
prints `{"cancel": false}` even when the device is offline, so Cline is never
blocked by the display.

## 4. Verify

Run a Cline task and check:

```bash
curl http://DEVICE_IP/health   # cline_seen / cline_state
curl http://DEVICE_IP/logs     # lines starting with "cline"
```

Make sure the Cline checkbox is enabled on the device settings page (`/`).

## Event mapping

| Cline hook | AgentFace32 state |
| --- | --- |
| `TaskStart` | online / welcome |
| `TaskResume` | session resumed |
| `UserPromptSubmit` | new task |
| `PreToolUse` | reading / editing / running, depending on the tool |
| `PostToolUse` | processing result |
| `PostToolUse` with `success: false` | error |
| `TaskCancel` | cancelled |
| `TaskComplete` | done (Cline marks this hook "coming soon") |
| `PreCompact` | compacting context |

Cline tool names are normalized by the firmware: `execute_command` →
running, `read_file` / `search_files` / `list_files` → reading,
`write_to_file` / `replace_in_file` → editing.

## Troubleshooting

- **Hooks do not fire.** Confirm **Enable Hooks** is checked, the files are
  executable (`ls -l .clinerules/hooks`), and the first line is a shebang.
- **Permission denied.** Run `chmod +x .clinerules/hooks/*`.
- **Device returns 200 but nothing changes.** Check that the Cline provider is
  enabled in `/api/settings`; disabled providers ignore hooks by design.
