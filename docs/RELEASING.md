# Making a release locally

GitHub Actions may be unavailable (for example when Actions minutes are
blocked). The firmware is small and fully reproducible on a development
machine; create the release with `gh` afterwards.

## 1. Bump the version

Edit `include/version.h` (`AGENT_COMPANION_VERSION`), commit and push.

## 2. Build every environment

```bash
for env in m5stack-basic esp32dev-ssd1306 esp32dev-ssd1306-max98357 \
           esp32dev-sh1106 esp32s3-ssd1306 lilygo-t-display esp32dev-st7789; do
  pio run -e "$env" || exit 1
done
```

## 3. Package OTA and full images

`-ota.bin` is `.pio/build/<env>/firmware.bin` and is used by `/update` or
`tools/agentface.py ota`. It keeps NVS/settings.

`-full.bin` is a merged image flashed at `0x0`: bootloader, partition table,
`boot_app0` and the application. It erases NVS.

ESP32 boards use bootloader offset `0x1000`; ESP32-S3 uses `0x0`:

```bash
ESPTOOL=~/.platformio/packages/tool-esptoolpy/esptool.py
BOOTAPP=~/.platformio/packages/framework-arduinoespressif32/tools/partitions/boot_app0.bin
ENV=m5stack-basic
VER=0.8.0

python "$ESPTOOL" --chip esp32 merge_bin \
  -o "dist/v$VER/agentface32-v$VER-$ENV-full.bin" \
  --flash_mode qio --flash_freq 80m --flash_size 4MB \
  0x1000 ".pio/build/$ENV/bootloader.bin" \
  0x8000 ".pio/build/$ENV/partitions.bin" \
  0xe000 "$BOOTAPP" \
  0x10000 ".pio/build/$ENV/firmware.bin"
```

Per-environment flash settings for the merge command:

| Environment | Chip | Mode | Frequency | Size | Bootloader |
| --- | --- | --- | --- | --- | --- |
| `m5stack-basic` | `esp32` | `qio` | `80m` | `4MB` | `0x1000` |
| `esp32dev-*` | `esp32` | `dio` | `40m` | `4MB` | `0x1000` |
| `lilygo-t-display` | `esp32` | `dio` | `40m` | `4MB` | `0x1000` |
| `esp32s3-ssd1306` | `esp32s3` | `qio` | `80m` | `8MB` | `0x0` |

## 4. Checksums and notes

```bash
cd "dist/v$VER"
sha256sum *.bin > SHA256SUMS.txt
```

Write `RELEASE_NOTES.md` with the download table and install instructions.

## 5. Create the GitHub release

```bash
gh release create "v$VER" \
  --repo zxkill/agentface32 \
  --target main \
  --title "AgentFace32 v$VER" \
  --notes-file "dist/v$VER/RELEASE_NOTES.md" \
  dist/v$VER/*.bin dist/v$VER/SHA256SUMS.txt
```

If the token cannot create releases, the fallback is to commit the images under
`releases/v$VER/` with the same notes; a release is still preferable because it
keeps binaries out of the Git history.

## 6. Publish the update manifest

The device does not call the GitHub API (large response, 60 requests/hour). It
reads a small generated `update.json` from the `main` branch:

```bash
python tools/make_update_manifest.py \
  --version "$VER" --dist "dist/v$VER" --out update.json
git add update.json
git commit -m "chore: publish update manifest for v$VER"
git push origin main
```

Do this **after** the release assets are uploaded: devices that check in the
gap would otherwise get a 404 for a file that does not exist yet.
`raw.githubusercontent.com` caches `update.json` for a few minutes, so allow
that before expecting already-running devices to see the new version. The firmware
compares `version` with its own and downloads
`https://github.com/zxkill/agentface32/releases/download/v<version>/<file>` for
its `AGENTFACE_RELEASE_ENV`.
