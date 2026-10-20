// Backend-independent view selection shared by every face renderer.
#include "face.h"
#include "settings.h"

static FaceViewMode g_mode = FaceViewMode::Agent;
static FaceViewMode g_lastManualMode = FaceViewMode::Agent;
static AgentId g_agent = AgentId::Claude;

static AgentId firstEnabled() {
  for (uint8_t i = 0; i < AGENT_COUNT; ++i) {
    if (settingsProviderEnabled(agentAt(i))) return agentAt(i);
  }
  return AgentId::Claude;
}

void faceViewSanitize() {
  if (!settingsProviderEnabled(g_agent)) g_agent = firstEnabled();
  if (settingsEnabledProviderCount() < 2) {
    if (g_mode == FaceViewMode::All) g_mode = FaceViewMode::Agent;
    if (g_lastManualMode == FaceViewMode::All) g_lastManualMode = FaceViewMode::Agent;
  }
}

void faceCycleViewMode() {
  faceViewSanitize();
  if (g_mode == FaceViewMode::Auto) {
    g_mode = g_lastManualMode;
    return;
  }
  if (g_mode == FaceViewMode::All) {
    g_mode = FaceViewMode::Agent;
    g_agent = firstEnabled();
  } else {
    // Next enabled provider; after the last one comes the All view.
    bool advanced = false;
    for (uint8_t i = (uint8_t)g_agent + 1; i < AGENT_COUNT; ++i) {
      if (settingsProviderEnabled(agentAt(i))) {
        g_agent = agentAt(i);
        advanced = true;
        break;
      }
    }
    if (!advanced) {
      if (settingsEnabledProviderCount() >= 2) g_mode = FaceViewMode::All;
      else g_agent = firstEnabled();
    }
  }
  g_lastManualMode = g_mode;
}

void faceToggleAutoMode() {
  if (g_mode == FaceViewMode::Auto) {
    g_mode = g_lastManualMode;
  } else {
    g_lastManualMode = g_mode;
    g_mode = FaceViewMode::Auto;
  }
  faceViewSanitize();
}

FaceViewMode faceGetViewMode() { return g_mode; }

const char* faceViewModeName() {
  switch (g_mode) {
    case FaceViewMode::Agent: return agentKey(g_agent);
    case FaceViewMode::All: return "both";
    case FaceViewMode::Auto: return "auto";
  }
  return agentKey(g_agent);
}

bool faceSetViewModeByName(const String& raw) {
  String mode(raw);
  mode.toLowerCase();
  AgentId agent;
  if (mode == "both" || mode == "all") {
    if (settingsEnabledProviderCount() < 2) return false;
    g_mode = FaceViewMode::All;
  } else if (mode == "auto") {
    g_mode = FaceViewMode::Auto;
  } else if (agentFromKey(mode, &agent) && settingsProviderEnabled(agent)) {
    g_mode = FaceViewMode::Agent;
    g_agent = agent;
  } else {
    return false;
  }
  if (g_mode != FaceViewMode::Auto) g_lastManualMode = g_mode;
  return true;
}

bool faceViewShowsAll() {
  return g_mode == FaceViewMode::All && settingsEnabledProviderCount() >= 2;
}

static AgentId mostRecentEnabled() {
  const AgentId recent = agentMostRecent();
  return settingsProviderEnabled(recent) ? recent : (settingsProviderEnabled(g_agent) ? g_agent : firstEnabled());
}

AgentId faceDisplayedAgent() {
  if (g_mode == FaceViewMode::Auto) return mostRecentEnabled();
  return settingsProviderEnabled(g_agent) ? g_agent : firstEnabled();
}

AgentId faceFocusedAgent() {
  if (g_mode == FaceViewMode::All) return mostRecentEnabled();
  return faceDisplayedAgent();
}

uint8_t faceVisibleAgents(AgentId* out) {
  uint8_t n = 0;
  for (uint8_t i = 0; i < AGENT_COUNT; ++i) {
    if (settingsProviderEnabled(agentAt(i))) out[n++] = agentAt(i);
  }
  return n;
}

bool faceImportantState(AgentState s) {
  return s == AgentState::Attention || s == AgentState::Error ||
         s == AgentState::Abort || s == AgentState::Done;
}

bool faceBackgroundAlert(AgentId shown, uint32_t now, uint32_t windowMs, AgentId* out) {
  if (g_mode == FaceViewMode::Auto || faceViewShowsAll()) return false;
  bool found = false;
  uint32_t newest = 0;
  for (uint8_t i = 0; i < AGENT_COUNT; ++i) {
    const AgentId other = agentAt(i);
    if (other == shown || !settingsProviderEnabled(other) || !agentHasSeen(other)) continue;
    if (!faceImportantState(agentGetState(other))) continue;
    const uint32_t since = agentStateSince(other);
    if (now - since > windowMs) continue;
    if (!found || since > newest) {
      *out = other;
      newest = since;
      found = true;
    }
  }
  return found;
}
