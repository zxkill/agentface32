#include "hardware_profile.h"
#if defined(PROFILE_M5STACK_BASIC)
#include <M5Unified.h>
#include <M5GFX.h>
#include <math.h>
#include "face.h"
#include "locale.h"
#include "agent.h"
#include "ui_text.h"

static String g_ip;
static bool g_voice = true;
static uint32_t g_lastFrame = 0;
static FaceViewMode g_viewMode = FaceViewMode::Claude;
static FaceViewMode g_lastManualMode = FaceViewMode::Claude;

static M5Canvas g_canvas(&M5.Display);
static bool g_canvasReady = false;

static constexpr uint16_t C_BG       = 0x0841;
static constexpr uint16_t C_PANEL    = 0x1082;
static constexpr uint16_t C_PANEL2   = 0x18C3;
static constexpr uint16_t C_WHITE    = 0xFFFF;
static constexpr uint16_t C_TEXT     = 0xD69A;
static constexpr uint16_t C_MUTED    = 0x7BEF;
static constexpr uint16_t C_CYAN     = 0x5DFF;
static constexpr uint16_t C_BLUE     = 0x5C9F;
static constexpr uint16_t C_GREEN    = 0x67EC;
static constexpr uint16_t C_YELLOW   = 0xFFE0;
static constexpr uint16_t C_ORANGE   = 0xFD20;
static constexpr uint16_t C_RED      = 0xF986;

static uint16_t accentForState(AgentState state) {
  switch (state) {
    case AgentState::Reading: return C_CYAN;
    case AgentState::Thinking: return C_BLUE;
    case AgentState::Typing: return C_CYAN;
    case AgentState::Running: return C_BLUE;
    case AgentState::Attention: return C_YELLOW;
    case AgentState::Done: return C_GREEN;
    case AgentState::Error: return C_RED;
    case AgentState::Abort: return C_ORANGE;
    case AgentState::Welcome: return C_GREEN;
    case AgentState::Sleep: return C_MUTED;
    default: return C_WHITE;
  }
}

static uint16_t brandColor(AgentId agent) {
  return agent == AgentId::Codex ? C_GREEN : C_WHITE;
}

static String taskClock(uint32_t ms) {
  uint32_t total = ms / 1000;
  uint32_t hours = total / 3600;
  uint32_t minutes = (total / 60) % 60;
  uint32_t seconds = total % 60;
  char buf[16];
  if (hours > 0) {
    snprintf(buf, sizeof(buf), "%lu:%02lu:%02lu",
             (unsigned long)hours, (unsigned long)minutes, (unsigned long)seconds);
  } else {
    snprintf(buf, sizeof(buf), "%lu:%02lu",
             (unsigned long)(total / 60), (unsigned long)seconds);
  }
  return String(buf);
}

static void thickLine(int x1, int y1, int x2, int y2, uint16_t color, int thickness = 3) {
  const int r = thickness / 2;
  for (int d = -r; d <= r; ++d) g_canvas.drawLine(x1, y1 + d, x2, y2 + d, color);
}

static void sparkle(int x, int y, int s, uint16_t color) {
  g_canvas.drawLine(x - s, y, x + s, y, color);
  g_canvas.drawLine(x, y - s, x, y + s, color);
  if (s >= 4) {
    g_canvas.drawLine(x - s / 2, y - s / 2, x + s / 2, y + s / 2, color);
    g_canvas.drawLine(x + s / 2, y - s / 2, x - s / 2, y + s / 2, color);
  }
}

static void drawEyeNormal(int cx, int cy, int gazeX, int gazeY, int openness,
                          uint16_t irisColor, bool pupilLarge = false) {
  openness = constrain(openness, 5, 100);
  const int h = 10 + (34 * openness) / 100;
  const int w = 76;
  const int top = cy - h / 2;
  g_canvas.fillRoundRect(cx - w / 2, top, w, h, min(18, h / 2), C_WHITE);

  if (h > 13) {
    const int maxY = max(0, h / 2 - 10);
    const int px = cx + constrain(gazeX, -19, 19);
    const int py = cy + constrain(gazeY, -maxY, maxY);
    const int pr = pupilLarge ? 13 : 11;
    g_canvas.fillCircle(px, py, pr, C_BG);
    g_canvas.fillCircle(px + 3, py - 4, 3, irisColor);
    g_canvas.fillCircle(px - 3, py - 5, 2, C_WHITE);
  }
}

static void drawHappyEye(int cx, int cy, uint16_t color) {
  thickLine(cx - 29, cy + 8, cx - 11, cy - 5, color, 4);
  thickLine(cx - 11, cy - 5, cx + 4, cy + 6, color, 4);
  thickLine(cx + 4, cy + 6, cx + 24, cy - 3, color, 4);
}

static void drawClosedEye(int cx, int cy, uint16_t color) {
  thickLine(cx - 27, cy, cx - 9, cy + 5, color, 3);
  thickLine(cx - 9, cy + 5, cx + 9, cy + 5, color, 3);
  thickLine(cx + 9, cy + 5, cx + 27, cy, color, 3);
}

static void drawBrows(int shiftX, int leftTilt, int rightTilt, int lift, uint16_t color) {
  const int y = 49 - lift;
  thickLine(63 + shiftX, y + leftTilt, 117 + shiftX, y - leftTilt, color, 4);
  thickLine(203 + shiftX, y - rightTilt, 257 + shiftX, y + rightTilt, color, 4);
}

static void drawMouth(AgentState state, uint32_t now, int shiftX) {
  const int x = 160 + shiftX;
  const int y = 139;
  const uint16_t a = accentForState(state);
  switch (state) {
    case AgentState::Welcome:
    case AgentState::Done:
      g_canvas.drawArc(x, y - 12, 31, 24, 25, 155, a);
      g_canvas.drawArc(x, y - 11, 32, 25, 25, 155, a);
      break;
    case AgentState::Attention:
      g_canvas.drawCircle(x, y - 2, 9, a);
      g_canvas.drawCircle(x, y - 2, 10, a);
      break;
    case AgentState::Error:
      g_canvas.drawArc(x, y + 16, 29, 20, 205, 335, a);
      g_canvas.drawArc(x, y + 17, 30, 21, 205, 335, a);
      break;
    case AgentState::Abort:
      thickLine(x - 23, y - 2, x + 22, y + 4, a, 3);
      break;
    case AgentState::Typing: {
      const int phase = (now / 120) % 3;
      for (int i = 0; i < 3; ++i) {
        int h = (i == phase) ? 9 : 5;
        g_canvas.fillRoundRect(x - 18 + i * 14, y - h / 2, 7, h, 3, a);
      }
      break;
    }
    case AgentState::Running: {
      const int w = 14 + ((now / 120) % 4) * 8;
      g_canvas.drawRoundRect(x - 27, y - 8, 54, 16, 4, a);
      g_canvas.fillRoundRect(x - 22, y - 3, min(w, 44), 6, 3, a);
      break;
    }
    case AgentState::Thinking:
      g_canvas.fillCircle(x - 9, y, 3, a);
      g_canvas.fillCircle(x, y, 3, a);
      g_canvas.fillCircle(x + 9, y, 3, a);
      break;
    case AgentState::Reading:
      thickLine(x - 18, y, x + 18, y, C_TEXT, 2);
      break;
    case AgentState::Sleep:
      thickLine(x - 12, y, x + 12, y, C_MUTED, 2);
      break;
    default:
      g_canvas.drawArc(x, y - 6, 18, 12, 35, 145, C_TEXT);
      break;
  }
}

static void drawDecorations(AgentState state, uint32_t now, int shiftX) {
  const uint16_t a = accentForState(state);
  switch (state) {
    case AgentState::Done: {
      const int pulse = 3 + ((now / 180) % 3);
      sparkle(35 + shiftX, 76, pulse, a);
      sparkle(286 + shiftX, 61, max(2, 7 - pulse), C_YELLOW);
      sparkle(277 + shiftX, 127, 3, a);
      break;
    }
    case AgentState::Welcome:
      sparkle(39 + shiftX, 73, 4, a);
      sparkle(281 + shiftX, 70, 4, C_CYAN);
      break;
    case AgentState::Thinking: {
      const int p = (now / 420) % 3;
      for (int i = 0; i < 3; ++i) {
        const int r = 3 + ((i == p) ? 2 : 0);
        g_canvas.fillCircle(260 + i * 13 + shiftX, 38 - i * 6, r, a);
      }
      break;
    }
    case AgentState::Attention: {
      const int pulse = 7 + (int)(3.0f * sinf(now / 180.0f));
      thickLine(287 + shiftX, 58, 287 + shiftX, 85, a, 5);
      g_canvas.fillCircle(287 + shiftX, 96, 3, a);
      g_canvas.drawCircle(287 + shiftX, 78, pulse + 18, a);
      break;
    }
    case AgentState::Error:
      thickLine(33 + shiftX, 65, 43 + shiftX, 55, a, 3);
      thickLine(29 + shiftX, 79, 42 + shiftX, 76, a, 3);
      thickLine(277 + shiftX, 55, 287 + shiftX, 65, a, 3);
      break;
    case AgentState::Sleep:
      g_canvas.fillCircle(275, 57, 13, C_BLUE);
      g_canvas.fillCircle(281, 52, 13, C_PANEL);
      sparkle(296, 83, 3, C_MUTED);
      break;
    case AgentState::Reading:
      g_canvas.drawLine(29, 87, 39, 87, a);
      g_canvas.drawLine(281, 87, 291, 87, a);
      break;
    default:
      break;
  }
}

static bool importantState(AgentState state) {
  return state == AgentState::Attention || state == AgentState::Error ||
         state == AgentState::Abort || state == AgentState::Done;
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

static void drawBackgroundToast(AgentId shown, uint32_t now) {
  if (g_viewMode == FaceViewMode::Both || g_viewMode == FaceViewMode::Auto) return;
  AgentId other = shown == AgentId::Claude ? AgentId::Codex : AgentId::Claude;
  if (!agentHasSeen(other)) return;
  AgentState otherState = agentGetState(other);
  if (!importantState(otherState)) return;
  if (now - agentStateSince(other) > 4200) return;

  const uint16_t a = accentForState(otherState);
  String line = String(agentName(other)) + ": " + stateTitle(otherState);
  if (otherState == AgentState::Done && agentLastTaskDurationMs(other)) {
    line += " · " + taskClock(agentLastTaskDurationMs(other));
  }
  g_canvas.fillRoundRect(12, 207, 296, 28, 8, C_PANEL2);
  g_canvas.drawRoundRect(12, 207, 296, 28, 8, a);
  uiDrawTextCentered(g_canvas, UI_FONT_SMALL, line, 160, 213, a, C_PANEL2, 282);
}

static void renderAgentFace(AgentId agent, uint32_t now) {
  const AgentState state = agentGetState(agent);
  const String detail = agentGetDetail(agent);
  const uint32_t age = now - agentStateSince(agent);
  const uint16_t accent = accentForState(state);

  int shiftX = 0;
  if (state == AgentState::Error && age < 1200) {
    shiftX = (int)(3.0f * sinf(now / 35.0f));
  }

  g_canvas.fillSprite(C_BG);
  g_canvas.fillRoundRect(7, 7, 306, 149, 18, C_PANEL);
  g_canvas.drawRoundRect(7, 7, 306, 149, 18, C_PANEL2);

  uiDrawText(g_canvas, UI_FONT_HEADER, agentName(agent), 20, 12,
             brandColor(agent), C_PANEL, 120);

  const bool connected = g_ip.length() && g_ip != "NO WIFI";
  g_canvas.fillCircle(300, 22, 4, connected ? C_GREEN : C_RED);

  String topRight;
  if (g_viewMode == FaceViewMode::Auto) topRight = "AUTO";
  if (agentTaskActive(agent)) {
    if (topRight.length()) topRight += "  ";
    topRight += taskClock(agentTaskElapsedMs(agent));
  }
  if (topRight.length()) {
    const int w = uiTextWidth(UI_FONT_SMALL, topRight);
    uiDrawText(g_canvas, UI_FONT_SMALL, topRight, 288 - w, 16,
               g_viewMode == FaceViewMode::Auto ? C_YELLOW : C_CYAN,
               C_PANEL, w);
  }

  int gazeX = 0;
  int gazeY = 0;
  int openL = 100;
  int openR = 100;
  int browLTilt = 0;
  int browRTilt = 0;
  int browLift = agent == AgentId::Codex ? 1 : 0;
  bool happyEyes = false;
  bool closedEyes = false;

  const uint32_t blinkPhase = now % (agent == AgentId::Codex ? 5100 : 4700);
  if (blinkPhase > 4480 && blinkPhase < 4620 &&
      state != AgentState::Attention && state != AgentState::Done &&
      state != AgentState::Sleep) {
    const int d = abs((int)blinkPhase - 4550);
    openL = openR = constrain(8 + d * 2, 8, 100);
  }

  switch (state) {
    case AgentState::Idle:
      gazeX = (int)(5.0f * sinf(now / 1200.0f));
      gazeY = (int)(2.0f * sinf(now / 1800.0f));
      browLift += 1;
      break;
    case AgentState::Welcome:
      gazeY = -2;
      browLift += 6;
      if (age > 550 && age < 850) openR = 10;
      break;
    case AgentState::Reading:
      gazeX = (int)(19.0f * sinf(now / 390.0f));
      gazeY = (int)(4.0f * sinf(now / 760.0f));
      browLift += 1;
      break;
    case AgentState::Thinking:
      gazeX = 10 + (int)(5.0f * sinf(now / 930.0f));
      gazeY = -9;
      browLTilt = -4;
      browRTilt = 4;
      browLift += 2;
      break;
    case AgentState::Typing:
      gazeX = (int)(6.0f * sinf(now / 170.0f));
      gazeY = 7;
      browLTilt = 4;
      browRTilt = 4;
      break;
    case AgentState::Running:
      gazeX = (int)(4.0f * sinf(now / 240.0f));
      gazeY = 8;
      browLTilt = 5;
      browRTilt = 5;
      break;
    case AgentState::Attention:
      openL = openR = 100;
      browLift += 11;
      break;
    case AgentState::Done:
      happyEyes = true;
      browLift += 5;
      break;
    case AgentState::Error:
      openL = openR = min(openL, 55);
      gazeY = 6;
      browLTilt = 8;
      browRTilt = 8;
      break;
    case AgentState::Abort:
      openL = min(openL, 42);
      openR = min(openR, 72);
      gazeX = 13;
      browLTilt = -6;
      browRTilt = 7;
      break;
    case AgentState::Sleep: {
      const int closing = age < 900 ? max(0, 100 - (int)(age / 9)) : 0;
      if (closing < 18) closedEyes = true;
      else openL = openR = closing;
      browLift -= 2;
      break;
    }
  }

  // Codex gets a subtly more angular expression without sacrificing the
  // state-specific emotion.
  if (agent == AgentId::Codex && state != AgentState::Done && state != AgentState::Sleep) {
    browLTilt += 1;
    browRTilt += 1;
  }

  drawBrows(shiftX, browLTilt, browRTilt, browLift,
            state == AgentState::Error ? C_RED : C_TEXT);

  if (happyEyes) {
    drawHappyEye(91 + shiftX, 91, accent);
    drawHappyEye(229 + shiftX, 91, accent);
  } else if (closedEyes) {
    drawClosedEye(91 + shiftX, 91, C_MUTED);
    drawClosedEye(229 + shiftX, 91, C_MUTED);
  } else {
    drawEyeNormal(91 + shiftX, 91, gazeX, gazeY, openL, accent,
                  state == AgentState::Attention);
    drawEyeNormal(229 + shiftX, 91, gazeX, gazeY, openR, accent,
                  state == AgentState::Attention);
  }

  drawMouth(state, now, shiftX);
  drawDecorations(state, now, shiftX);

  uiDrawTextCentered(g_canvas, UI_FONT_BIG, stateTitle(state), 160, 162,
                     accent, C_BG, 300);
  if (detail.length()) {
    uiDrawTextCentered(g_canvas, UI_FONT_SMALL, detail, 160, 197,
                       C_TEXT, C_BG, 296);
  }

  // Important event from the background agent temporarily replaces the footer.
  AgentId other = agent == AgentId::Claude ? AgentId::Codex : AgentId::Claude;
  const bool toast = g_viewMode != FaceViewMode::Auto && agentHasSeen(other) &&
                     importantState(agentGetState(other)) &&
                     now - agentStateSince(other) <= 4200;
  if (toast) {
    drawBackgroundToast(agent, now);
    return;
  }

  if (g_ip.length() && g_ip != "NO WIFI") {
    uiDrawText(g_canvas, UI_FONT_SMALL, g_ip, 13, 220, C_MUTED, C_BG, 150);
  } else {
    uiDrawText(g_canvas, UI_FONT_SMALL, tr("NO NETWORK", "НЕТ СЕТИ"), 13, 220, C_RED, C_BG, 150);
  }

  String rightLabel;
  uint16_t rightColor = C_MUTED;
  if (agentTaskActive(agent)) {
    rightLabel = String(tr("STEPS: ", "ШАГОВ: ")) + String(agentToolCount(agent));
    rightColor = C_CYAN;
  } else {
    rightLabel = g_voice ? tr("SOUND", "ЗВУК") : tr("MUTED", "БЕЗ ЗВУКА");
    rightColor = g_voice ? C_GREEN : C_MUTED;
  }
  const int rightWidth = uiTextWidth(UI_FONT_SMALL, rightLabel);
  uiDrawText(g_canvas, UI_FONT_SMALL, rightLabel, 307 - rightWidth, 220,
             rightColor, C_BG, rightWidth);
}

static void renderAgentCard(AgentId agent, int y, uint32_t now) {
  const bool seen = agentHasSeen(agent);
  const AgentState state = agentGetState(agent);
  const uint16_t a = seen ? accentForState(state) : C_MUTED;

  g_canvas.fillRoundRect(10, y, 300, 82, 13, C_PANEL);
  g_canvas.drawRoundRect(10, y, 300, 82, 13, seen ? C_PANEL2 : C_MUTED);
  g_canvas.fillRoundRect(10, y, 5, 82, 3, seen ? brandColor(agent) : C_MUTED);

  uiDrawText(g_canvas, UI_FONT_HEADER, agentName(agent), 23, y + 8,
             brandColor(agent), C_PANEL, 100);

  String status = seen ? String(stateTitle(state)) : String(tr("NO DATA", "НЕТ ДАННЫХ"));
  const int sw = uiTextWidth(UI_FONT_SMALL, status);
  uiDrawText(g_canvas, UI_FONT_SMALL, status, 294 - sw, y + 13, a, C_PANEL, sw);

  String detail = seen ? agentGetDetail(agent) : tr("Waiting for first hook", "Жду первый hook");
  uiDrawText(g_canvas, UI_FONT_SMALL, detail, 23, y + 38, C_TEXT, C_PANEL, 270);

  String stats;
  if (agentTaskActive(agent)) {
    stats = taskClock(agentTaskElapsedMs(agent)) + tr("  ·  steps: ", "  ·  шагов: ") + String(agentToolCount(agent));
  } else if (agentLastTaskDurationMs(agent)) {
    stats = String(tr("last: ", "последняя: ")) + taskClock(agentLastTaskDurationMs(agent)) +
            tr("  ·  steps: ", "  ·  шагов: ") + String(agentLastToolCount(agent));
  } else {
    stats = seen ? tr("ready", "готов к работе") : tr("not connected", "не подключён");
  }
  uiDrawText(g_canvas, UI_FONT_SMALL, stats, 23, y + 59,
             agentTaskActive(agent) ? C_CYAN : C_MUTED, C_PANEL, 270);
}

static void renderBoth(uint32_t now) {
  g_canvas.fillSprite(C_BG);
  uiDrawText(g_canvas, UI_FONT_HEADER, tr("AGENTS", "АГЕНТЫ"), 14, 8, C_WHITE, C_BG, 130);
  const bool connected = g_ip.length() && g_ip != "NO WIFI";
  g_canvas.fillCircle(303, 18, 4, connected ? C_GREEN : C_RED);

  renderAgentCard(AgentId::Claude, 35, now);
  renderAgentCard(AgentId::Codex, 127, now);

  String footer = g_voice ? tr("A: sound   C: view", "A: звук   C: режим") : tr("A: muted   C: view", "A: без звука   C: режим");
  uiDrawTextCentered(g_canvas, UI_FONT_SMALL, footer, 160, 218, C_MUTED, C_BG, 300);
}

void faceBegin() {
  M5.Display.setRotation(1);
  M5.Display.fillScreen(C_BG);
  g_canvas.setColorDepth(8);
  g_canvasReady = g_canvas.createSprite(M5.Display.width(), M5.Display.height()) != nullptr;

  if (!g_canvasReady) {
    M5.Display.setTextDatum(middle_center);
    M5.Display.setTextFont(2);
    M5.Display.setTextColor(C_RED, C_BG);
    M5.Display.drawString("DISPLAY BUFFER ERROR", M5.Display.width() / 2, M5.Display.height() / 2);
  }
}

void faceSetNetwork(const String& ip) {
  g_ip = ip;
  g_lastFrame = 0;
}

void faceSetVoiceEnabled(bool enabled) {
  g_voice = enabled;
  g_lastFrame = 0;
}

void faceCycleViewMode() {
  if (g_viewMode == FaceViewMode::Auto) {
    g_viewMode = g_lastManualMode;
  } else if (g_viewMode == FaceViewMode::Claude) {
    g_viewMode = FaceViewMode::Codex;
    g_lastManualMode = g_viewMode;
  } else if (g_viewMode == FaceViewMode::Codex) {
    g_viewMode = FaceViewMode::Both;
    g_lastManualMode = g_viewMode;
  } else {
    g_viewMode = FaceViewMode::Claude;
    g_lastManualMode = g_viewMode;
  }
  g_lastFrame = 0;
}

void faceToggleAutoMode() {
  if (g_viewMode == FaceViewMode::Auto) {
    g_viewMode = g_lastManualMode;
  } else {
    g_lastManualMode = g_viewMode;
    g_viewMode = FaceViewMode::Auto;
  }
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

bool faceSetViewModeByName(const String& modeRaw) {
  String mode(modeRaw);
  mode.toLowerCase();
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
  if (!g_canvasReady) return;
  const uint32_t now = millis();
  if (g_lastFrame != 0 && now - g_lastFrame < 66) return;
  g_lastFrame = now;

  if (g_viewMode == FaceViewMode::Both) renderBoth(now);
  else renderAgentFace(displayedAgent(), now);

  g_canvas.pushSprite(0, 0);
}

#endif // PROFILE_M5STACK_BASIC
