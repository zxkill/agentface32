#pragma once
#include <Arduino.h>
#include "agent.h"

enum class FaceViewMode : uint8_t {
  Claude,
  Codex,
  Both,
  Auto
};

void faceBegin();
void faceSetNetwork(const String& ip);
void faceSetVoiceEnabled(bool enabled);
void faceLoop();

// Btn C: short press cycles Claude -> Codex -> Both. Long press toggles Auto.
void faceCycleViewMode();
void faceToggleAutoMode();
FaceViewMode faceGetViewMode();
const char* faceViewModeName();
AgentId faceFocusedAgent();
bool faceSetViewModeByName(const String& mode);
