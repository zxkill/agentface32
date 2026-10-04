#include <Arduino.h>
#include <WiFi.h>
#include <ESPmDNS.h>

#include "config.h"
#include "hardware_profile.h"
#include "hardware.h"
#include "agent.h"
#include "audio.h"
#include "diagnostics.h"
#include "face.h"
#include "http_api.h"
#include "locale.h"

static bool g_networkServicesStarted = false;
static String g_lastIp;

static void startNetworkServicesIfNeeded() {
  if (WiFi.status() != WL_CONNECTED) return;

  const String ip = WiFi.localIP().toString();
  if (ip != g_lastIp) {
    g_lastIp = ip;
    faceSetNetwork(ip);
    Serial.printf("WiFi connected: %s\n", ip.c_str());
  }

  if (g_networkServicesStarted) return;

  if (!MDNS.begin(DEVICE_HOSTNAME)) {
    Serial.println("mDNS start failed; numeric IP still works");
  } else {
    Serial.printf("Open: http://%s.local/health\n", DEVICE_HOSTNAME);
  }

  httpApiBegin();
  g_networkServicesStarted = true;
  Serial.println("HTTP API listening on port 80");
  Serial.println("Agent endpoints: /hook/claude and /hook/codex");
}

static void connectWifi() {
  WiFi.mode(WIFI_STA);
  WiFi.setSleep(true);
  WiFi.setAutoReconnect(true);
  WiFi.persistent(false);
  WiFi.setHostname(DEVICE_HOSTNAME);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

  const uint32_t started = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - started < 20000) {
    hardwareUpdate();
    faceLoop();
    delay(20);
  }

  if (WiFi.status() == WL_CONNECTED) {
    startNetworkServicesIfNeeded();
  } else {
    faceSetNetwork("NO WIFI");
    Serial.println("Initial WiFi connection failed; background reconnect remains enabled");
    Serial.println("Check include/config.local.h if it does not recover");
  }
}

void setup() {
  hardwareBegin();
  diagnosticsBegin();
  hardwareSetDisplayBrightness(DISPLAY_BRIGHTNESS);

  agentBegin();
  faceBegin();
  audioBegin();
  faceSetVoiceEnabled(audioIsEnabled());

  Serial.printf("AgentFace32 %s\n", HARDWARE_PROFILE_NAME);
  Serial.printf("UI language: %s\n", UI_LANGUAGE);

  connectWifi();
}

void loop() {
  hardwareUpdate();

  if (hardwareButtonAClicked()) {
    audioToggle();
    faceSetVoiceEnabled(audioIsEnabled());
  }

  if (hardwareButtonBClicked()) {
    audioButtonClick();
    static uint8_t demo = 0;
    static const AgentState demoStates[] = {
      AgentState::Welcome,
      AgentState::Reading,
      AgentState::Thinking,
      AgentState::Typing,
      AgentState::Running,
      AgentState::Attention,
      AgentState::Done,
      AgentState::Error,
      AgentState::Abort,
      AgentState::Idle
    };
    AgentId target = faceFocusedAgent();
    AgentState state = demoStates[demo++ % (sizeof(demoStates) / sizeof(demoStates[0]))];
    agentSetState(target, state, tr("Demo", "Демонстрация"), true);
  }

  if (hardwareButtonCHeld()) {
    audioButtonClick();
    faceToggleAutoMode();
  } else if (hardwareButtonCClicked()) {
    audioButtonClick();
    faceCycleViewMode();
  }

  startNetworkServicesIfNeeded();
  if (g_networkServicesStarted) httpApiLoop();
  agentLoop();
  audioLoop();
  faceLoop();
  delay(2);
}
