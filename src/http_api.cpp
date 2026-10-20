#include <ArduinoJson.h>
#include <WebServer.h>
#include <WiFi.h>
#include <esp_partition.h>
#include "http_api.h"
#include "agent.h"
#include "audio.h"
#include "face.h"
#include "diagnostics.h"
#include "hardware_profile.h"
#include "logger.h"
#include "menu.h"
#include "ota.h"
#include "settings.h"
#include "translations.h"
#include "version.h"
#include "web_page.h"
#include "wifi_manager.h"

static WebServer server(80);
static bool g_uploadRejected = false;
static bool g_uploadOk = false;
static uint8_t g_uploadShownProgress = 255;

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

static AgentId agentFromArg(const String& raw, bool* ok) {
  AgentId agent = AgentId::Claude;
  *ok = raw.length() == 0 || agentFromKey(raw, &agent);
  return agent;
}

static void addCors() {
  server.sendHeader("Access-Control-Allow-Origin", "*");
  server.sendHeader("Access-Control-Allow-Headers", "Content-Type");
}

static void sendJson(int code, JsonDocument& doc) {
  String out;
  serializeJson(doc, out);
  addCors();
  server.send(code, "application/json", out);
}

static void sendError(int code, const char* error) {
  JsonDocument doc;
  doc["ok"] = false;
  doc["error"] = error;
  sendJson(code, doc);
}

// Settings, OTA, logs and restarts require the admin password once one is set
// (HTTP Basic auth, any user name). Hooks and status stay open as before.
static bool requireAdmin() {
  const String& password = settings().adminPassword;
  if (!password.length()) return true;
  if (server.authenticate("admin", password.c_str())) return true;
  server.requestAuthentication(BASIC_AUTH, AGENT_COMPANION_NAME);
  return false;
}

static void addAgentStatus(JsonDocument& doc, AgentId agent) {
  const String key(agentKey(agent));
  doc[key + "_enabled"] = settingsProviderEnabled(agent);
  doc[key + "_seen"] = agentHasSeen(agent);
  doc[key + "_state"] = stateName(agentGetState(agent));
  doc[key + "_detail"] = agentGetDetail(agent);
  doc[key + "_task_active"] = agentTaskActive(agent);
  doc[key + "_task_elapsed_ms"] = agentTaskElapsedMs(agent);
  doc[key + "_tool_count"] = agentToolCount(agent);
  doc[key + "_last_task_duration_ms"] = agentLastTaskDurationMs(agent);
  doc[key + "_last_tool_count"] = agentLastToolCount(agent);
}

static void addAllAgents(JsonDocument& doc) {
  for (uint8_t i = 0; i < AGENT_COUNT; ++i) addAgentStatus(doc, agentAt(i));
}

// WebServer only keeps the raw body ("plain") for non-form content types;
// `curl -d` without `-H Content-Type:application/json` sends a form body that
// the library discards before handlers run.
static const char* kNeedJsonError = "body missing: send JSON with Content-Type: application/json";

static void handleHook(AgentId agent) {
  addCors();
  if (!settingsProviderEnabled(agent)) {
    server.send(200, "application/json", "{\"ok\":true,\"ignored\":\"provider disabled\"}");
    return;
  }
  if (!server.hasArg("plain")) {
    logWarn(agentKey(agent), "hook without JSON body (missing Content-Type: application/json?)");
    sendError(400, kNeedJsonError);
    return;
  }
  agentHandleHookJson(agent, server.arg("plain"));
  server.send(200, "application/json", "{}");
}

// --- Settings ---------------------------------------------------------------

static void handleGetSettings() {
  if (!requireAdmin()) return;
  const DeviceSettings& s = settings();
  JsonDocument doc;
  doc["wifi_ssid"] = s.wifiSsid;
  doc["wifi_password_set"] = s.wifiPassword.length() > 0;
  doc["hostname"] = s.hostname;
  doc["language"] = s.russian ? "ru" : "en";
  doc["sound"] = audioIsEnabled();
#if defined(HARDWARE_HAS_BRIGHTNESS)
  doc["brightness_supported"] = true;
#else
  doc["brightness_supported"] = false;
#endif
  doc["brightness"] = s.brightness;
  doc["admin_password_set"] = s.adminPassword.length() > 0;
  JsonArray providers = doc["providers"].to<JsonArray>();
  for (uint8_t i = 0; i < AGENT_COUNT; ++i) {
    JsonObject p = providers.add<JsonObject>();
    p["id"] = agentKey(agentAt(i));
    p["name"] = agentName(agentAt(i));
    p["enabled"] = settingsProviderEnabled(agentAt(i));
  }
  doc["setup_active"] = wifiSetupActive();
  doc["setup_ssid"] = wifiSetupSsid();
  sendJson(200, doc);
}

static void handlePostSettings() {
  if (!requireAdmin()) return;
  JsonDocument doc;
  if (!server.hasArg("plain")) {
    sendError(400, kNeedJsonError);
    return;
  }
  if (deserializeJson(doc, server.arg("plain"))) {
    sendError(400, "invalid JSON");
    return;
  }

  // Validate everything first so a bad field does not leave a half-applied save.
  if (doc["providers"].is<JsonObject>()) {
    bool any = false;
    for (uint8_t i = 0; i < AGENT_COUNT; ++i) {
      const AgentId agent = agentAt(i);
      any |= doc["providers"][agentKey(agent)] | settingsProviderEnabled(agent);
    }
    if (!any) {
      sendError(400, "at least one provider must stay enabled");
      return;
    }
  }
  const bool wifiChange = doc["wifi_ssid"].is<const char*>();
  String ssid = wifiChange ? String(doc["wifi_ssid"].as<const char*>()) : settings().wifiSsid;
  String password = doc["wifi_password"].is<const char*>() ? String(doc["wifi_password"].as<const char*>())
                                                          : settings().wifiPassword;
  ssid.trim();
  if (wifiChange && (ssid.length() > 32 || (password.length() && (password.length() < 8 || password.length() > 63)))) {
    sendError(400, "Wi-Fi password must be empty or 8..63 characters, SSID up to 32");
    return;
  }

  bool restart = false;
  if (doc["hostname"].is<const char*>()) {
    const String hostname = doc["hostname"].as<const char*>();
    if (hostname != settings().hostname) {
      if (!settingsSetHostname(hostname)) {
        sendError(400, "hostname: letters, digits and '-' only");
        return;
      }
      restart = true;
    }
  }
  if (wifiChange || doc["wifi_password"].is<const char*>()) {
    if (ssid != settings().wifiSsid || password != settings().wifiPassword) {
      settingsSetWifi(ssid, password);
      restart = true;
    }
  }
  if (doc["language"].is<const char*>()) settingsSetRussian(String(doc["language"].as<const char*>()) == "ru");
  if (doc["sound"].is<bool>()) settingsSetSound(doc["sound"].as<bool>());
  if (doc["brightness"].is<int>()) settingsSetBrightness((uint8_t)constrain(doc["brightness"].as<int>(), 0, 100));
  if (doc["admin_password"].is<const char*>()) settingsSetAdminPassword(doc["admin_password"].as<const char*>());
  if (doc["providers"].is<JsonObject>()) {
    // Enable first so disabling never hits the "last provider" guard midway.
    for (int pass = 0; pass < 2; ++pass) {
      for (uint8_t i = 0; i < AGENT_COUNT; ++i) {
        const AgentId agent = agentAt(i);
        JsonVariant v = doc["providers"][agentKey(agent)];
        if (!v.is<bool>() || v.as<bool>() != (pass == 0)) continue;
        settingsSetProviderEnabled(agent, v.as<bool>());
      }
    }
  }
  menuApplySettings();

  JsonDocument res;
  res["ok"] = true;
  res["restart"] = restart;
  sendJson(200, res);
  if (restart) diagnosticsScheduleRestart(1500, "network settings changed");
}

static void handleWifiScan() {
  if (!requireAdmin()) return;
  int n = WiFi.scanComplete();
  JsonDocument doc;
  if (n == WIFI_SCAN_FAILED || server.hasArg("refresh")) {
    if (n >= 0) WiFi.scanDelete();
    WiFi.scanNetworks(true);
    doc["scanning"] = true;
    sendJson(200, doc);
    return;
  }
  if (n == WIFI_SCAN_RUNNING) {
    doc["scanning"] = true;
    sendJson(200, doc);
    return;
  }
  doc["scanning"] = false;
  JsonArray list = doc["networks"].to<JsonArray>();
  for (int i = 0; i < n; ++i) {
    const String ssid = WiFi.SSID(i);
    if (!ssid.length()) continue;
    bool duplicate = false;
    for (JsonObject existing : list) {
      if (ssid == existing["ssid"].as<const char*>()) { duplicate = true; break; }
    }
    if (duplicate) continue;
    JsonObject item = list.add<JsonObject>();
    item["ssid"] = ssid;
    item["rssi"] = WiFi.RSSI(i);
    item["secure"] = WiFi.encryptionType(i) != WIFI_AUTH_OPEN;
  }
  sendJson(200, doc);
}

static void handleWifiSetup() {
  if (!requireAdmin()) return;
  const String action = server.arg("action");
  if (action == "start") wifiStartSetup();
  else if (action == "stop") wifiStopSetup();
  else {
    sendError(400, "action must be start or stop");
    return;
  }
  JsonDocument doc;
  doc["ok"] = true;
  doc["setup_active"] = wifiSetupActive();
  sendJson(200, doc);
}

// --- OTA ----------------------------------------------------------------------

static void handleUpdateUpload() {
  HTTPUpload& upload = server.upload();
  if (upload.status == UPLOAD_FILE_START) {
    g_uploadOk = false;
    g_uploadRejected = !settings().adminPassword.length() ? false
                       : !server.authenticate("admin", settings().adminPassword.c_str());
    if (g_uploadRejected) {
      logWarn("ota", "upload rejected: bad credentials");
      return;
    }
    // Multipart overhead is small; Content-Length is good enough for progress.
    const size_t expected = server.hasHeader("Content-Length") ? server.header("Content-Length").toInt() : 0;
    g_uploadShownProgress = 255;
    otaStart(expected, server.arg("md5"));
  } else if (upload.status == UPLOAD_FILE_WRITE) {
    if (g_uploadRejected || !otaInProgress()) return;
    otaWrite(upload.buf, upload.currentSize);
    // The main loop is blocked during the upload; draw progress from here.
    const uint8_t pct = otaProgress();
    if (pct / 5 != g_uploadShownProgress / 5) {
      g_uploadShownProgress = pct;
      menuLoop();
      faceRenderNow();
    }
  } else if (upload.status == UPLOAD_FILE_END) {
    if (g_uploadRejected) return;
    g_uploadOk = otaFinish();
  } else if (upload.status == UPLOAD_FILE_ABORTED) {
    logWarn("ota", "upload aborted by client");
    otaAbort();
  }
}

static void handleUpdateDone() {
  if (g_uploadRejected) {
    server.requestAuthentication(BASIC_AUTH, AGENT_COMPANION_NAME);
    return;
  }
  if (!g_uploadOk) {
    JsonDocument doc;
    doc["ok"] = false;
    doc["error"] = otaLastError().length() ? otaLastError() : String("no firmware received");
    sendJson(500, doc);
    return;
  }
  JsonDocument doc;
  doc["ok"] = true;
  doc["restart"] = true;
  sendJson(200, doc);
  diagnosticsScheduleRestart(800, "firmware update");
}

// --- Logs and crash data --------------------------------------------------------

static void handleLogs() {
  if (!requireAdmin()) return;
  uint32_t pos = server.hasArg("since") ? (uint32_t)strtoul(server.arg("since").c_str(), nullptr, 10) : 0;
  const uint32_t end = loggerEnd();
  const uint32_t oldest = loggerOldest();
  if (pos < oldest || pos > end) pos = oldest;

  addCors();
  server.sendHeader("X-Log-Next", String(end));
  server.sendHeader("X-Log-Oldest", String(oldest));
  server.sendHeader("Cache-Control", "no-store");
  server.setContentLength(end - pos);
  server.send(200, "text/plain; charset=utf-8", "");
  char buf[512];
  while (pos < end) {
    const size_t n = loggerRead(&pos, buf, min((uint32_t)sizeof(buf), end - pos));
    if (!n) break;
    server.sendContent(buf, n);
  }
}

static void handleCrash() {
  if (!requireAdmin()) return;
  JsonDocument doc;
  diagnosticsCrashJson(doc);
  doc["firmware"] = AGENT_COMPANION_VERSION;
  sendJson(200, doc);
}

static void handleCrashClear() {
  if (!requireAdmin()) return;
  JsonDocument doc;
  doc["ok"] = diagnosticsCoreDumpErase();
  logInfo("crash", "core dump erased");
  sendJson(200, doc);
}

static void handleCoreDump() {
  if (!requireAdmin()) return;
  size_t addr = 0, size = 0;
  const esp_partition_t* part = esp_partition_find_first(ESP_PARTITION_TYPE_DATA,
                                                         ESP_PARTITION_SUBTYPE_DATA_COREDUMP, nullptr);
  if (!part || !diagnosticsCoreDumpInfo(&addr, &size) || addr < part->address) {
    sendError(404, "no core dump stored");
    return;
  }
  addCors();
  server.sendHeader("Content-Disposition", "attachment; filename=coredump.bin");
  server.setContentLength(size);
  server.send(200, "application/octet-stream", "");
  uint8_t buf[512];
  for (size_t off = 0; off < size; off += sizeof(buf)) {
    const size_t n = min(sizeof(buf), size - off);
    if (esp_partition_read(part, addr - part->address + off, buf, n) != ESP_OK) break;
    server.sendContent((const char*)buf, n);
  }
}

// --- Status --------------------------------------------------------------------

static void handleHealth() {
  JsonDocument doc;
  doc["ok"] = true;
  doc["version"] = AGENT_COMPANION_VERSION;
  doc["build"] = __DATE__ " " __TIME__;
  doc["hardware"] = HARDWARE_PROFILE_NAME;
  doc["voice"] = audioIsEnabled();
  doc["view_mode"] = faceViewModeName();
  doc["uptime_ms"] = millis();
  doc["hostname"] = settings().hostname;
  doc["ip"] = wifiIp();
  doc["wifi_ssid"] = WiFi.SSID();
  doc["rssi"] = WiFi.RSSI();
  doc["setup_active"] = wifiSetupActive();
  doc["free_heap"] = ESP.getFreeHeap();
  doc["min_free_heap"] = ESP.getMinFreeHeap();
  doc["last_reset"] = diagnosticsResetReason();
  doc["boot_count"] = diagnosticsBootCount();
  doc["app_partition"] = otaRunningPartition();
  doc["ota_pending_verify"] = otaPendingVerify();
  doc["ota_rolled_back"] = otaRolledBack();
  size_t addr = 0, size = 0;
  doc["coredump"] = diagnosticsCoreDumpInfo(&addr, &size);
  doc["log_bytes"] = loggerEnd();
  addAllAgents(doc);
  sendJson(200, doc);
}

static bool isSetupPortalRequest() {
  return wifiSetupActive() && server.client().localIP() == WiFi.softAPIP();
}

void httpApiBegin() {
  static const char* headerKeys[] = {"Content-Length"};
  server.collectHeaders(headerKeys, 1);

  server.on("/", HTTP_GET, []() {
    if (!requireAdmin()) return;
    server.sendHeader("Cache-Control", "no-store");
    server.send_P(200, "text/html; charset=utf-8", WEB_PAGE_HTML);
  });
  server.on("/help", HTTP_GET, []() {
    addCors();
    server.send(200, "text/plain",
      AGENT_COMPANION_NAME " v" AGENT_COMPANION_VERSION "\n\n"
      "POST /hook/<provider>      lifecycle hook (claude, codex)\n"
      "POST /hook                 alias for Claude\n"
      "POST /anim?name=reading&agent=<provider>\n"
      "POST /view?mode=<provider>|both|auto\n"
      "GET  /health, /state\n"
      "GET  /                     settings page\n"
      "GET  /api/settings, POST /api/settings (JSON)\n"
      "GET  /api/wifi/scan, POST /api/wifi/setup?action=start|stop\n"
      "POST /update               firmware upload (multipart, field 'firmware', ?md5=)\n"
      "GET  /logs?since=N         log ring buffer (X-Log-Next header)\n"
      "GET  /api/crash, POST /api/crash/clear, GET /coredump\n"
      "POST /api/reboot\n");
  });
  server.on("/health", HTTP_GET, handleHealth);
  server.on("/state", HTTP_GET, []() {
    JsonDocument doc;
    doc["view_mode"] = faceViewModeName();
    doc["focused_agent"] = agentName(faceFocusedAgent());
    addAllAgents(doc);
    sendJson(200, doc);
  });

  server.on("/hook", HTTP_POST, []() { handleHook(AgentId::Claude); });
  for (uint8_t i = 0; i < AGENT_COUNT; ++i) {
    const AgentId agent = agentAt(i);
    server.on(String("/hook/") + agentKey(agent), HTTP_POST, [agent]() { handleHook(agent); });
  }

  server.on("/anim", HTTP_GET, []() {
    bool agentOk = false;
    AgentId agent = agentFromArg(server.arg("agent"), &agentOk);
    if (!agentOk) {
      sendError(400, "unknown agent");
      return;
    }
    JsonDocument doc;
    doc["ok"] = true;
    doc["agent"] = agentName(agent);
    doc["animation"] = stateName(agentGetState(agent));
    sendJson(200, doc);
  });
  server.on("/anim", HTTP_POST, []() {
    bool stateOk = false;
    bool agentOk = false;
    AgentState state = stateFromName(server.arg("name"), &stateOk);
    AgentId agent = agentFromArg(server.arg("agent"), &agentOk);
    if (!stateOk || !agentOk) {
      sendError(400, "unknown state or agent");
      return;
    }
    agentSetState(agent, state, String(tr("Manual: ", "Ручной режим: ")) + stateTitle(state));
    JsonDocument doc;
    doc["ok"] = true;
    doc["agent"] = agentName(agent);
    doc["animation"] = stateName(state);
    sendJson(200, doc);
  });
  server.on("/view", HTTP_GET, []() {
    JsonDocument doc;
    doc["ok"] = true;
    doc["mode"] = faceViewModeName();
    sendJson(200, doc);
  });
  server.on("/view", HTTP_POST, []() {
    if (!faceSetViewModeByName(server.arg("mode"))) {
      sendError(400, "unknown or disabled view mode");
      return;
    }
    JsonDocument doc;
    doc["ok"] = true;
    doc["mode"] = faceViewModeName();
    sendJson(200, doc);
  });

  server.on("/api/settings", HTTP_GET, handleGetSettings);
  server.on("/api/settings", HTTP_POST, handlePostSettings);
  server.on("/api/wifi/scan", HTTP_GET, handleWifiScan);
  server.on("/api/wifi/setup", HTTP_POST, handleWifiSetup);
  server.on("/api/reboot", HTTP_POST, []() {
    if (!requireAdmin()) return;
    JsonDocument doc;
    doc["ok"] = true;
    sendJson(200, doc);
    diagnosticsScheduleRestart(500, "web request");
  });
  server.on("/update", HTTP_POST, handleUpdateDone, handleUpdateUpload);
  server.on("/logs", HTTP_GET, handleLogs);
  server.on("/api/crash", HTTP_GET, handleCrash);
  server.on("/api/crash/clear", HTTP_POST, handleCrashClear);
  server.on("/coredump", HTTP_GET, handleCoreDump);

  server.onNotFound([]() {
    // Captive portal: phones probe random URLs after joining the setup AP.
    if (isSetupPortalRequest()) {
      server.sendHeader("Location", String("http://") + WiFi.softAPIP().toString() + "/");
      server.send(302, "text/plain", "");
      return;
    }
    sendError(404, "not found");
  });
  server.begin();
  logInfo("http", "server listening on port 80");
}

void httpApiLoop() { server.handleClient(); }
