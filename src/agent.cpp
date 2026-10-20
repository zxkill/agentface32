#include <ArduinoJson.h>
#include <string.h>
#include "agent.h"
#include "audio.h"
#include "config.h"
#include "translations.h"
#include "logger.h"

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
  // Two-phase suppression:
  //   Phase 1 (grace): no alert at all, wait for auto-approval to resolve.
  //   Phase 2 (post-grace): set a flag but don't show Attention yet; if any
  //     continuation event arrives, clear everything silently.
  //     Only after post-grace expires with no activity do we show Attention.
  bool permissionPending = false;
  uint32_t permissionRequestedAt = 0;
  String permissionTool;
  bool permissionAttentionPending = false;  // phase 2 flag
};

static AgentContext g_agents[AGENT_COUNT];
static AgentId g_mostRecent = AgentId::Claude;

static size_t idx(AgentId agent) {
  const size_t i = (size_t)agent;
  return i < AGENT_COUNT ? i : 0;
}
static AgentContext& ctx(AgentId agent) { return g_agents[idx(agent)]; }
static const AgentContext& cctx(AgentId agent) { return g_agents[idx(agent)]; }

// Provider registry. Adding a provider: extend AgentId/AGENT_COUNT in agent.h
// and add a row here; /hook/<key>, settings and the UI pick it up.
// The dialect selects the small payload normalizer in agentHandleHookJson.
enum class HookDialect : uint8_t {
  Claude,    // Claude Code: PascalCase hook_event_name, tool_input
  Codex,     // Codex: Claude-shaped payload plus permission quirks
  Cursor,    // Cursor: camelCase events, top-level hook fields
  Cline,     // Cline: hookName + preToolUse.toolName/parameters
  OpenCode   // OpenCode plugin we ship: canonical event names
};

struct ProviderInfo {
  const char* key;
  const char* name;
  HookDialect dialect;
};

static const ProviderInfo kProviders[AGENT_COUNT] = {
  {"claude", "Claude", HookDialect::Claude},
  {"codex", "Codex", HookDialect::Codex},
  {"opencode", "OpenCode", HookDialect::OpenCode},
  {"cline", "Cline", HookDialect::Cline},
  {"cursor", "Cursor", HookDialect::Cursor},
};

const char* agentName(AgentId agent) { return kProviders[idx(agent)].name; }
const char* agentKey(AgentId agent) { return kProviders[idx(agent)].key; }
AgentId agentAt(uint8_t index) { return (AgentId)(index < AGENT_COUNT ? index : 0); }

bool agentFromKey(const String& raw, AgentId* out) {
  String key(raw);
  key.trim();
  key.toLowerCase();
  for (uint8_t i = 0; i < AGENT_COUNT; ++i) {
    if (key == kProviders[i].key) {
      *out = agentAt(i);
      return true;
    }
  }
  return false;
}

static HookDialect dialectOf(AgentId agent) { return kProviders[idx(agent)].dialect; }

static String fieldString(JsonVariant obj, const char* key) {
  if (!obj.is<JsonObject>()) return String();
  const char* raw = obj[key] | "";
  return String(raw);
}

static String firstField(JsonVariant obj, const char* const* keys, size_t count) {
  for (size_t i = 0; i < count; ++i) {
    String value = fieldString(obj, keys[i]);
    if (value.length()) return value;
  }
  return String();
}

// Every provider puts the tool parameters somewhere slightly different:
// Claude/Cursor use tool_input, Cline nests them in preToolUse.parameters,
// and our OpenCode plugin forwards them as tool_input/args.
static JsonVariant toolInputOf(JsonDocument& doc) {
  if (doc["tool_input"].is<JsonObject>()) return doc["tool_input"];
  if (doc["preToolUse"]["parameters"].is<JsonObject>()) return doc["preToolUse"]["parameters"];
  if (doc["postToolUse"]["parameters"].is<JsonObject>()) return doc["postToolUse"]["parameters"];
  if (doc["args"].is<JsonObject>()) return doc["args"];
  return doc.as<JsonVariant>();
}

static String toolNameOf(JsonDocument& doc) {
  const char* tool = doc["tool_name"] | "";
  if (!strlen(tool)) tool = doc["preToolUse"]["toolName"] | "";
  if (!strlen(tool)) tool = doc["postToolUse"]["toolName"] | "";
  if (!strlen(tool)) tool = doc["tool"] | "";
  return String(tool);
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
  for (size_t i = 0; i < AGENT_COUNT; ++i) {
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
  if (changed) c.stateSince = millis();
  c.seen = true;
  if (markRecent) {
    c.lastEventAt = millis();
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

static bool toolLooksLikeRead(const String& tool) {
  return tool.indexOf("read") >= 0 || tool.indexOf("grep") >= 0 ||
         tool.indexOf("search") >= 0 || tool.indexOf("find") >= 0 ||
         tool.indexOf("list") >= 0 || tool.indexOf("glob") >= 0;
}

// Tool naming differs per provider:
//   Claude/Cursor: Read, Write, Shell, Grep, WebFetch
//   Cline: read_file, write_to_file, execute_command, search_files
//   OpenCode: read, write, bash, grep, webfetch
// Classification is therefore substring-based, not an exact-name match.
static void handlePreToolUse(AgentId agent, JsonVariant input, const String& tool) {
  AgentContext& c = ctx(agent);
  if (!c.taskActive) taskStart(agent);
  ++c.toolCount;

  static const char* const kPathKeys[] = {"file_path", "path", "filePath", "file"};
  static const char* const kCommandKeys[] = {"command", "cmd"};
  static const char* const kPatternKeys[] = {"pattern", "regex", "query", "search"};
  static const char* const kUrlKeys[] = {"url", "uri"};

  const String filePath = firstField(input, kPathKeys, sizeof(kPathKeys) / sizeof(kPathKeys[0]));
  const String command = firstField(input, kCommandKeys, sizeof(kCommandKeys) / sizeof(kCommandKeys[0]));
  const String pattern = firstField(input, kPatternKeys, sizeof(kPatternKeys) / sizeof(kPatternKeys[0]));
  const String url = firstField(input, kUrlKeys, sizeof(kUrlKeys) / sizeof(kUrlKeys[0]));

  String lower(tool);
  lower.toLowerCase();

  const bool isShell = lower == "bash" || lower == "powershell" || lower == "shell" ||
                       lower == "execute_command" || lower == "run_command" ||
                       lower == "terminal" || lower.indexOf("shell") >= 0;
  const bool isWeb = lower.indexOf("web_fetch") >= 0 || lower.indexOf("webfetch") >= 0 ||
                     lower.indexOf("web_search") >= 0 || lower.indexOf("websearch") >= 0 ||
                     lower.indexOf("browser_action") >= 0;
  const bool isWebSearch = lower.indexOf("search") >= 0 && !command.length();
  const bool isPatch = lower == "apply_patch" || lower.indexOf("apply_patch") >= 0;
  const bool isEdit = lower.indexOf("edit") >= 0 || lower.indexOf("write") >= 0 ||
                      isPatch || lower.indexOf("replace_in_file") >= 0 ||
                      lower.indexOf("notebookedit") >= 0 || lower == "delete";
  const bool isGlob = lower.indexOf("glob") >= 0 || lower.indexOf("list") >= 0;
  const bool isGrep = lower.indexOf("grep") >= 0;

  if (isWeb) {
    if (isWebSearch) {
      String d = pattern.length() ? String(tr("Search: ", "Ищу: ")) + pattern : tr("Searching web", "Ищу в интернете");
      agentSetState(agent, AgentState::Reading, shorten(d), false);
      return;
    }
    String host = hostFromUrl(url.c_str());
    agentSetState(agent, AgentState::Reading,
                  host.length() ? String(tr("Page: ", "Страница: ")) + host : tr("Opening page", "Открываю страницу"), false);
    return;
  }

  if (isShell) {
    String d = command.length() ? String(tr("Command: ", "Команда: ")) + command : tr("Running command", "Выполняю команду");
    agentSetState(agent, AgentState::Running, shorten(d), false);
    return;
  }

  // Codex reports file edits as apply_patch and places the patch text in
  // tool_input.command. Extract the first affected path when possible.
  if (isPatch) {
    String path = firstPatchPath(command.c_str());
    if (!path.length()) path = filePath;
    agentSetState(agent, AgentState::Typing,
                  path.length() ? String(tr("File: ", "Файл: ")) + path : tr("Editing files", "Изменяю файлы"), false);
    return;
  }

  if (isGlob) {
    String d = pattern.length() ? String(tr("Pattern: ", "Шаблон: ")) + pattern
                                : (filePath.length() ? fileDetail(filePath.c_str()) : String(tr("Finding files", "Ищу файлы")));
    agentSetState(agent, AgentState::Reading, shorten(d), false);
    return;
  }

  if (isGrep) {
    String d = pattern.length() ? String(tr("Search: ", "Ищу: ")) + pattern : tr("Searching project", "Ищу по проекту");
    agentSetState(agent, AgentState::Reading, shorten(d), false);
    return;
  }

  if (isEdit) {
    agentSetState(agent, AgentState::Typing,
                  filePath.length() ? shorten(fileDetail(filePath.c_str())) : tr("Editing file", "Изменяю файл"), false);
    return;
  }

  if (toolLooksLikeRead(lower)) {
    String d = filePath.length() ? fileDetail(filePath.c_str())
                                 : (pattern.length() ? String(tr("Search: ", "Ищу: ")) + pattern
                                                     : String(tr("Tool: ", "Инструмент: ")) + tool);
    agentSetState(agent, AgentState::Reading, shorten(d), false);
    return;
  }

  String d = tool.length() ? String(tr("Tool: ", "Инструмент: ")) + tool : tr("Using tool", "Работаю с инструментом");
  agentSetState(agent, AgentState::Thinking, shorten(d), false);
}

// Cursor and Cline use their own lifecycle vocabularies. Translate them into
// the canonical event names handled below; return an empty string for events
// the display does not care about (they are acknowledged and ignored).
static String canonicalEvent(AgentId agent, JsonDocument& doc, String* source) {
  String event = doc["hook_event_name"] | "";
  if (!event.length()) event = doc["hookName"] | "";
  if (!event.length()) return String();

  if (dialectOf(agent) == HookDialect::Cursor) {
    if (event == "sessionStart") { *source = "startup"; return "SessionStart"; }
    if (event == "sessionEnd") return "SessionEnd";
    if (event == "beforeSubmitPrompt") return "UserPromptSubmit";
    if (event == "preToolUse") return "PreToolUse";
    if (event == "postToolUse") return "PostToolUse";
    if (event == "postToolUseFailure") return "PostToolUseFailure";
    if (event == "subagentStart") return "SubagentStart";
    if (event == "subagentStop") return "SubagentStop";
    if (event == "preCompact") return "PreCompact";
    if (event == "stop") {
      const String status = doc["status"] | "";
      if (status == "aborted") return "Interrupt";
      if (status == "error") return "StopFailure";
      return "Stop";
    }
    return String();  // beforeShellExecution, afterFileEdit, ...
  }

  if (dialectOf(agent) == HookDialect::Cline) {
    if (event == "TaskStart") { *source = "startup"; return "SessionStart"; }
    if (event == "TaskResume") { *source = "resume"; return "SessionStart"; }
    if (event == "TaskCancel") return "Interrupt";
    if (event == "TaskComplete") return "Stop";
    if (event == "UserPromptSubmit") return "UserPromptSubmit";
    if (event == "PreToolUse") return "PreToolUse";
    if (event == "PostToolUse") {
      const bool success = doc["postToolUse"]["success"] | true;
      return success ? "PostToolUse" : "PostToolUseFailure";
    }
    if (event == "PreCompact") return "PreCompact";
    return String();  // PreCompact/TaskComplete while "coming soon"
  }

  return event;
}

void agentHandleHookJson(AgentId agent, const String& body) {
  JsonDocument doc;
  DeserializationError err = deserializeJson(doc, body);
  if (err) {
    logWarn(agentKey(agent), "hook JSON error: %s", err.c_str());
    return;
  }

  String source(doc["source"] | "");
  const String eventName = canonicalEvent(agent, doc, &source);
  const String tool = toolNameOf(doc);
  const char* event = eventName.c_str();
  const char* sourceName = source.c_str();
  const char* reason = doc["reason"] | "";
  const char* permissionMode = doc["permission_mode"] | "";

  if (!eventName.length()) {
    logInfo(agentKey(agent), "hook event ignored");
    return;
  }

  logInfo(agentKey(agent), "hook=%s tool=%s", event, tool.c_str());

  AgentContext& current = ctx(agent);

  // Any activity from Codex means it did not stop waiting for the user.
  // This suppresses false alerts when auto-approval resolves the request.
  // We cancel the pending alert on any follow-up event including repeated
  // PermissionRequest with different tools or permission modes.
  if (agent == AgentId::Codex) {
    // Phase 1: grace period still running
    if (current.permissionPending) {
      const char* resolvedMode = doc["permission_mode"] | "";
      const bool isGranted = !strcmp(resolvedMode, "autoApproved") ||
                             !strcmp(resolvedMode, "approved") ||
                             !strcmp(resolvedMode, "bypassPermissions");

      if (isGranted || strcmp(event, "PermissionRequest") != 0) {
        logInfo("codex", "permission %s cancelled by hook=%s (mode=%s)",
                isGranted ? "granted" : "wait", event, resolvedMode);
        current.permissionPending = false;
        current.permissionAttentionPending = false;
        current.permissionRequestedAt = 0;
        current.permissionTool = "";
      } else {
        logInfo("codex", "permission still pending, refreshing grace window");
        current.permissionRequestedAt = millis();  // Refresh timer
      }
    }

    // Phase 2: post-grace window — any event from Codex means it moved on,
    // the permission auto-resolved or Codex continued independently.
    // Clear the pending attention regardless of current state (even if
    // Attention was already shown to the user).
    if (current.permissionAttentionPending &&
        strcmp(event, "PermissionRequest") != 0 &&
        strcmp(event, "Interrupt") != 0) {
      logInfo("codex", "phase 2 cancelled by hook=%s (auto-resolved)", event);
      current.permissionAttentionPending = false;
      current.permissionRequestedAt = 0;
      current.permissionTool = "";
      // Let the normal event handlers below transition to the correct state
    }
  }

  if (!strcmp(event, "SessionStart")) {
    agentSetState(agent,
                  !strcmp(sourceName, "startup") ? AgentState::Welcome : AgentState::Idle,
                  !strcmp(sourceName, "startup") ? String(agentName(agent)) + tr(" connected", " подключён") : tr("Session resumed", "Сеанс продолжен"));
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
    handlePreToolUse(agent, toolInputOf(doc), tool);
    return;
  }

  // Claude currently has PostToolBatch; Codex exposes PostToolUse.
  // Skip state update if already Thinking with a specific detail from PreToolUse —
  // preserves "Команда: ..." / "Файл: ..." instead of overwriting with "Обрабатываю результат".
  if (!strcmp(event, "PostToolBatch") || !strcmp(event, "PostToolUse")) {
    String curDetail = agentGetDetail(agent);
    const char* genericProcessing = tr("Processing result", "Обрабатываю результат");
    const char* genericTool = tr("Using tool", "Работаю с инструментом");
    const bool hasSpecificDetail = curDetail.length() &&
        curDetail != genericProcessing &&
        curDetail != genericTool;
    if (!hasSpecificDetail) {
      agentSetState(agent, AgentState::Thinking, genericProcessing, false);
    }
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
      if (!strcmp(permissionMode, "dontAsk") || !strcmp(permissionMode, "bypassPermissions") ||
          !strcmp(permissionMode, "autoApproved") || !strcmp(permissionMode, "approved")) {
        // Don't alert, but track the event so g_mostRecent stays current.
        current.lastEventAt = millis();
        g_mostRecent = agent;
        return;
      }

      // Other setups can resolve an approval request automatically moments
      // after the lifecycle event is emitted. Keep the current face/state
      // unchanged for a grace period and alert only if Codex remains quiet.
      current.permissionPending = true;
      current.permissionRequestedAt = millis();
      current.permissionTool = tool;
      logInfo("codex", "permission pending; grace=%lu ms tool=%s",
              (unsigned long)CODEX_PERMISSION_GRACE_MS, tool.c_str());
      return;
    }

    String d = tool.length() ? String(tr("Approval needed: ", "Нужно подтвердить: ")) + tool :
               String(tr("Check ", "Проверьте ")) + agentName(agent);
    agentSetState(agent, AgentState::Attention, shorten(d));
    return;
  }

  // Explicit permission resolution events
  if (!strcmp(event, "PermissionGranted") || !strcmp(event, "PermissionApproved")) {
    if (agent == AgentId::Codex) {
      const bool wasAttending = (current.state == AgentState::Attention);
      logInfo("codex", "permission granted, clearing pending alert");
      current.permissionPending = false;
      current.permissionAttentionPending = false;
      current.permissionRequestedAt = 0;
      current.permissionTool = "";
      // If we were showing attention for this permission, transition back
      if (wasAttending) {
        agentSetState(agent, AgentState::Thinking, tr("Permission resolved", "Разрешение обработано"), false);
      }
      return;
    }
    // Claude: permission was granted, resume from Attention -> Thinking
    agentSetState(agent, AgentState::Thinking, tr("Permission granted", "Разрешение получено"), false);
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
      logInfo(agentKey(agent), "notification ignored: %s", notificationType);
      return;
    }
    String d = tool.length() ? String(tr("Approval needed: ", "Нужно подтвердить: ")) + tool :
               String(tr("Check ", "Проверьте ")) + agentName(agent);
    agentSetState(agent, AgentState::Attention, shorten(d));
    return;
  }

  if (!strcmp(event, "PermissionDenied") || !strcmp(event, "PermissionRejected")) {
    if (agent == AgentId::Codex) {
      logInfo("codex", "permission denied/rejected, clearing pending");
      current.permissionPending = false;
      current.permissionAttentionPending = false;
      current.permissionRequestedAt = 0;
      current.permissionTool = "";
    } else {
      agentSetState(agent, AgentState::Abort, tr("Permission denied", "Доступ не разрешён"));
    }
    return;
  }

  if (!strcmp(event, "Interrupt")) {
    agentSetState(agent, AgentState::Abort, tr("Interrupted", "Работа прервана"));
    return;
  }

  if (!strcmp(event, "PostToolUseFailure")) {
    const char* errorText = doc["error"] | "";
    if (!strlen(errorText)) errorText = doc["error_message"] | "";
    if (!strlen(errorText)) errorText = doc["postToolUse"]["result"] | "";
    String d;
    if (tool.length()) d = String(tr("Failure: ", "Сбой: ")) + tool;
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
  for (size_t i = 0; i < AGENT_COUNT; ++i) {
    AgentId agent = agentAt(i);
    AgentContext& c = g_agents[i];

    // Phase 1 → Phase 2: grace period expired, but don't alert yet
    if (agent == AgentId::Codex && c.permissionPending &&
        now - c.permissionRequestedAt >= CODEX_PERMISSION_GRACE_MS) {
      c.permissionPending = false;
      c.permissionAttentionPending = true;
      logInfo("codex", "phase 2 started (post-grace=%lu ms), tool=%s",
              (unsigned long)CODEX_PERMISSION_POST_GRACE_MS, c.permissionTool.c_str());
    }

    // Phase 2 → Attention: post-grace also expired, show the alert
    if (agent == AgentId::Codex && c.permissionAttentionPending &&
        !c.permissionPending &&
        now - c.permissionRequestedAt >= CODEX_PERMISSION_GRACE_MS + CODEX_PERMISSION_POST_GRACE_MS) {
      String d = c.permissionTool.length()
                   ? String(tr("Approval needed: ", "Нужно подтвердить: ")) + c.permissionTool
                   : String(tr("Approval needed", "Нужно подтверждение"));
      c.permissionTool = "";
      c.permissionAttentionPending = false;
      logInfo("codex", "post-grace expired -> showing attention");
      agentSetState(agent, AgentState::Attention, shorten(d), true);
    }

    if (!isOneShotState(c.state) || now - c.stateSince <= ONE_SHOT_STATE_MS) continue;

    if (c.taskActive) {
      setStateInternal(agent, AgentState::Thinking, tr("Waiting for work to continue", "Жду продолжения работы"), false, true);
    } else {
      setStateInternal(agent, AgentState::Idle, idleDetail(agent), false, true);
    }
  }
}
