#pragma once
#include <Arduino.h>

enum class AgentState : uint8_t {
  Idle,
  Welcome,
  Reading,
  Thinking,
  Typing,
  Running,
  Attention,
  Done,
  Error,
  Abort,
  Sleep
};

// Stable ASCII identifier used by the HTTP API.
const char* stateName(AgentState state);
// Human-facing localized state titles.
const char* stateTitleEn(AgentState state);
const char* stateTitleRu(AgentState state);
bool isOneShotState(AgentState state);
