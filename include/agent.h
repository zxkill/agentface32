#pragma once
#include <Arduino.h>
#include "state.h"

// One independent context per provider. Values are indexes into the provider
// table in agent.cpp and bit positions in the provider settings mask.
enum class AgentId : uint8_t {
  Claude = 0,
  Codex = 1,
  OpenCode = 2,
  Cline = 3,
  Cursor = 4
};
static constexpr uint8_t AGENT_COUNT = 5;

const char* agentName(AgentId agent);   // display name, e.g. "Claude"
const char* agentKey(AgentId agent);    // URL/settings key, e.g. "claude"
AgentId agentAt(uint8_t index);
bool agentFromKey(const String& key, AgentId* out);

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
