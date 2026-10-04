# Карточка GitHub и чек-лист публикации

## Название

**Product:** `AgentFace32`

**Repository:** `agentface32`

Название `esp32-agent-companion` лучше не использовать: на GitHub уже существует заметный проект с точно таким именем и близкой идеей. `AgentFace32` короче, запоминается и всё ещё явно связывает проект с ESP32.

## GitHub About / Description

Рекомендуемый вариант:

> Animated ESP32 desk display for Claude Code and Codex. Multi-agent status, expressive faces, task timers, hooks, OLED/TFT/M5Stack support, optional sound — all local, no cloud.

## Topics

Добавить в **About → Topics**:

```text
esp32
claude-code
codex
ai-agents
coding-agent
developer-tools
platformio
m5stack
ssd1306
sh1106
st7789
lilygo
multi-agent
iot
arduino
```

## Первый релиз

Tag:

```text
v0.6.0
```

Title:

```text
AgentFace32 v0.6.0 — First public release
```

Короткое описание релиза:

> First public release of AgentFace32: a local ESP32 desk display for Claude Code and Codex, with independent multi-agent state, expressive UI, task details/timers, English/Russian localization, REST hooks, and hardware profiles for M5Stack Basic, SSD1306, SH1106, ST7789 and LILYGO T-Display.

## Репозиторий

Рекомендуемые настройки:

- Public
- Default branch: `main`
- License: MIT
- Issues: enabled
- Discussions: можно включить после первых пользователей
- Wiki: не нужен, документация уже в `docs/`
- Squash merge: enabled
- Delete head branches after merge: enabled

## Перед `git push`

```bash
git init
git add .
git commit -m "feat: initial public release v0.6.0"
git branch -M main
git remote add origin git@github.com:YOUR_USER/agentface32.git
git push -u origin main
```

Потом:

1. убедиться, что GitHub Actions `Build firmware profiles` зелёный;
2. добавить Topics и Description;
3. загрузить social preview 1280×640;
4. проверить README на мобильном GitHub;
5. создать tag/release `v0.6.0`;
6. открыть `/issues/new` и проверить шаблоны;
7. убедиться, что `include/config.local.h` не попал в git (`git status`, `git check-ignore include/config.local.h`).

## SEO-формулировки, которые уже естественно есть в README

Не нужно набивать keywords искусственно. В тексте уже присутствуют запросы, по которым проект логично искать:

- ESP32 Claude Code display
- ESP32 Codex status display
- Claude Code hardware companion
- Codex ESP32 hooks
- AI coding agent desk display
- ESP32 OLED AI agent status
- M5Stack Claude Code
- multi-agent ESP32 dashboard

## Что добавить после публикации

Самый сильный следующий SEO/конверсионный элемент — короткий GIF с реальным устройством. Инструкция лежит в `docs/MEDIA.md`.
