#pragma once
#include <Arduino.h>
#include "state.h"

enum class AgentId : uint8_t {
  Claude = 0,
  Codex = 1
};

const char* agentName(AgentId agent);

void agentBegin();
void agentSetState(AgentId agent, AgentState state, const String& detail = "", bool playAudio = true);

AgentState agentGetState(AgentId agent);
String agentGetDetail(AgentId agent);
bool agentTaskActive(AgentId agent);
uint32_t agentTaskElapsedMs(AgentId agent);
uint32_t agentLastTaskDurationMs(AgentId agent);
uint32_t agentToolCount(AgentId agent);
uint32_t agentLastToolCount(AgentId agent);
uint32_t agentStateSince(AgentId agent);
uint32_t agentLastEventAt(AgentId agent);
bool agentHasSeen(AgentId agent);
AgentId agentMostRecent();

// Parse one lifecycle hook payload and update only the selected agent context.
void agentHandleHookJson(AgentId agent, const String& body);
void agentLoop();
