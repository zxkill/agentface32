#include "hardware_profile.h"
#if defined(DISPLAY_BACKEND_U8G2)

#include <Arduino.h>
#include <math.h>
#include <U8g2lib.h>
#include <Wire.h>
#include "face.h"
#include "agent.h"
#include "config.h"
#include "locale.h"

#if defined(DISPLAY_DRIVER_SH1106)
static U8G2_SH1106_128X64_NONAME_F_HW_I2C g_oled(U8G2_R0, U8X8_PIN_NONE, OLED_SCL_PIN, OLED_SDA_PIN);
#else
static U8G2_SSD1306_128X64_NONAME_F_HW_I2C g_oled(U8G2_R0, U8X8_PIN_NONE, OLED_SCL_PIN, OLED_SDA_PIN);
#endif

static FaceViewMode g_viewMode = FaceViewMode::Claude;
static FaceViewMode g_lastManualMode = FaceViewMode::Claude;
static String g_ip;
static bool g_voice = true;
static uint32_t g_lastFrame = 0;

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

static AgentId displayedAgent() {
  if (g_viewMode == FaceViewMode::Codex) return AgentId::Codex;
  if (g_viewMode == FaceViewMode::Auto) return agentMostRecent();
  return AgentId::Claude;
}

AgentId faceFocusedAgent() {
  if (g_viewMode == FaceViewMode::Both) return agentMostRecent();
  return displayedAgent();
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

static bool importantState(AgentState s) {
  return s == AgentState::Attention || s == AgentState::Error || s == AgentState::Abort || s == AgentState::Done;
}

static void renderAgent(AgentId agent, uint32_t now) {
  const AgentState state = agentGetState(agent);
  g_oled.setFont(u8g2_font_6x12_t_cyrillic);
  g_oled.drawUTF8(0, 10, agentName(agent));
  if (agentTaskActive(agent)) {
    String t = taskClock(agentTaskElapsedMs(agent));
    g_oled.drawUTF8(127 - g_oled.getUTF8Width(t.c_str()), 10, t.c_str());
  } else if (g_viewMode == FaceViewMode::Auto) {
    g_oled.drawStr(102, 10, "AUTO");
  }

  drawEyes(state, now);
  drawMouth(state, now);

  String title = stateTitle(state);
  centered(title, 48);

  String detail = fitText(agentGetDetail(agent), 124);
  if (detail.length()) centered(detail, 62);

  // If the hidden agent needs attention, invert a small corner marker.
  AgentId other = agent == AgentId::Claude ? AgentId::Codex : AgentId::Claude;
  if (g_viewMode != FaceViewMode::Auto && agentHasSeen(other) && importantState(agentGetState(other)) &&
      now - agentStateSince(other) < 4500) {
    g_oled.drawBox(112, 13, 15, 10);
    g_oled.setDrawColor(0);
    g_oled.drawStr(115, 22, "!");
    g_oled.setDrawColor(1);
  }
}

static void card(AgentId agent, int top, uint32_t now) {
  g_oled.drawRFrame(0, top, 128, 27, 3);
  g_oled.setFont(u8g2_font_6x12_t_cyrillic);
  String name = agentName(agent);
  String state = agentHasSeen(agent) ? String(stateTitle(agentGetState(agent))) : String("-");
  g_oled.drawUTF8(4, top + 10, name.c_str());
  int sw = g_oled.getUTF8Width(state.c_str());
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

static void renderBoth(uint32_t now) {
  g_oled.setFont(u8g2_font_6x12_t_cyrillic);
  centered(tr("AGENTS", "АГЕНТЫ"), 10);
  card(AgentId::Claude, 12, now);
  card(AgentId::Codex, 40, now);
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

void faceCycleViewMode() {
  if (g_viewMode == FaceViewMode::Auto) g_viewMode = g_lastManualMode;
  else if (g_viewMode == FaceViewMode::Claude) g_viewMode = g_lastManualMode = FaceViewMode::Codex;
  else if (g_viewMode == FaceViewMode::Codex) g_viewMode = g_lastManualMode = FaceViewMode::Both;
  else g_viewMode = g_lastManualMode = FaceViewMode::Claude;
  g_lastFrame = 0;
}

void faceToggleAutoMode() {
  if (g_viewMode == FaceViewMode::Auto) g_viewMode = g_lastManualMode;
  else { g_lastManualMode = g_viewMode; g_viewMode = FaceViewMode::Auto; }
  g_lastFrame = 0;
}

FaceViewMode faceGetViewMode() { return g_viewMode; }
const char* faceViewModeName() {
  switch (g_viewMode) {
    case FaceViewMode::Claude: return "claude";
    case FaceViewMode::Codex: return "codex";
    case FaceViewMode::Both: return "both";
    case FaceViewMode::Auto: return "auto";
  }
  return "claude";
}

bool faceSetViewModeByName(const String& raw) {
  String mode(raw); mode.toLowerCase();
  if (mode == "claude") g_viewMode = FaceViewMode::Claude;
  else if (mode == "codex") g_viewMode = FaceViewMode::Codex;
  else if (mode == "both" || mode == "all") g_viewMode = FaceViewMode::Both;
  else if (mode == "auto") g_viewMode = FaceViewMode::Auto;
  else return false;
  if (g_viewMode != FaceViewMode::Auto) g_lastManualMode = g_viewMode;
  g_lastFrame = 0;
  return true;
}

void faceLoop() {
  uint32_t now = millis();
  if (g_lastFrame && now - g_lastFrame < 100) return; // 10 FPS is enough on I2C OLED.
  g_lastFrame = now;
  g_oled.clearBuffer();
  if (g_viewMode == FaceViewMode::Both) renderBoth(now);
  else renderAgent(displayedAgent(), now);
  g_oled.sendBuffer();
}

#endif // DISPLAY_BACKEND_U8G2
