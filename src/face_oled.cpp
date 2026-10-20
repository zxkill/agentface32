#include "hardware_profile.h"
#if defined(DISPLAY_BACKEND_U8G2)

#include <Arduino.h>
#include <math.h>
#include <U8g2lib.h>
#include <Wire.h>
#include "face.h"
#include "agent.h"
#include "config.h"
#include "translations.h"

#if defined(DISPLAY_DRIVER_SH1106)
static U8G2_SH1106_128X64_NONAME_F_HW_I2C g_oled(U8G2_R0, U8X8_PIN_NONE, OLED_SCL_PIN, OLED_SDA_PIN);
#else
static U8G2_SSD1306_128X64_NONAME_F_HW_I2C g_oled(U8G2_R0, U8X8_PIN_NONE, OLED_SCL_PIN, OLED_SDA_PIN);
#endif

static String g_ip;
static bool g_voice = true;
static uint32_t g_lastFrame = 0;
static const FacePanel* g_panel = nullptr;

static String taskClock(uint32_t ms) {
  uint32_t total = ms / 1000;
  char buf[12];
  if (total >= 3600) {
    snprintf(buf, sizeof(buf), "%lu:%02lu:%02lu", (unsigned long)(total / 3600),
             (unsigned long)((total / 60) % 60), (unsigned long)(total % 60));
  } else {
    snprintf(buf, sizeof(buf), "%lu:%02lu", (unsigned long)(total / 60),
             (unsigned long)(total % 60));
  }
  return String(buf);
}

static void centered(const String& text, int y) {
  int w = g_oled.getUTF8Width(text.c_str());
  g_oled.drawUTF8(max(0, (128 - w) / 2), y, text.c_str());
}

static String fitText(String text, int maxPx) {
  text.replace("\r", " "); text.replace("\n", " "); text.replace("\t", " ");
  while (text.length() && g_oled.getUTF8Width(text.c_str()) > maxPx) {
    // Remove one UTF-8 codepoint from the end.
    int cut = text.length() - 1;
    while (cut > 0 && (((uint8_t)text[cut] & 0xC0) == 0x80)) --cut;
    text.remove(cut);
  }
  return text;
}

static void drawEyes(AgentState state, uint32_t now) {
  int lx = 31, rx = 83, y = 17;
  int gaze = 0;
  bool blink = (now % 4300) > 4180;
  if (state == AgentState::Reading) gaze = (int)(4.0f * sinf(now / 360.0f));
  if (state == AgentState::Thinking) gaze = 4;
  if (state == AgentState::Typing || state == AgentState::Running) gaze = (int)(2.0f * sinf(now / 160.0f));

  if (state == AgentState::Sleep || blink) {
    g_oled.drawLine(lx, y + 5, lx + 14, y + 5);
    g_oled.drawLine(rx, y + 5, rx + 14, y + 5);
    return;
  }

  if (state == AgentState::Done) {
    // Happy closed eyes, deliberately drawn with line segments so this works
    // across U8g2 versions without relying on arc API differences.
    g_oled.drawLine(lx, y + 5, lx + 7, y + 9);
    g_oled.drawLine(lx + 7, y + 9, lx + 14, y + 5);
    g_oled.drawLine(rx, y + 5, rx + 7, y + 9);
    g_oled.drawLine(rx + 7, y + 9, rx + 14, y + 5);
    return;
  }

  g_oled.drawRFrame(lx, y, 16, 12, 3);
  g_oled.drawRFrame(rx, y, 16, 12, 3);
  if (state == AgentState::Error) {
    g_oled.drawLine(lx + 3, y + 3, lx + 12, y + 9);
    g_oled.drawLine(lx + 12, y + 3, lx + 3, y + 9);
    g_oled.drawLine(rx + 3, y + 3, rx + 12, y + 9);
    g_oled.drawLine(rx + 12, y + 3, rx + 3, y + 9);
  } else {
    int gy = state == AgentState::Thinking ? y + 3 : (state == AgentState::Typing || state == AgentState::Running ? y + 8 : y + 6);
    g_oled.drawDisc(lx + 8 + gaze, gy, state == AgentState::Attention ? 3 : 2);
    g_oled.drawDisc(rx + 8 + gaze, gy, state == AgentState::Attention ? 3 : 2);
  }

  // Brows make the tiny face much easier to read at a glance.
  if (state == AgentState::Error || state == AgentState::Abort) {
    g_oled.drawLine(lx + 1, y - 4, lx + 13, y - 1);
    g_oled.drawLine(rx + 2, y - 1, rx + 14, y - 4);
  } else if (state == AgentState::Attention) {
    g_oled.drawLine(lx + 1, y - 4, lx + 14, y - 4);
    g_oled.drawLine(rx + 1, y - 4, rx + 14, y - 4);
  }
}

static void drawMouth(AgentState state, uint32_t now) {
  int y = 34;
  if (state == AgentState::Done || state == AgentState::Welcome) {
    g_oled.drawLine(55, y - 3, 60, y + 1);
    g_oled.drawLine(60, y + 1, 68, y + 1);
    g_oled.drawLine(68, y + 1, 73, y - 3);
  } else if (state == AgentState::Error) {
    g_oled.drawLine(55, y + 4, 60, y);
    g_oled.drawLine(60, y, 68, y);
    g_oled.drawLine(68, y, 73, y + 4);
  } else if (state == AgentState::Thinking) {
    int phase = (now / 350) % 3;
    for (int i = 0; i < 3; ++i) g_oled.drawDisc(55 + i * 9, y, i == phase ? 2 : 1);
  } else if (state == AgentState::Running) {
    g_oled.drawBox(54, y - 2, 20, 5);
  } else {
    g_oled.drawLine(55, y, 73, y);
  }
}

static void renderAgent(AgentId agent, uint32_t now) {
  const AgentState state = agentGetState(agent);
  g_oled.setFont(u8g2_font_6x12_t_cyrillic);
  g_oled.drawUTF8(0, 10, agentName(agent));
  if (agentTaskActive(agent)) {
    String t = taskClock(agentTaskElapsedMs(agent));
    g_oled.drawUTF8(127 - g_oled.getUTF8Width(t.c_str()), 10, t.c_str());
  } else if (faceGetViewMode() == FaceViewMode::Auto) {
    g_oled.drawStr(102, 10, "AUTO");
  }

  drawEyes(state, now);
  drawMouth(state, now);

  String title = stateTitle(state);
  centered(title, 48);

  String detail = fitText(agentGetDetail(agent), 124);
  if (detail.length()) centered(detail, 62);

  // If the hidden agent needs attention, invert a small corner marker.
  AgentId other;
  if (faceBackgroundAlert(agent, now, 4500, &other)) {
    g_oled.drawBox(112, 13, 15, 10);
    g_oled.setDrawColor(0);
    g_oled.drawStr(115, 22, "!");
    g_oled.setDrawColor(1);
  }
}

static void card(AgentId agent, int top, int height) {
  g_oled.setFont(u8g2_font_6x12_t_cyrillic);
  String name = agentName(agent);
  String state = agentHasSeen(agent) ? String(stateTitle(agentGetState(agent))) : String("-");
  int sw = g_oled.getUTF8Width(state.c_str());

  if (height < 20) {
    // Three or more providers: one line per provider.
    g_oled.drawUTF8(2, top + 10, name.c_str());
    g_oled.drawUTF8(max(50, 126 - sw), top + 10, state.c_str());
    return;
  }

  g_oled.drawRFrame(0, top, 128, height - 1, 3);
  g_oled.drawUTF8(4, top + 10, name.c_str());
  g_oled.drawUTF8(max(50, 124 - sw), top + 10, state.c_str());

  String info;
  if (agentTaskActive(agent)) {
    info = taskClock(agentTaskElapsedMs(agent)) + " / " + String(agentToolCount(agent));
  } else if (agentLastTaskDurationMs(agent)) {
    info = taskClock(agentLastTaskDurationMs(agent)) + " / " + String(agentLastToolCount(agent));
  } else {
    info = agentHasSeen(agent) ? tr("ready", "готов") : tr("no data", "нет данных");
  }
  info = fitText(info, 118);
  g_oled.drawUTF8(4, top + 23, info.c_str());
}

static void renderAll() {
  g_oled.setFont(u8g2_font_6x12_t_cyrillic);
  centered(tr("AGENTS", "АГЕНТЫ"), 10);
  AgentId agents[AGENT_COUNT];
  const uint8_t n = faceVisibleAgents(agents);
  const int height = 52 / max(1, (int)n);
  for (uint8_t i = 0; i < n; ++i) card(agents[i], 12 + i * height, height);
}

static void renderPanel(const FacePanel& p) {
  // 5x8 font fits ~25 characters per row, enough for "Wi-Fi AgentFace32-AB12".
  g_oled.setFont(u8g2_font_5x8_t_cyrillic);
  g_oled.drawUTF8(0, 7, fitText(p.title, 128).c_str());
  g_oled.drawHLine(0, 9, 128);

  constexpr int rowH = 9;
  constexpr int top = 11;
  const int bottom = p.progress >= 0 ? 52 : 64;
  const int visible = max(1, (bottom - top) / rowH);
  int first = 0;
  if (p.selected >= visible) first = p.selected - visible + 1;

  for (int i = first; i < p.count && i - first < visible; ++i) {
    const int y = top + (i - first) * rowH;
    const bool sel = i == p.selected;
    if (sel) {
      g_oled.drawBox(0, y, 128, rowH);
      g_oled.setDrawColor(0);
    }
    String value = fitText(p.lines[i].value, 122);
    const int vw = g_oled.getUTF8Width(value.c_str());
    const int labelSpace = 124 - vw - (vw ? 4 : 0);
    if (labelSpace >= 15) g_oled.drawUTF8(2, y + 7, fitText(p.lines[i].label, labelSpace).c_str());
    if (vw) g_oled.drawUTF8(126 - vw, y + 7, value.c_str());
    g_oled.setDrawColor(1);
  }

  if (p.progress >= 0) {
    g_oled.drawFrame(0, 54, 128, 9);
    g_oled.drawBox(2, 56, (124 * min((int)p.progress, 100)) / 100, 5);
  }
}

void faceBegin() {
  Wire.begin(OLED_SDA_PIN, OLED_SCL_PIN);
  g_oled.setI2CAddress(OLED_I2C_ADDRESS << 1);
  g_oled.begin();
  g_oled.setFontMode(1);
  g_oled.setFontDirection(0);
  g_oled.clearBuffer();
  g_oled.setFont(u8g2_font_6x12_t_cyrillic);
  centered("AgentFace32", 28);
  centered(HARDWARE_PROFILE_NAME, 46);
  g_oled.sendBuffer();
}

void faceSetNetwork(const String& ip) { g_ip = ip; (void)g_ip; }
void faceSetVoiceEnabled(bool enabled) { g_voice = enabled; (void)g_voice; }

// OLED panels have no backlight; contrast is the closest equivalent.
void faceSetBrightness(uint8_t percent) {
  g_oled.setContrast((uint8_t)((constrain((int)percent, 1, 100) * 255) / 100));
}

void faceShowPanel(const FacePanel* panel) {
  if (panel != g_panel) g_lastFrame = 0;
  g_panel = panel;
}

void faceRenderNow() {
  const uint32_t now = millis();
  g_lastFrame = now;
  g_oled.clearBuffer();
  if (g_panel) renderPanel(*g_panel);
  else if (faceViewShowsAll()) renderAll();
  else renderAgent(faceDisplayedAgent(), now);
  g_oled.sendBuffer();
}

void faceLoop() {
  if (g_lastFrame && millis() - g_lastFrame < 100) return; // 10 FPS is enough on I2C OLED.
  faceRenderNow();
}

#endif // DISPLAY_BACKEND_U8G2
