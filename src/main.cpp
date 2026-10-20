#include <Arduino.h>
#include "config.h"
#include "hardware_profile.h"
#include "hardware.h"
#include "agent.h"
#include "audio.h"
#include "diagnostics.h"
#include "face.h"
#include "http_api.h"
#include "logger.h"
#include "menu.h"
#include "ota.h"
#include "settings.h"
#include "translations.h"
#include "update_check.h"
#include "version.h"
#include "wifi_manager.h"

static void handleFaceButtons() {
  if (hardwareButtonAClicked()) {
    audioToggle();
    settingsSetSound(audioIsEnabled());
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
}

void setup() {
  hardwareBegin();
  diagnosticsBegin();
  loggerBegin(diagnosticsResetReason(), diagnosticsBootCount(), diagnosticsColdBoot());
  logInfo("boot", "%s v%s, free heap %u", HARDWARE_PROFILE_NAME, AGENT_COMPANION_VERSION,
          (unsigned)ESP.getFreeHeap());
  diagnosticsReportCrash();
  otaBegin();
  settingsBegin();

  agentBegin();
  faceBegin();
  audioBegin();
  menuBegin();
  menuApplySettings();

  wifiManagerBegin();
  httpApiBegin();
  updateBegin();
}

void loop() {
  hardwareUpdate();
  if (!menuHandleButtons()) handleFaceButtons();

  wifiManagerLoop();
  httpApiLoop();
  agentLoop();
  otaLoop();
  updateLoop();
  audioLoop();
  menuLoop();
  faceLoop();
  diagnosticsLoop();
  delay(2);
}
