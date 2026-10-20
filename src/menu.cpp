#include "menu.h"
#include "audio.h"
#include "diagnostics.h"
#include "face.h"
#include "hardware.h"
#include "hardware_profile.h"
#include "logger.h"
#include "ota.h"
#include "settings.h"
#include "translations.h"
#include "version.h"
#include "wifi_manager.h"

static constexpr uint32_t kMenuTimeoutMs = 30000;
static const uint8_t kBrightnessSteps[] = {10, 25, 50, 75, 100};

enum class ItemKind : uint8_t { Brightness, Provider, Sound, Language, WifiSetup, Network, Reboot, Exit };
struct MenuItem {
  ItemKind kind;
  AgentId agent;
};

static MenuItem g_items[FACE_PANEL_MAX_LINES];
static uint8_t g_itemCount = 0;
static bool g_open = false;
static uint8_t g_selected = 0;
static uint32_t g_lastInput = 0;
static FacePanel g_menuPanel;
static FacePanel g_infoPanel;
static const FacePanel* g_shown = nullptr;

static void addItem(ItemKind kind, AgentId agent = AgentId::Claude) {
  if (g_itemCount < FACE_PANEL_MAX_LINES) g_items[g_itemCount++] = {kind, agent};
}

static void buildItems() {
  g_itemCount = 0;
#if defined(HARDWARE_HAS_BRIGHTNESS)
  addItem(ItemKind::Brightness);
#endif
  for (uint8_t i = 0; i < AGENT_COUNT; ++i) addItem(ItemKind::Provider, agentAt(i));
  addItem(ItemKind::Sound);
  addItem(ItemKind::Language);
  addItem(ItemKind::WifiSetup);
  addItem(ItemKind::Network);
  addItem(ItemKind::Reboot);
  addItem(ItemKind::Exit);
}

static const char* onOff(bool on) { return on ? tr("ON", "ВКЛ") : tr("OFF", "ВЫКЛ"); }

static void fillMenuPanel() {
  FacePanel& p = g_menuPanel;
  p.title = tr("Settings", "Настройки");
  p.count = g_itemCount;
  p.selected = g_selected;
  p.progress = -1;
  p.footer = tr("A: next   B/C: select", "A: далее   B/C: выбрать");
  for (uint8_t i = 0; i < g_itemCount; ++i) {
    FacePanelLine& line = p.lines[i];
    const MenuItem& item = g_items[i];
    line.value = "";
    switch (item.kind) {
      case ItemKind::Brightness:
        line.label = tr("Brightness", "Яркость");
        line.value = String(settings().brightness) + "%";
        break;
      case ItemKind::Provider:
        line.label = agentName(item.agent);
        line.value = onOff(settingsProviderEnabled(item.agent));
        break;
      case ItemKind::Sound:
        line.label = tr("Sound", "Звук");
        line.value = onOff(audioIsEnabled());
        break;
      case ItemKind::Language:
        line.label = tr("Language", "Язык");
        line.value = localeIsRussian() ? "RU" : "EN";
        break;
      case ItemKind::WifiSetup:
        line.label = tr("Wi-Fi setup", "Настройка Wi-Fi");
        line.value = wifiSetupActive() ? tr("stop", "стоп") : tr("start", "запуск");
        break;
      case ItemKind::Network:
        line.label = tr("IP", "IP");
        line.value = wifiConnected() ? wifiIp() : String(tr("offline", "нет сети"));
        break;
      case ItemKind::Reboot:
        line.label = tr("Restart", "Перезагрузка");
        break;
      case ItemKind::Exit:
        line.label = tr("Exit", "Выход");
        line.value = "v" AGENT_COMPANION_VERSION;
        break;
    }
  }
}

void menuApplySettings() {
  faceSetBrightness(settings().brightness);
  audioSetEnabled(settings().sound);
  faceSetVoiceEnabled(audioIsEnabled());
  faceViewSanitize();
}

static void activate(const MenuItem& item) {
  switch (item.kind) {
    case ItemKind::Brightness: {
      uint8_t next = kBrightnessSteps[0];
      for (uint8_t step : kBrightnessSteps) {
        if (step > settings().brightness) { next = step; break; }
      }
      settingsSetBrightness(next);
      faceSetBrightness(next);
      break;
    }
    case ItemKind::Provider:
      // Refused for the last enabled provider.
      settingsSetProviderEnabled(item.agent, !settingsProviderEnabled(item.agent));
      faceViewSanitize();
      break;
    case ItemKind::Sound:
      audioToggle();
      settingsSetSound(audioIsEnabled());
      faceSetVoiceEnabled(audioIsEnabled());
      break;
    case ItemKind::Language:
      settingsSetRussian(!localeIsRussian());
      break;
    case ItemKind::WifiSetup:
      if (wifiSetupActive()) wifiStopSetup();
      else wifiStartSetup();
      g_open = false;  // show the setup instructions right away
      break;
    case ItemKind::Network:
      break;
    case ItemKind::Reboot:
      diagnosticsScheduleRestart(300, "menu");
      g_open = false;
      break;
    case ItemKind::Exit:
      g_open = false;
      break;
  }
}

void menuBegin() {
  buildItems();
}

bool menuIsOpen() { return g_open; }

bool menuHandleButtons() {
  const uint32_t now = millis();
  if (!g_open) {
    if (!hardwareButtonAHeld()) return false;
    audioButtonClick();
    g_open = true;
    g_selected = 0;
    g_lastInput = now;
    logInfo("menu", "opened");
    return true;
  }

  const bool next = hardwareButtonAClicked();
  const bool select = hardwareButtonBClicked() || hardwareButtonCClicked() || hardwareButtonAHeld();
  hardwareButtonCHeld();  // swallow so it does not toggle Auto after closing

  if (next) {
    g_selected = (g_selected + 1) % g_itemCount;
    g_lastInput = now;
    audioButtonClick();
  } else if (select) {
    g_lastInput = now;
    audioButtonClick();
    activate(g_items[g_selected]);
  } else if (now - g_lastInput > kMenuTimeoutMs) {
    g_open = false;
  }
  return true;
}

static void fillSetupPanel() {
  FacePanel& p = g_infoPanel;
  p.title = tr("Wi-Fi setup", "Настройка Wi-Fi");
  p.selected = -1;
  p.progress = -1;
  uint8_t n = 0;
  p.lines[n++] = {tr("1. Join Wi-Fi", "1. Сеть Wi-Fi"), wifiSetupSsid()};
  p.lines[n++] = {tr("Password", "Пароль"), wifiSetupPassword()};
  p.lines[n++] = {tr("2. Open", "2. Откройте"), "http://" + wifiSetupIp()};
  if (wifiConnected()) p.lines[n++] = {tr("or on LAN", "или в сети"), "http://" + wifiIp()};
  else if (settingsHasWifi()) p.lines[n++] = {tr("Saved network", "Сохранённая сеть"), settings().wifiSsid};
  p.lines[n++] = {tr("Clients", "Подключено"), String(wifiSetupClients())};
  p.count = n;
#if defined(HARDWARE_HAS_BUTTON_A)
  p.footer = tr("Hold A: menu", "Удерж. A: меню");
#else
  p.footer = "";
#endif
}

static void fillOtaPanel() {
  FacePanel& p = g_infoPanel;
  p.title = tr("Firmware update", "Обновление прошивки");
  p.selected = -1;
  p.count = 0;
  p.progress = otaProgress();
  p.lines[p.count++] = {tr("Receiving", "Загрузка"), String(otaProgress()) + "%"};
  p.lines[p.count++] = {tr("Do not power off", "Не выключайте питание"), ""};
  p.footer = "";
}

static void fillRestartPanel() {
  FacePanel& p = g_infoPanel;
  p.title = tr("Restarting...", "Перезагрузка...");
  p.selected = -1;
  p.progress = -1;
  p.count = 0;
  p.footer = "";
}

void menuLoop() {
  const FacePanel* panel = nullptr;
  if (otaInProgress()) {
    fillOtaPanel();
    panel = &g_infoPanel;
  } else if (diagnosticsRestartPending()) {
    fillRestartPanel();
    panel = &g_infoPanel;
  } else if (g_open) {
    fillMenuPanel();
    panel = &g_menuPanel;
  } else if (wifiSetupActive()) {
    fillSetupPanel();
    panel = &g_infoPanel;
  }
  if (panel != g_shown) {
    g_shown = panel;
    faceShowPanel(panel);
  }
}
