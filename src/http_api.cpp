#include <ArduinoJson.h>
#include <WebServer.h>
#include <WiFi.h>
#include "http_api.h"
#include "agent.h"
#include "audio.h"
#include "face.h"
#include "diagnostics.h"
#include "hardware_profile.h"
#include "locale.h"
#include "version.h"

static WebServer server(80);

static AgentState stateFromName(const String& name, bool* ok) {
  *ok = true;
  if (name == "none" || name == "idle") return AgentState::Idle;
  if (name == "welcome" || name == "wakeup") return AgentState::Welcome;
  if (name == "reading") return AgentState::Reading;
  if (name == "thinking") return AgentState::Thinking;
  if (name == "typing" || name == "editing") return AgentState::Typing;
  if (name == "running") return AgentState::Running;
  if (name == "attention") return AgentState::Attention;
  if (name == "ring" || name == "done") return AgentState::Done;
  if (name == "error") return AgentState::Error;
  if (name == "abort" || name == "cancelled") return AgentState::Abort;
  if (name == "sleep" || name == "dead") return AgentState::Sleep;
  *ok = false;
  return AgentState::Idle;
}

static AgentId agentFromArg(const String& raw, bool* ok = nullptr) {
  String name(raw);
  name.toLowerCase();
  if (name == "codex") {
    if (ok) *ok = true;
    return AgentId::Codex;
  }
  if (name == "" || name == "claude") {
    if (ok) *ok = true;
    return AgentId::Claude;
  }
  if (ok) *ok = false;
  return AgentId::Claude;
}

static void addCors() {
  server.sendHeader("Access-Control-Allow-Origin", "*");
  server.sendHeader("Access-Control-Allow-Headers", "Content-Type");
}

static void addAgentStatus(JsonDocument& doc, AgentId agent) {
  // Keep keys static where possible; this avoids temporary-key allocations on
  // the ESP32 heap while /health is polled repeatedly.
  const bool claude = agent == AgentId::Claude;
  if (claude) {
    doc["claude_seen"] = agentHasSeen(agent);
    doc["claude_state"] = stateName(agentGetState(agent));
    doc["claude_detail"] = agentGetDetail(agent);
    doc["claude_task_active"] = agentTaskActive(agent);
    doc["claude_task_elapsed_ms"] = agentTaskElapsedMs(agent);
    doc["claude_tool_count"] = agentToolCount(agent);
    doc["claude_last_task_duration_ms"] = agentLastTaskDurationMs(agent);
    doc["claude_last_tool_count"] = agentLastToolCount(agent);
  } else {
    doc["codex_seen"] = agentHasSeen(agent);
    doc["codex_state"] = stateName(agentGetState(agent));
    doc["codex_detail"] = agentGetDetail(agent);
    doc["codex_task_active"] = agentTaskActive(agent);
    doc["codex_task_elapsed_ms"] = agentTaskElapsedMs(agent);
    doc["codex_tool_count"] = agentToolCount(agent);
    doc["codex_last_task_duration_ms"] = agentLastTaskDurationMs(agent);
    doc["codex_last_tool_count"] = agentLastToolCount(agent);
  }
}

static void handleHook(AgentId agent) {
  agentHandleHookJson(agent, server.arg("plain"));
  addCors();
  server.send(200, "application/json", "{}");
}

void httpApiBegin() {
  server.on("/", HTTP_GET, []() {
    addCors();
    server.send(200, "text/plain",
      AGENT_COMPANION_NAME " v" AGENT_COMPANION_VERSION "\n\n"
      "POST /hook/claude          Claude Code lifecycle hook\n"
      "POST /hook/codex           Codex lifecycle hook\n"
      "POST /hook                 Alias for Claude\n"
      "POST /anim?name=reading&agent=claude|codex\n"
      "POST /view?mode=claude|codex|both|auto\n"
      "GET  /health\n"
      "GET  /state\n");
  });

  server.on("/health", HTTP_GET, []() {
    JsonDocument doc;
    doc["ok"] = true;
    doc["version"] = AGENT_COMPANION_VERSION;
    doc["hardware"] = HARDWARE_PROFILE_NAME;
    doc["voice"] = audioIsEnabled();
    doc["view_mode"] = faceViewModeName();
    doc["uptime_ms"] = millis();
    doc["ip"] = WiFi.localIP().toString();
    doc["rssi"] = WiFi.RSSI();
    doc["free_heap"] = ESP.getFreeHeap();
    doc["min_free_heap"] = ESP.getMinFreeHeap();
    doc["last_reset"] = diagnosticsResetReason();
    doc["boot_count"] = diagnosticsBootCount();
    addAgentStatus(doc, AgentId::Claude);
    addAgentStatus(doc, AgentId::Codex);
    String out;
    serializeJson(doc, out);
    addCors();
    server.send(200, "application/json", out);
  });

  server.on("/state", HTTP_GET, []() {
    JsonDocument doc;
    doc["view_mode"] = faceViewModeName();
    doc["focused_agent"] = agentName(faceFocusedAgent());
    addAgentStatus(doc, AgentId::Claude);
    addAgentStatus(doc, AgentId::Codex);
    String out;
    serializeJson(doc, out);
    addCors();
    server.send(200, "application/json", out);
  });

  server.on("/hook", HTTP_POST, []() { handleHook(AgentId::Claude); });
  server.on("/hook/claude", HTTP_POST, []() { handleHook(AgentId::Claude); });
  server.on("/hook/codex", HTTP_POST, []() { handleHook(AgentId::Codex); });

  server.on("/anim", HTTP_GET, []() {
    bool agentOk = false;
    AgentId agent = agentFromArg(server.arg("agent"), &agentOk);
    if (!agentOk) {
      addCors();
      server.send(400, "application/json", "{\"ok\":false,\"error\":\"unknown agent\"}");
      return;
    }
    JsonDocument doc;
    doc["ok"] = true;
    doc["agent"] = agentName(agent);
    doc["animation"] = stateName(agentGetState(agent));
    String out;
    serializeJson(doc, out);
    addCors();
    server.send(200, "application/json", out);
  });

  server.on("/anim", HTTP_POST, []() {
    bool stateOk = false;
    bool agentOk = false;
    AgentState state = stateFromName(server.arg("name"), &stateOk);
    AgentId agent = agentFromArg(server.arg("agent"), &agentOk);
    if (!stateOk || !agentOk) {
      addCors();
      server.send(400, "application/json", "{\"ok\":false,\"error\":\"unknown state or agent\"}");
      return;
    }
    agentSetState(agent, state, String(tr("Manual: ", "Ручной режим: ")) + stateTitle(state));
    JsonDocument doc;
    doc["ok"] = true;
    doc["agent"] = agentName(agent);
    doc["animation"] = stateName(state);
    String out;
    serializeJson(doc, out);
    addCors();
    server.send(200, "application/json", out);
  });

  server.on("/view", HTTP_GET, []() {
    JsonDocument doc;
    doc["ok"] = true;
    doc["mode"] = faceViewModeName();
    String out;
    serializeJson(doc, out);
    addCors();
    server.send(200, "application/json", out);
  });

  server.on("/view", HTTP_POST, []() {
    if (!faceSetViewModeByName(server.arg("mode"))) {
      addCors();
      server.send(400, "application/json", "{\"ok\":false,\"error\":\"unknown view mode\"}");
      return;
    }
    JsonDocument doc;
    doc["ok"] = true;
    doc["mode"] = faceViewModeName();
    String out;
    serializeJson(doc, out);
    addCors();
    server.send(200, "application/json", out);
  });

  server.onNotFound([]() {
    addCors();
    server.send(404, "application/json", "{\"ok\":false,\"error\":\"not found\"}");
  });

  server.begin();
  Serial.printf("HTTP ready: http://%s/health\n", WiFi.localIP().toString().c_str());
}

void httpApiLoop() { server.handleClient(); }
