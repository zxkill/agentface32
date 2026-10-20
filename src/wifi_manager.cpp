#include <DNSServer.h>
#include <ESPmDNS.h>
#include <WiFi.h>
#include "wifi_manager.h"
#include "config.h"
#include "face.h"
#include "logger.h"
#include "settings.h"

// While the fallback AP is up, the station is retried only when nobody is
// connected to the AP: channel scanning would drop the phone mid-setup.
static constexpr uint32_t kStaRetryMs = 60000;
// A manually started setup AP closes itself if forgotten while the station
// connection works.
static constexpr uint32_t kManualSetupTimeoutMs = 15UL * 60UL * 1000UL;

static DNSServer g_dns;
static String g_apSsid;
static bool g_apActive = false;
static bool g_apManual = false;
static uint32_t g_apStartedAt = 0;
static bool g_connected = false;
static bool g_mdnsStarted = false;
static uint32_t g_disconnectedSince = 0;
static uint32_t g_lastStaRetry = 0;

static void beginStation() {
  const DeviceSettings& s = settings();
  WiFi.begin(s.wifiSsid.c_str(), s.wifiPassword.c_str());
  g_lastStaRetry = millis();
}

static void updateFaceNetwork() {
  faceSetNetwork(g_connected ? WiFi.localIP().toString() : String());
}

static void startAp(bool manual) {
  if (g_apActive) {
    g_apManual = g_apManual || manual;
    return;
  }
  // AP+STA even without a saved network so the setup page can scan.
  WiFi.mode(WIFI_AP_STA);
  WiFi.setSleep(false);
  WiFi.softAPConfig(IPAddress(192, 168, 4, 1), IPAddress(192, 168, 4, 1), IPAddress(255, 255, 255, 0));
  if (!WiFi.softAP(g_apSsid.c_str(), SETUP_AP_PASSWORD)) {
    logError("wifi", "failed to start setup AP");
    return;
  }
  g_dns.setErrorReplyCode(DNSReplyCode::NoError);
  g_dns.start(53, "*", WiFi.softAPIP());
  g_apActive = true;
  g_apManual = manual;
  g_apStartedAt = millis();
  logInfo("wifi", "setup AP '%s' started (%s) at %s", g_apSsid.c_str(),
          manual ? "manual" : (settingsHasWifi() ? "fallback" : "first boot"),
          WiFi.softAPIP().toString().c_str());
}

static void stopAp() {
  if (!g_apActive) return;
  g_dns.stop();
  WiFi.softAPdisconnect(true);
  WiFi.mode(WIFI_STA);
  WiFi.setSleep(true);
  WiFi.setAutoReconnect(true);
  g_apActive = false;
  g_apManual = false;
  logInfo("wifi", "setup AP stopped");
}

void wifiManagerBegin() {
  char suffix[8];
  const uint64_t mac = ESP.getEfuseMac();
  snprintf(suffix, sizeof(suffix), "%02X%02X", (uint8_t)(mac >> 32), (uint8_t)(mac >> 40));
  g_apSsid = String(SETUP_AP_PREFIX) + "-" + suffix;

  WiFi.persistent(false);
  WiFi.setHostname(settings().hostname.c_str());

  if (!settingsHasWifi()) {
    logInfo("wifi", "no saved network, starting setup");
    startAp(false);
    updateFaceNetwork();
    return;
  }

  WiFi.mode(WIFI_STA);
  WiFi.setSleep(true);
  WiFi.setAutoReconnect(true);
  logInfo("wifi", "connecting to '%s'", settings().wifiSsid.c_str());
  beginStation();
  g_disconnectedSince = millis();
  updateFaceNetwork();
}

void wifiManagerLoop() {
  const uint32_t now = millis();
  if (g_apActive) g_dns.processNextRequest();

  const bool connected = WiFi.status() == WL_CONNECTED;
  if (connected != g_connected) {
    g_connected = connected;
    if (connected) {
      logInfo("wifi", "connected to '%s' ip=%s rssi=%d", WiFi.SSID().c_str(),
              WiFi.localIP().toString().c_str(), WiFi.RSSI());
      if (!g_mdnsStarted) {
        g_mdnsStarted = MDNS.begin(settings().hostname.c_str());
        if (g_mdnsStarted) {
          MDNS.addService("http", "tcp", 80);
          logInfo("wifi", "mDNS: http://%s.local/", settings().hostname.c_str());
        } else {
          logWarn("wifi", "mDNS start failed; numeric IP still works");
        }
      }
    } else {
      logWarn("wifi", "connection lost (status=%d)", (int)WiFi.status());
      g_disconnectedSince = now;
    }
    updateFaceNetwork();
  }

  if (!settingsHasWifi()) return;

  if (connected) {
    // Fallback AP is no longer needed once the saved network is back.
    if (g_apActive && !g_apManual && WiFi.softAPgetStationNum() == 0) stopAp();
    if (g_apActive && g_apManual && now - g_apStartedAt > kManualSetupTimeoutMs &&
        WiFi.softAPgetStationNum() == 0) {
      logInfo("wifi", "manual setup timed out");
      stopAp();
    }
    return;
  }

  if (!g_apActive && now - g_disconnectedSince > WIFI_SETUP_FALLBACK_MS) {
    logWarn("wifi", "'%s' unreachable for %lus, opening setup AP",
            settings().wifiSsid.c_str(), (unsigned long)(WIFI_SETUP_FALLBACK_MS / 1000));
    WiFi.setAutoReconnect(false);
    startAp(false);
  }

  if (g_apActive && WiFi.softAPgetStationNum() == 0 && now - g_lastStaRetry > kStaRetryMs) {
    logInfo("wifi", "retrying '%s'", settings().wifiSsid.c_str());
    beginStation();
  }
}

bool wifiConnected() { return g_connected; }
String wifiIp() { return g_connected ? WiFi.localIP().toString() : String(); }
bool wifiSetupActive() { return g_apActive; }

void wifiStartSetup() {
  startAp(true);
}

void wifiStopSetup() {
  if (!settingsHasWifi()) return;
  stopAp();
  if (!g_connected) {
    beginStation();
    g_disconnectedSince = millis();
  }
}

String wifiSetupSsid() { return g_apSsid; }
const char* wifiSetupPassword() { return SETUP_AP_PASSWORD; }
String wifiSetupIp() { return WiFi.softAPIP().toString(); }
uint8_t wifiSetupClients() { return g_apActive ? WiFi.softAPgetStationNum() : 0; }
