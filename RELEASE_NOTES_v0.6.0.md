# AgentFace32 v0.6.0 — First public release

AgentFace32 is a local ESP32 desk display for **Claude Code** and **Codex**. It turns coding-agent lifecycle hooks into an expressive face, useful task status, current file/command, timers, tool counts and attention alerts — without running the AI on the microcontroller or requiring a cloud service.

## Highlights

- Independent Claude Code and Codex state on one device
- Claude / Codex / Both / Auto views
- Reading, thinking, editing, running, attention, done, error, cancelled and sleep states
- Current file, search, URL or shell command when available
- Per-task elapsed time and tool-call count
- Background-agent completion/attention indicators
- Codex permission grace to reduce false approval alerts
- English and Russian UI
- Optional sound
- Local REST API
- PlatformIO hardware profiles for M5Stack Basic, SSD1306, SH1106, ST7789, ESP32-S3 and LILYGO T-Display
- GitHub Actions build matrix

The **M5Stack Basic** profile is the physically tested reference build. Other profiles are included for compile/community validation and are clearly marked as such in the hardware table.

See `README.md` for quick start and `README_RU.md` for Russian documentation.
