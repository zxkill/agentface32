# Adding a board or display

The preferred contribution is a new PlatformIO profile, not a fork of the application logic.

## 1. Add a profile macro

Edit `include/hardware_profile.h` and map a new `PROFILE_*` to one of the existing display backends where possible:

- `DISPLAY_BACKEND_M5`
- `DISPLAY_BACKEND_U8G2`
- `DISPLAY_BACKEND_TFT_ESPI`

If the controller is SSD1306/SH1106, reuse the U8g2 renderer. If it is an ST77xx-like color display supported by TFT_eSPI, try the generic TFT renderer first.

## 2. Add a PlatformIO environment

Add an `[env:...]` section to `platformio.ini` with the correct board ID, libraries and fixed pins.

Keep user-adjustable wiring in `config.local.h`; keep physical board constants in the environment/profile.

## 3. Add it to CI

Add the environment name to `.github/workflows/build.yml`.

## 4. Document wiring

Update `docs/HARDWARE.md`. State clearly whether the profile is physically tested or only compile-tested.

## 5. Avoid board-specific logic in `agent.cpp`

Agent parsing, timers, API behavior and localization should remain board-independent. Hardware-specific behavior belongs in display/audio/hardware backends.

## Pull request checklist

- clean build for the new environment
- existing build matrix remains green
- no credentials in source
- wiring documented
- photo of a real build if available
- mention exact display module/controller variant
