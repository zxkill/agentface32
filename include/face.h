#pragma once
#include <Arduino.h>
#include "agent.h"

enum class FaceViewMode : uint8_t {
  Agent,  // one provider face (faceDisplayedAgent)
  All,    // cards for every enabled provider
  Auto    // face of the most recently active enabled provider
};

// Full-screen list used by the settings menu, Wi-Fi setup and OTA progress.
static constexpr uint8_t FACE_PANEL_MAX_LINES = 16;
struct FacePanelLine {
  String label;
  String value;
};
struct FacePanel {
  String title;
  FacePanelLine lines[FACE_PANEL_MAX_LINES];
  uint8_t count = 0;
  int8_t selected = -1;   // highlighted row, -1 for informational panels
  int8_t progress = -1;   // 0..100 draws a progress bar
  String footer;
};

// --- Implemented by the selected display backend (face_*.cpp) ---
void faceBegin();
void faceSetNetwork(const String& ip);
void faceSetVoiceEnabled(bool enabled);
void faceSetBrightness(uint8_t percent);
// Shows a panel instead of the face until called with nullptr. The caller owns
// the panel and may update it in place.
void faceShowPanel(const FacePanel* panel);
void faceLoop();
// Draw immediately, bypassing the frame limiter (used inside blocking work
// such as OTA uploads).
void faceRenderNow();

// --- Shared view logic (view.cpp) ---
// Btn C: short press cycles enabled providers -> All. Long press toggles Auto.
void faceCycleViewMode();
void faceToggleAutoMode();
FaceViewMode faceGetViewMode();
const char* faceViewModeName();
bool faceSetViewModeByName(const String& mode);
// Re-validate the view after providers are enabled/disabled.
void faceViewSanitize();
bool faceViewShowsAll();
AgentId faceDisplayedAgent();
AgentId faceFocusedAgent();
uint8_t faceVisibleAgents(AgentId* out);  // enabled providers, out[AGENT_COUNT]
// Another enabled provider that recently reached an important state while the
// user is looking at `shown`.
bool faceBackgroundAlert(AgentId shown, uint32_t now, uint32_t windowMs, AgentId* out);
bool faceImportantState(AgentState state);
