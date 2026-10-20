# Troubleshooting

## Reading device logs

Everything the firmware logs goes to Serial **and** to a 16 KB RAM ring buffer
that survives crashes, watchdog resets, OTA and software restarts (a power
cycle clears it):

```bash
python tools/agentface.py logs        # or: curl http://DEVICE_IP/logs
python tools/agentface.py logs -f     # follow
```

Each boot starts with `===== boot #N reset=<reason> fw=<version> =====`, so the
lines just above a `reset=panic` banner are what happened before the crash.

## Device crashed / rebooted by itself

```bash
python tools/agentface.py crash --env m5stack-basic
```

prints the reset reason and, if a core dump is stored, the crashing task, PC,
exception cause and a backtrace decoded against
`.pio/build/<env>/firmware.elf` (must be the same build that crashed). The raw
dump is at `GET /coredump` for `esp_coredump`. Clear it with
`POST /api/crash/clear`.

## Cannot connect to Wi-Fi / moved to another network

If the saved network is unreachable for ~45 s the device opens the setup access
point `AgentFace32-XXXX` (password `agentface`) and keeps retrying the saved
network in the background. Join it and open `http://192.168.4.1`. You can also
start setup from the device menu (hold **A** → Wi-Fi setup) or from the web page.

Some phones stay on mobile data when the AP has no internet; turn mobile data
off or open `http://192.168.4.1` manually.

## OTA upload fails or the old version comes back

- `/health` → `ota_rolled_back: true` means the new image crashed or was reset
  before it was confirmed (~20 s with network up), so the bootloader went back
  to the previous firmware. Check `logs` and `crash`.
- Devices flashed with v0.6 or older on M5Stack have the old single-slot
  partition table: flash once over USB.
- With an admin password set, pass `AGENTFACE_PASSWORD` (tools) or
  `curl -u admin:PASSWORD`.

## Device gets an IP but HTTP does not respond

First test the actual IP, not mDNS:

```powershell
Test-NetConnection 192.168.1.50 -Port 80
Invoke-RestMethod "http://192.168.1.50/health"
```

The URL is `http://...`, not `http\://...`.

Make sure the PC and ESP32 are on the same LAN/VLAN and client isolation is not enabled on the Wi-Fi access point.

## Provider hooks do not arrive

- Confirm the provider is enabled: `curl http://DEVICE_IP/api/settings` (or the
  checkboxes at `/`). Disabled providers return
  `{"ok":true,"ignored":"provider disabled"}`.
- Send a test payload directly:
  `curl -X POST http://DEVICE_IP/hook/cursor -H 'Content-Type: application/json' -d '{"hook_event_name":"stop","status":"completed"}'`
- If the direct call works, the forwarder cannot reach the device. Check
  `AGENTFACE_URL`, use the IP instead of `agentface32.local`, and look at
  `curl http://DEVICE_IP/logs` for lines starting with the provider key.
- Each guide has provider-specific notes: [opencode](OPENCODE.md),
  [Cline](CLINE.md), [Cursor](CURSOR.md).

## Display flickers

The M5Stack and TFT backends draw into an off-screen sprite/canvas and push a complete frame. If you add another color display, do not clear the physical LCD and redraw it element-by-element every frame; use a framebuffer/sprite when RAM permits.

The 128×64 U8g2 profiles use the full-buffer `_F_` constructors for the same reason.

## M5Stack firmware is larger than 1.25 MB

Use the repository's `m5stack-basic` environment. It selects `partitions.csv`: two 1.875 MB app slots (OTA with rollback) and a core dump slot. Do not remove:

```ini
board_build.partitions = partitions.csv
board_upload.maximum_size = 1966080
```

## M5Stack quietly hisses/crackles when idle

The M5 audio backend ends the speaker subsystem after playback. If you are modifying audio code, avoid leaving the DAC/speaker path continuously active.

## Random resets

Call:

```bash
curl http://DEVICE_IP/health
```

Look at:

```json
"last_reset": "brownout",
"min_free_heap": 123456
```

Common meanings:

- `brownout` — power/cable/current problem
- `task_watchdog` / `watchdog` — blocked task or long critical section
- `panic` — software crash
- `software` — program-requested restart

For noisy or unstable setups, use a good USB cable and power source. Speakers and bright backlights increase peak current.

## Codex keeps saying NEEDS YOU while auto-approval is on

**v0.6.1+ improvement:** The firmware now cancels the pending alert on **ANY** follow-up event from Codex (including repeated `PermissionRequest`), not just non-PermissionRequest events. It also recognizes explicit `PermissionGranted`/`PermissionApproved` events.

If you still see false alerts, your Codex environment may spend longer than the grace period in the approval path. Increase `CODEX_PERMISSION_GRACE_MS` in `config.local.h`:

```cpp
#define CODEX_PERMISSION_GRACE_MS 15000UL  // or longer
```

If you use manual approvals, do not set this excessively high or real alerts will be delayed by the same amount.

**Debug tip:** Check the logs (`python tools/agentface.py logs -f` or Serial Monitor) for messages like:
- `codex: permission granted, clearing pending alert` — permission was auto-approved quickly
- `codex: permission wait cancelled by hook=PreToolUse` — follow-up event came in time
- `codex: permission still pending, refreshing grace window` — multiple PermissionRequest events received
- `codex: post-grace expired -> showing attention` — no events for the full grace period (real attention needed)

## Russian text is missing/garbled

Use a supported profile. M5/TFT builds use the bundled UI font data; U8g2 builds use a Cyrillic-capable U8g2 font. Switch the language in the device menu or web page (`UI_LANGUAGE` in `config.local.h` is only the default for a fresh device).

## PlatformIO cannot find a header/library

Clean once after switching hardware profiles:

```bash
pio run -t clean
```

Then build the exact environment from `platformio.ini`. Do not copy `.pio` between machines.
