#include <ArduinoJson.h>
#include <string.h>
#include "agent.h"
#include "audio.h"
#include "config.h"
#include "locale.h"

struct AgentContext {
  AgentState state = AgentState::Idle;
  String detail;
  uint32_t stateSince = 0;
  uint32_t lastEventAt = 0;
  bool seen = false;

  bool taskActive = false;
  uint32_t taskStartedAt = 0;
  uint32_t lastTaskDuration = 0;
  uint32_t toolCount = 0;
  uint32_t lastToolCount = 0;

  // Codex may emit PermissionRequest even when auto-approval is enabled.
  // Delay the visible/audio alert briefly and cancel it if Codex continues.
  bool permissionPending = false;
  uint32_t permissionRequestedAt = 0;
  String permissionTool;
};

static AgentContext g_agents[2];
static AgentId g_mostRecent = AgentId::Claude;

static size_t idx(AgentId agent) { return agent == AgentId::Codex ? 1 : 0; }
static AgentContext& ctx(AgentId agent) { return g_agents[idx(agent)]; }
static const AgentContext& cctx(AgentId agent) { return g_agents[idx(agent)]; }

const char* agentName(AgentId agent) {
  return agent == AgentId::Codex ? "Codex" : "Claude";
}

static String oneLine(String s) {
  s.replace("\r", " ");
  s.replace("\n", " ");
  s.replace("\t", " ");
  while (s.indexOf("  ") >= 0) s.replace("  ", " ");
  s.trim();
  return s;
}

static String shorten(const String& s, size_t maxBytes = 58) {
  String clean = oneLine(s);
  if (clean.length() <= maxBytes) return clean;
  size_t cut = maxBytes;
  while (cut > 0 && (((uint8_t)clean[cut] & 0xC0) == 0x80)) --cut;
  return clean.substring(0, cut) + "...";
}

static String compactPath(const char* filePath) {
  if (!filePath || !strlen(filePath)) return "";
  String path(filePath);
  path.replace("\\", "/");
  int last = path.lastIndexOf('/');
  if (last <= 0) return path;
  String parent = path.substring(0, last);
  int prev = parent.lastIndexOf('/');
  return prev >= 0 ? path.substring(prev + 1) : path;
}

static String fileDetail(const char* filePath) {
  String path = compactPath(filePath);
  return path.length() ? String(tr("File: ", "Файл: ")) + path : String();
}

static String firstPatchPath(const char* raw) {
  if (!raw || !strlen(raw)) return "";
  String patch(raw);
  const char* markers[] = {
    "*** Update File: ",
    "*** Add File: ",
    "*** Delete File: "
  };
  for (const char* marker : markers) {
    int p = patch.indexOf(marker);
    if (p < 0) continue;
    p += strlen(marker);
    int e = patch.indexOf('\n', p);
    if (e < 0) e = patch.length();
    String path = patch.substring(p, e);
    path.trim();
    return compactPath(path.c_str());
  }
  return "";
}

static String hostFromUrl(const char* raw) {
  if (!raw || !strlen(raw)) return "";
  String url(raw);
  int start = url.indexOf("://");
  start = start >= 0 ? start + 3 : 0;
  int end = url.indexOf('/', start);
  if (end < 0) end = url.length();
  return shorten(url.substring(start, end), 40);
}

static String formatDuration(uint32_t ms) {
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

static void taskStart(AgentId agent) {
  AgentContext& c = ctx(agent);
  c.taskActive = true;
  c.taskStartedAt = millis();
  c.toolCount = 0;
}

static void taskFinish(AgentId agent) {
  AgentContext& c = ctx(agent);
  if (!c.taskActive) return;
  c.lastTaskDuration = millis() - c.taskStartedAt;
  c.lastToolCount = c.toolCount;
  c.taskActive = false;
}

static String completedDetail(AgentId agent) {
  const AgentContext& c = cctx(agent);
  if (!c.lastTaskDuration) return tr("Task completed", "Задача завершена");
  return formatDuration(c.lastTaskDuration) + tr("  /  steps: ", "  /  шагов: ") + String(c.lastToolCount);
}

static String idleDetail(AgentId agent) {
  const AgentContext& c = cctx(agent);
  if (!c.seen) return tr("No events from agent", "Нет событий от агента");
  if (!c.lastTaskDuration) return tr("Ready", "Готов к работе");
  return String(tr("Last: ", "Последняя: ")) + formatDuration(c.lastTaskDuration) +
         tr("  /  steps: ", "  /  шагов: ") + String(c.lastToolCount);
}

void agentBegin() {
  const uint32_t now = millis();
  for (size_t i = 0; i < 2; ++i) {
    g_agents[i] = AgentContext();
    g_agents[i].stateSince = now;
    g_agents[i].lastEventAt = 0;
    g_agents[i].detail = tr("No events from agent", "Нет событий от агента");
  }
  g_mostRecent = AgentId::Claude;
}

static void setStateInternal(AgentId agent, AgentState state, const String& detail,
                             bool playAudio, bool markRecent) {
  AgentContext& c = ctx(agent);
  const bool changed = state != c.state;
  c.state = state;
  c.detail = detail;
  c.stateSince = millis();
  c.seen = true;
  if (markRecent) {
    c.lastEventAt = c.stateSince;
    g_mostRecent = agent;
  }
  if (playAudio && changed) audioForState(state);
}

void agentSetState(AgentId agent, AgentState state, const String& detail, bool playAudio) {
  setStateInternal(agent, state, detail, playAudio, true);
}

AgentState agentGetState(AgentId agent) { return cctx(agent).state; }
String agentGetDetail(AgentId agent) { return cctx(agent).detail; }
bool agentTaskActive(AgentId agent) { return cctx(agent).taskActive; }
uint32_t agentTaskElapsedMs(AgentId agent) {
  const AgentContext& c = cctx(agent);
  return c.taskActive ? (millis() - c.taskStartedAt) : 0;
}
uint32_t agentLastTaskDurationMs(AgentId agent) { return cctx(agent).lastTaskDuration; }
uint32_t agentToolCount(AgentId agent) { return cctx(agent).toolCount; }
uint32_t agentLastToolCount(AgentId agent) { return cctx(agent).lastToolCount; }
uint32_t agentStateSince(AgentId agent) { return cctx(agent).stateSince; }
uint32_t agentLastEventAt(AgentId agent) { return cctx(agent).lastEventAt; }
bool agentHasSeen(AgentId agent) { return cctx(agent).seen; }
AgentId agentMostRecent() { return g_mostRecent; }

static bool toolLooksLikeRead(const char* tool) {
  if (!tool || !strlen(tool)) return false;
  String t(tool);
  t.toLowerCase();
  return t == "read" || t.indexOf("read_file") >= 0 || t.indexOf("grep") >= 0 ||
         t.indexOf("search") >= 0 || t.indexOf("find") >= 0 || t.indexOf("list") >= 0 ||
         t.indexOf("glob") >= 0;
}

static void handlePreToolUse(AgentId agent, JsonDocument& doc, const char* tool) {
  AgentContext& c = ctx(agent);
  if (!c.taskActive) taskStart(agent);
  ++c.toolCount;

  const char* filePath = doc["tool_input"]["file_path"] | "";
  if (!strlen(filePath)) filePath = doc["tool_input"]["path"] | "";
  const char* command = doc["tool_input"]["command"] | "";
  const char* pattern = doc["tool_input"]["pattern"] | "";
  const char* query = doc["tool_input"]["query"] | "";
  const char* url = doc["tool_input"]["url"] | "";

  if (!strcmp(tool, "Read")) {
    agentSetState(agent, AgentState::Reading,
                  strlen(filePath) ? shorten(fileDetail(filePath)) : tr("Reading file", "Читаю файл"), false);
    return;
  }

  if (!strcmp(tool, "Grep")) {
    String d = strlen(pattern) ? String(tr("Search: ", "Ищу: ")) + pattern : tr("Searching project", "Ищу по проекту");
    agentSetState(agent, AgentState::Reading, shorten(d), false);
    return;
  }

  if (!strcmp(tool, "Glob")) {
    String d = strlen(pattern) ? String(tr("Pattern: ", "Шаблон: ")) + pattern : tr("Finding files", "Ищу файлы");
    agentSetState(agent, AgentState::Reading, shorten(d), false);
    return;
  }

  if (!strcmp(tool, "WebFetch")) {
    String host = hostFromUrl(url);
    agentSetState(agent, AgentState::Reading,
                  host.length() ? String(tr("Page: ", "Страница: ")) + host : tr("Opening page", "Открываю страницу"), false);
    return;
  }

  if (!strcmp(tool, "WebSearch")) {
    String d = strlen(query) ? String(tr("Search: ", "Ищу: ")) + query : tr("Searching web", "Ищу в интернете");
    agentSetState(agent, AgentState::Reading, shorten(d), false);
    return;
  }

  if (!strcmp(tool, "Edit") || !strcmp(tool, "Write") || !strcmp(tool, "NotebookEdit")) {
    agentSetState(agent, AgentState::Typing,
                  strlen(filePath) ? shorten(fileDetail(filePath)) : tr("Editing file", "Изменяю файл"), false);
    return;
  }

  // Codex reports file edits as apply_patch and places the patch text in
  // tool_input.command. Extract the first affected path when possible.
  if (!strcmp(tool, "apply_patch")) {
    String path = firstPatchPath(command);
    agentSetState(agent, AgentState::Typing,
                  path.length() ? String(tr("File: ", "Файл: ")) + path : tr("Editing files", "Изменяю файлы"), false);
    return;
  }

  if (!strcmp(tool, "Bash") || !strcmp(tool, "PowerShell")) {
    String d = strlen(command) ? String(tr("Command: ", "Команда: ")) + command : tr("Running command", "Выполняю команду");
    agentSetState(agent, AgentState::Running, shorten(d), false);
    return;
  }

  // Codex MCP/local tools often have names such as mcp__fs__read_file.
  // If an input path is available, surface it instead of a long tool id.
  if (toolLooksLikeRead(tool)) {
    String d = strlen(filePath) ? fileDetail(filePath) : String(tr("Tool: ", "Инструмент: ")) + tool;
    agentSetState(agent, AgentState::Reading, shorten(d), false);
    return;
  }

  String d = strlen(tool) ? String(tr("Tool: ", "Инструмент: ")) + tool : tr("Using tool", "Работаю с инструментом");
  agentSetState(agent, AgentState::Thinking, shorten(d), false);
}

void agentHandleHookJson(AgentId agent, const String& body) {
  JsonDocument doc;
  DeserializationError err = deserializeJson(doc, body);
  if (err) {
    Serial.printf("[%s] hook JSON error: %s\n", agentName(agent), err.c_str());
    return;
  }

  const char* event = doc["hook_event_name"] | "";
  const char* tool = doc["tool_name"] | "";
  const char* source = doc["source"] | "";
  const char* reason = doc["reason"] | "";
  const char* permissionMode = doc["permission_mode"] | "";

  Serial.printf("[%s] hook=%s tool=%s\n", agentName(agent), event, tool);

  AgentContext& current = ctx(agent);

  // A real follow-up event means Codex did not stop waiting for the user.
  // This is what suppresses false alerts when auto-approval handles the
  // permission request immediately. Repeated PermissionRequest refreshes the
  // grace window instead of cancelling it.
  if (agent == AgentId::Codex && current.permissionPending &&
      strcmp(event, "PermissionRequest") != 0) {
    Serial.printf("[Codex] permission wait cancelled by hook=%s\n", event);
    current.permissionPending = false;
    current.permissionRequestedAt = 0;
    current.permissionTool = "";
  }

  if (!strcmp(event, "SessionStart")) {
    agentSetState(agent,
                  !strcmp(source, "startup") ? AgentState::Welcome : AgentState::Idle,
                  !strcmp(source, "startup") ? String(agentName(agent)) + tr(" connected", " подключён") : tr("Session resumed", "Сеанс продолжен"));
    return;
  }

  if (!strcmp(event, "SessionEnd")) {
    if (agent == AgentId::Claude && (!strcmp(reason, "clear") || !strcmp(reason, "resume"))) return;
    taskFinish(agent);
    agentSetState(agent, AgentState::Sleep, tr("Session ended", "Сеанс завершён"), false);
    return;
  }

  if (!strcmp(event, "UserPromptSubmit")) {
    taskStart(agent);
    agentSetState(agent, AgentState::Reading, tr("New task received", "Получил новую задачу"), false);
    return;
  }

  if (!strcmp(event, "PreToolUse")) {
    handlePreToolUse(agent, doc, tool);
    return;
  }

  // Claude currently has PostToolBatch; Codex exposes PostToolUse.
  if (!strcmp(event, "PostToolBatch") || !strcmp(event, "PostToolUse")) {
    agentSetState(agent, AgentState::Thinking, tr("Processing result", "Обрабатываю результат"), false);
    return;
  }

  if (!strcmp(event, "SubagentStart")) {
    agentSetState(agent, AgentState::Thinking, tr("Starting subagent", "Запускаю помощника"), false);
    return;
  }

  if (!strcmp(event, "SubagentStop")) {
    agentSetState(agent, AgentState::Thinking, tr("Subagent finished", "Помощник завершил работу"), false);
    return;
  }

  if (!strcmp(event, "PreCompact") || !strcmp(event, "PostCompact")) {
    agentSetState(agent, AgentState::Thinking, tr("Compacting context", "Упорядочиваю контекст"), false);
    return;
  }

  if (!strcmp(event, "PermissionRequest")) {
    if (agent == AgentId::Codex) {
      // Some Codex permission modes explicitly mean that interactive approval
      // should not be required. If such a payload still reaches the observer,
      // never turn it into a human-attention alert.
      if (!strcmp(permissionMode, "dontAsk") || !strcmp(permissionMode, "bypassPermissions")) {
        Serial.printf("[Codex] permission request ignored for permission_mode=%s\n", permissionMode);
        return;
      }

      // Other setups can resolve an approval request automatically moments
      // after the lifecycle event is emitted. Keep the current face/state
      // unchanged for a grace period and alert only if Codex remains quiet.
      current.permissionPending = true;
      current.permissionRequestedAt = millis();
      current.permissionTool = strlen(tool) ? String(tool) : String();
      Serial.printf("[Codex] permission pending; grace=%lu ms tool=%s\n",
                    (unsigned long)CODEX_PERMISSION_GRACE_MS, tool);
      return;
    }

    String d = strlen(tool) ? String(tr("Approval needed: ", "Нужно подтвердить: ")) + tool :
               String(tr("Check ", "Проверьте ")) + agentName(agent);
    agentSetState(agent, AgentState::Attention, shorten(d));
    return;
  }

  if (!strcmp(event, "Notification")) {
    // Claude emits Notification for several informational events as well as
    // genuine user-input requests. Only surface the kinds that can actually
    // require the human; otherwise the companion becomes noisy.
    const char* notificationType = doc["notification_type"] | "";
    const bool needsHuman =
        !strcmp(notificationType, "permission_prompt") ||
        !strcmp(notificationType, "idle_prompt") ||
        !strcmp(notificationType, "agent_needs_input") ||
        !strcmp(notificationType, "elicitation_dialog") ||
        !strcmp(notificationType, "elicitation_url_dialog");
    if (!needsHuman) {
      Serial.printf("[%s] notification ignored: %s\n", agentName(agent), notificationType);
      return;
    }
    String d = strlen(tool) ? String(tr("Approval needed: ", "Нужно подтвердить: ")) + tool :
               String(tr("Check ", "Проверьте ")) + agentName(agent);
    agentSetState(agent, AgentState::Attention, shorten(d));
    return;
  }

  if (!strcmp(event, "PermissionDenied") || !strcmp(event, "Interrupt")) {
    agentSetState(agent, AgentState::Abort,
                  !strcmp(event, "Interrupt") ? tr("Interrupted", "Работа прервана") : tr("Permission denied", "Доступ не разрешён"));
    return;
  }

  if (!strcmp(event, "PostToolUseFailure")) {
    const char* errorText = doc["error"] | "";
    String d;
    if (strlen(tool)) d = String(tr("Failure: ", "Сбой: ")) + tool;
    else if (strlen(errorText)) d = String(errorText);
    else d = tr("Something went wrong", "Что-то пошло не так");
    agentSetState(agent, AgentState::Error, shorten(d));
    return;
  }

  if (!strcmp(event, "StopFailure")) {
    taskFinish(agent);
    const uint32_t duration = agentLastTaskDurationMs(agent);
    agentSetState(agent, AgentState::Error,
                  duration ? String(tr("Failed after ", "Сбой через ")) + formatDuration(duration) :
                             String(tr("Task failed", "Задача завершилась ошибкой")));
    return;
  }

  if (!strcmp(event, "Stop")) {
    taskFinish(agent);
    agentSetState(agent, AgentState::Done, completedDetail(agent));
    return;
  }
}

void agentLoop() {
  const uint32_t now = millis();
  for (size_t i = 0; i < 2; ++i) {
    AgentId agent = i == 0 ? AgentId::Claude : AgentId::Codex;
    AgentContext& c = g_agents[i];

    if (agent == AgentId::Codex && c.permissionPending &&
        now - c.permissionRequestedAt >= CODEX_PERMISSION_GRACE_MS) {
      c.permissionPending = false;
      c.permissionRequestedAt = 0;

      String d = c.permissionTool.length()
                   ? String(tr("Approval needed: ", "Нужно подтвердить: ")) + c.permissionTool
                   : String(tr("Approval needed", "Нужно подтверждение"));
      c.permissionTool = "";

      Serial.println("[Codex] permission grace expired -> user attention required");
      agentSetState(agent, AgentState::Attention, shorten(d), true);
    }

    if (!isOneShotState(c.state) || now - c.stateSince <= ONE_SHOT_STATE_MS) continue;

    if (c.taskActive) {
      setStateInternal(agent, AgentState::Thinking, tr("Waiting for work to continue", "Жду продолжения работы"), false, false);
    } else {
      setStateInternal(agent, AgentState::Idle, idleDetail(agent), false, false);
    }
  }
}
