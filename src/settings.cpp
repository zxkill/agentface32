#include <Preferences.h>
#include <string.h>
#include "settings.h"
#include "config.h"
#include "logger.h"

static DeviceSettings g_settings;
static Preferences g_prefs;
static const char* kNamespace = "agentface";

static bool compiledWifiIsPlaceholder() {
  return !strlen(WIFI_SSID) || !strcmp(WIFI_SSID, "YOUR_WIFI_SSID") || !strcmp(WIFI_SSID, "MyWiFi");
}

static uint8_t clampBrightness(int percent) {
  return (uint8_t)constrain(percent, 10, 100);
}

static bool validHostname(const String& name) {
  if (name.length() < 1 || name.length() > 32) return false;
  if (name[0] == '-' || name[name.length() - 1] == '-') return false;
  for (size_t i = 0; i < name.length(); ++i) {
    const char c = name[i];
    if (!(isalnum((unsigned char)c) || c == '-')) return false;
  }
  return true;
}

void settingsBegin() {
  g_prefs.begin(kNamespace, false);

  // Existing installs configured through config.local.h keep working: the
  // compiled credentials are used until the web UI saves something else.
  if (g_prefs.isKey("ssid")) {
    g_settings.wifiSsid = g_prefs.getString("ssid", "");
    g_settings.wifiPassword = g_prefs.getString("pass", "");
  } else if (!compiledWifiIsPlaceholder()) {
    g_settings.wifiSsid = WIFI_SSID;
    g_settings.wifiPassword = WIFI_PASSWORD;
  }

  g_settings.hostname = g_prefs.getString("host", DEVICE_HOSTNAME);
  if (!validHostname(g_settings.hostname)) g_settings.hostname = DEVICE_HOSTNAME;
  g_settings.adminPassword = g_prefs.getString("admin", "");
  g_settings.russian = g_prefs.getBool("ru", strcmp(UI_LANGUAGE, "ru") == 0);
  g_settings.sound = g_prefs.getBool("sound", VOICE_ENABLED_BY_DEFAULT);
  g_settings.autoUpdate = g_prefs.getBool("autoupd", true);
  g_settings.brightness = clampBrightness(g_prefs.getUChar("bright", (DISPLAY_BRIGHTNESS * 100) / 255));

  const uint32_t allProviders = (1UL << AGENT_COUNT) - 1;
  g_settings.providerMask = g_prefs.getULong("prov", allProviders) & allProviders;
  if (!g_settings.providerMask) g_settings.providerMask = allProviders;

  logInfo("settings", "wifi=%s host=%s lang=%s providers=0x%lx brightness=%u%%",
          g_settings.wifiSsid.length() ? g_settings.wifiSsid.c_str() : "<none>",
          g_settings.hostname.c_str(), g_settings.russian ? "ru" : "en",
          (unsigned long)g_settings.providerMask, g_settings.brightness);
}

const DeviceSettings& settings() { return g_settings; }

bool settingsHasWifi() { return g_settings.wifiSsid.length() > 0; }

bool settingsProviderEnabled(AgentId agent) {
  return (g_settings.providerMask >> (uint8_t)agent) & 1UL;
}

uint8_t settingsEnabledProviderCount() {
  uint8_t n = 0;
  for (uint8_t i = 0; i < AGENT_COUNT; ++i) n += (g_settings.providerMask >> i) & 1UL;
  return n;
}

bool settingsSetWifi(const String& ssid, const String& password) {
  if (ssid.length() > 32) return false;
  if (password.length() && (password.length() < 8 || password.length() > 63)) return false;
  g_settings.wifiSsid = ssid;
  g_settings.wifiPassword = password;
  g_prefs.putString("ssid", ssid);
  g_prefs.putString("pass", password);
  logInfo("settings", "wifi network set to '%s'", ssid.c_str());
  return true;
}

bool settingsSetHostname(const String& hostname) {
  String name(hostname);
  name.toLowerCase();
  if (!validHostname(name)) return false;
  g_settings.hostname = name;
  g_prefs.putString("host", name);
  return true;
}

void settingsSetAdminPassword(const String& password) {
  g_settings.adminPassword = password;
  g_prefs.putString("admin", password);
  logInfo("settings", "admin password %s", password.length() ? "set" : "cleared");
}

void settingsSetRussian(bool russian) {
  g_settings.russian = russian;
  g_prefs.putBool("ru", russian);
}

void settingsSetSound(bool enabled) {
  g_settings.sound = enabled;
  g_prefs.putBool("sound", enabled);
}

void settingsSetAutoUpdate(bool enabled) {
  g_settings.autoUpdate = enabled;
  g_prefs.putBool("autoupd", enabled);
}

void settingsSetBrightness(uint8_t percent) {
  g_settings.brightness = clampBrightness(percent);
  g_prefs.putUChar("bright", g_settings.brightness);
}

bool settingsSetProviderEnabled(AgentId agent, bool enabled) {
  const uint32_t bit = 1UL << (uint8_t)agent;
  const uint32_t mask = enabled ? (g_settings.providerMask | bit) : (g_settings.providerMask & ~bit);
  if (!mask) return false;
  g_settings.providerMask = mask;
  g_prefs.putULong("prov", mask);
  logInfo("settings", "provider %s %s", agentName(agent), enabled ? "enabled" : "disabled");
  return true;
}
