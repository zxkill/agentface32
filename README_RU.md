# AgentFace32

**Настольный ESP32-индикатор, который даёт Claude Code и Codex лицо.**


AgentFace32 лежит на столе и показывает, чем прямо сейчас занят coding-agent: читает файлы, думает, редактирует код, выполняет команду, ждёт подтверждения, закончил работу или получил ошибку. Claude Code и Codex могут работать одновременно — состояние каждого хранится независимо.

Важно: **нейросеть на ESP32 не запускается**. Claude/Codex работают на компьютере, а устройство получает небольшие lifecycle-события по HTTP внутри локальной сети. Никакого облачного сервиса, аккаунта, API-ключа, базы данных или постоянно запущенного companion-daemon не требуется.

[English README](README.md)

## Что умеет

- одновременно отслеживает **Claude Code и Codex**;
- режимы экрана Claude / Codex / оба / AUTO;
- живые эмоции и состояния: читает, думает, пишет, выполняет, ждёт пользователя, готово, ошибка и т. д.;
- показывает файл, поисковый запрос, URL или выполняемую команду;
- считает время задачи и число вызовов инструментов;
- хранит последнее состояние обоих агентов при переключении экрана;
- показывает важное событие фонового агента;
- для Codex ждёт 10 секунд после `PermissionRequest`, чтобы авто-подтверждение не вызывало ложное «НУЖНО ВНИМАНИЕ»;
- русский и английский интерфейс;
- звук необязателен;
- REST API позволяет подключить другой агент или собственный скрипт;
- одна кодовая база для нескольких популярных ESP32 + OLED/TFT конфигураций.

## Железо

| PlatformIO environment | Железо | Экран | Статус |
| --- | --- | --- | --- |
| `m5stack-basic` | M5Stack Basic / Core ESP32 | встроенный 320×240 TFT | **проверено на реальном устройстве** |
| `esp32dev-ssd1306` | ESP32 DevKit + SSD1306 0.96″ | 128×64 I²C | валидация ожидается |
| `esp32dev-ssd1306-max98357` | ESP32 DevKit + SSD1306 + MAX98357A | 128×64 I²C | валидация ожидается |
| `esp32dev-sh1106` | ESP32 DevKit + SH1106 1.3″ | 128×64 I²C | валидация ожидается |
| `esp32s3-ssd1306` | ESP32-S3 DevKitC-1 + SSD1306 | 128×64 I²C | валидация ожидается |
| `lilygo-t-display` | LILYGO TTGO T-Display | встроенный ST7789 240×135 | валидация ожидается |
| `esp32dev-st7789` | ESP32 DevKit + ST7789 | 240×240 SPI | валидация ожидается |

M5Stack Basic — референсное и физически проверенное железо. Остальные профили подготовлены для публичного проекта и добавлены в CI-матрицу, но им ещё нужна первая проверка сборкой и на реальном железе; отчёты о конкретных модулях приветствуются.

Подключение: [docs/HARDWARE.md](docs/HARDWARE.md).

## Быстрый запуск

1. Установите PlatformIO.
2. Создайте локальный конфиг:

```powershell
Copy-Item include/config.example.h include/config.local.h
```

3. Укажите Wi-Fi и язык:

```cpp
#define WIFI_SSID "MyWiFi"
#define WIFI_PASSWORD "change-me"
#define UI_LANGUAGE "ru"
```

4. Для M5Stack Basic:

```bash
pio run -e m5stack-basic -t upload
```

Для ESP32 + SSD1306:

```bash
pio run -e esp32dev-ssd1306 -t upload
```

5. После подключения к Wi-Fi проверьте:

```powershell
Invoke-RestMethod "http://DEVICE_IP/health"
```

6. Подключите хуки:
   - [Claude Code](docs/CLAUDE_CODE.md)
   - [Codex](docs/CODEX.md)

`include/config.local.h` добавлен в `.gitignore`, поэтому пароль Wi-Fi случайно не попадёт в GitHub.

## Управление на M5Stack Basic

- **A** — звук вкл./выкл.;
- **B** — демонстрация эмоций текущего агента;
- **C** — Claude → Codex → ОБА;
- **удерживать C** — AUTO.

В AUTO отображается тот агент, от которого последним пришло реальное событие.

## REST API

```powershell
Invoke-RestMethod -Method Post "http://DEVICE_IP/anim?agent=claude&name=thinking"
Invoke-RestMethod -Method Post "http://DEVICE_IP/anim?agent=codex&name=done"
Invoke-RestMethod -Method Post "http://DEVICE_IP/view?mode=both"
Invoke-RestMethod "http://DEVICE_IP/state"
```

Подробнее: [docs/API.md](docs/API.md).

## Документация

- [Железо и подключение](docs/HARDWARE.md)
- [Claude Code](docs/CLAUDE_CODE.md)
- [Codex](docs/CODEX.md)
- [API](docs/API.md)
- [Архитектура](docs/ARCHITECTURE.md)
- [Диагностика](docs/TROUBLESHOOTING.md)
- [Как добавить новую плату](docs/ADDING_HARDWARE.md)
- [Как снять GIF/видео для GitHub](docs/MEDIA.md)

## Безопасность

Сервис локальный и не получает ключи Anthropic/OpenAI, однако lifecycle-hook может содержать имя файла, команду Bash/PowerShell и другие данные разработки. Используйте устройство только в доверенной локальной сети и не пробрасывайте HTTP-порт ESP32 в интернет.

## История

Идея проекта появилась после знакомства с [Tiny Engineer](https://github.com/jamro/tiny-engineer): захотелось получить ту же физическую обратную связь от coding-agent, но без сборки робота — используя экран и динамик уже лежащего без дела M5Stack. Затем появились русский интерфейс, информативные статусы, Claude + Codex и поддержка обычных ESP32-дисплеев.

## Лицензия

MIT. Проект независимый и не связан с Anthropic, OpenAI, Espressif, M5Stack или LILYGO.
