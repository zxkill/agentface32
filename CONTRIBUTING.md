# Contributing

Thanks for helping make AgentFace32 useful on more desks and more hardware.

## Good first contributions

- validate an existing hardware profile on a real board and report exact module details
- add a common display/board profile
- improve event mapping for a new Claude Code or Codex lifecycle event
- improve documentation, translations or troubleshooting
- add a host integration that posts to the existing REST API

## Development setup

1. Install PlatformIO.
2. Fork/clone the repository.
3. Copy `include/config.example.h` to `include/config.local.h`.
4. Build at least the environment you are changing.
5. Before a PR, build the full matrix if practical:

```bash
pio run -e m5stack-basic
pio run -e esp32dev-ssd1306
pio run -e esp32dev-ssd1306-max98357
pio run -e esp32dev-sh1106
pio run -e esp32s3-ssd1306
pio run -e lilygo-t-display
pio run -e esp32dev-st7789
```

GitHub Actions runs the same matrix.

## Design rules

- Keep credentials out of source. `config.local.h` must stay local.
- Keep agent/event logic independent of hardware.
- Prefer adapting an existing display backend over duplicating the application.
- Keep network failures non-blocking from the agent's point of view.
- Unknown hook events should be safe to ignore.
- Public UI strings must go through English/Russian localization where appropriate.
- Document whether new hardware is physically tested or only compile-tested.

## Pull requests

Keep PRs focused. Include:

- what changed and why
- PlatformIO environments built
- real-hardware test result when applicable
- photo/video for UI or new hardware changes when useful

By contributing, you agree that your contribution is licensed under the repository's MIT license.
