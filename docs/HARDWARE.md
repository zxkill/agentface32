# Hardware and wiring

AgentFace32 separates application logic from display/audio hardware. Select a PlatformIO environment and the correct backend is compiled automatically.

## M5Stack Basic / Core ESP32

Environment: `m5stack-basic`

No wiring is required. The profile uses the built-in 320×240 display, buttons and speaker.

```bash
pio run -e m5stack-basic -t upload
```

The custom `partitions.csv` gives the application a larger single firmware slot. This is intentional: the bundled M5 UI/font/audio assets do not fit the default 1.25 MB application partition. USB flashing remains available; dual-slot Arduino OTA is not enabled in this profile.

## ESP32 DevKit + SSD1306 0.96" 128×64

Environment: `esp32dev-ssd1306`

Default wiring:

| SSD1306 | ESP32 DevKit |
| --- | --- |
| VCC | 3.3 V |
| GND | GND |
| SDA | GPIO21 |
| SCL | GPIO22 |

The default I²C address is `0x3C`. Override pins/address in `include/config.local.h`.

Optional passive piezo/buzzer signal: GPIO26. A piezo element is suitable for direct logic-level tones; **do not drive a low-impedance raw speaker directly from an ESP32 GPIO**.

## ESP32 DevKit + SH1106 1.3" 128×64

Environment: `esp32dev-sh1106`

Default wiring is the same I²C pinout as SSD1306. The firmware uses the U8g2 SH1106 backend.

## ESP32-S3 DevKitC-1 + SSD1306

Environment: `esp32s3-ssd1306`

Default profile pins:

| SSD1306 | ESP32-S3 |
| --- | --- |
| VCC | 3.3 V |
| GND | GND |
| SDA | GPIO8 |
| SCL | GPIO9 |

Optional piezo: GPIO4.

These are project defaults, not universal ESP32-S3 wiring. Override them if your board reserves those pins.

## SSD1306 + MAX98357A I²S amplifier

Environment: `esp32dev-ssd1306-max98357`

OLED wiring stays the same. Default MAX98357A pins:

| MAX98357A | ESP32 DevKit |
| --- | --- |
| BCLK | GPIO27 |
| LRC / WS | GPIO26 |
| DIN | GPIO25 |
| GND | GND |
| VIN | module-appropriate supply (commonly 5 V) |

Connect the speaker to the amplifier output, not to the ESP32. Check the voltage and speaker impedance required by your exact MAX98357A module.

The public MAX98357A backend currently plays event tones rather than speech.

## LILYGO TTGO T-Display (classic ESP32)

Environment: `lilygo-t-display`

The profile uses the board's built-in ST7789 display and buttons. The PlatformIO/TFT_eSPI build flags use LILYGO's common classic T-Display mapping:

- MOSI GPIO19
- SCLK GPIO18
- CS GPIO5
- DC GPIO16
- RST GPIO23
- backlight GPIO4
- buttons GPIO35 and GPIO0

Reference: [LILYGO T-Display documentation](https://github.com/Xinyuan-LilyGO/TTGO-T-Display).

Audio is disabled by default for this profile.

## Generic ESP32 DevKit + ST7789 240×240

Environment: `esp32dev-st7789`

Default project wiring:

| ST7789 | ESP32 DevKit |
| --- | --- |
| MOSI | GPIO23 |
| SCLK | GPIO18 |
| CS | GPIO5 |
| DC | GPIO16 |
| RST | GPIO17 |
| BL | GPIO4 |
| VCC/GND | according to your module |

ST7789 breakout boards are not standardized. If your module has no CS pin, uses a different resolution, or needs different inversion/offset settings, copy the PlatformIO environment and change its TFT_eSPI build flags.

## Optional buttons on generic boards

Buttons are active-low: connect the configured GPIO to GND when pressed. Internal pull-ups are enabled.

```cpp
#define BUTTON_A_PIN 32   // sound
#define BUTTON_B_PIN 33   // demo
#define BUTTON_C_PIN 25   // view / hold for AUTO
```

Use `-1` to disable a button. Without buttons, every important function is still available through HTTP (`/view`, `/anim`).

## Configuration overrides

Keep board-specific personal changes in `include/config.local.h` whenever possible:

```cpp
#define OLED_SDA_PIN 21
#define OLED_SCL_PIN 22
#define OLED_I2C_ADDRESS 0x3C
#define PIEZO_PIN 26
#define I2S_BCLK_PIN 27
#define I2S_LRCLK_PIN 26
#define I2S_DATA_PIN 25
```

If the change is intrinsic to a new board (display controller, fixed pins, screen size), add a real hardware profile instead. See [ADDING_HARDWARE.md](ADDING_HARDWARE.md).
