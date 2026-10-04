# Troubleshooting

## Device gets an IP but HTTP does not respond

First test the actual IP, not mDNS:

```powershell
Test-NetConnection 192.168.1.50 -Port 80
Invoke-RestMethod "http://192.168.1.50/health"
```

The URL is `http://...`, not `http\://...`.

Make sure the PC and ESP32 are on the same LAN/VLAN and client isolation is not enabled on the Wi-Fi access point.

## Display flickers

The M5Stack and TFT backends draw into an off-screen sprite/canvas and push a complete frame. If you add another color display, do not clear the physical LCD and redraw it element-by-element every frame; use a framebuffer/sprite when RAM permits.

The 128×64 U8g2 profiles use the full-buffer `_F_` constructors for the same reason.

## M5Stack firmware is larger than 1.25 MB

Use the repository's `m5stack-basic` environment. It selects `partitions.csv`, giving the app a large single partition. Do not remove:

```ini
board_build.partitions = partitions.csv
board_upload.maximum_size = 3145728
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

The firmware delays Codex `PermissionRequest` by `CODEX_PERMISSION_GRACE_MS` (10 seconds by default). A following event cancels the alert.

If your Codex environment routinely spends longer in its approval path, increase in `config.local.h`:

```cpp
#define CODEX_PERMISSION_GRACE_MS 15000UL
```

If you use manual approvals, do not set this excessively high or real alerts will be delayed by the same amount.

## Russian text is missing/garbled

Use a supported profile. M5/TFT builds use the bundled UI font data; U8g2 builds use a Cyrillic-capable U8g2 font. Set:

```cpp
#define UI_LANGUAGE "ru"
```

## PlatformIO cannot find a header/library

Clean once after switching hardware profiles:

```bash
pio run -t clean
```

Then build the exact environment from `platformio.ini`. Do not copy `.pio` between machines.
