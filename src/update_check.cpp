#include <ArduinoJson.h>
#include <HTTPClient.h>
#include <WiFiClientSecure.h>
#include <esp_ota_ops.h>
#include "update_check.h"
#include "logger.h"
#include "ota.h"
#include "settings.h"
#include "translations.h"
#include "version.h"
#include "wifi_manager.h"

#ifndef AGENTFACE_RELEASE_ENV
#define AGENTFACE_RELEASE_ENV "unknown"
#endif

// Small generated manifest in the repository. It avoids the 27 KB GitHub
// releases API response and its 60 requests/hour rate limit, which matters on
// an ESP32 with a TLS session in RAM.
static const char* kManifestUrl =
    "https://raw.githubusercontent.com/zxkill/agentface32/main/update.json";
static const char* kReleaseBase =
    "https://github.com/zxkill/agentface32/releases/download/v";

// GitHub currently serves api.github.com from Sectigo/USERTrust and the
// release download redirects to objects.githubusercontent.com (ISRG Root X1).
// Both roots are embedded so the update path does not fall back to insecure.
static const char kRootCAs[] PROGMEM = R"EOF(
-----BEGIN CERTIFICATE-----
MIICjzCCAhWgAwIBAgIQXIuZxVqUxdJxVt7NiYDMJjAKBggqhkjOPQQDAzCBiDEL
MAkGA1UEBhMCVVMxEzARBgNVBAgTCk5ldyBKZXJzZXkxFDASBgNVBAcTC0plcnNl
eSBDaXR5MR4wHAYDVQQKExVUaGUgVVNFUlRSVVNUIE5ldHdvcmsxLjAsBgNVBAMT
JVVTRVJUcnVzdCBFQ0MgQ2VydGlmaWNhdGlvbiBBdXRob3JpdHkwHhcNMTAwMjAx
MDAwMDAwWhcNMzgwMTE4MjM1OTU5WjCBiDELMAkGA1UEBhMCVVMxEzARBgNVBAgT
Ck5ldyBKZXJzZXkxFDASBgNVBAcTC0plcnNleSBDaXR5MR4wHAYDVQQKExVUaGUg
VVNFUlRSVVNUIE5ldHdvcmsxLjAsBgNVBAMTJVVTRVJUcnVzdCBFQ0MgQ2VydGlm
aWNhdGlvbiBBdXRob3JpdHkwdjAQBgcqhkjOPQIBBgUrgQQAIgNiAAQarFRaqflo
I+d61SRvU8Za2EurxtW20eZzca7dnNYMYf3boIkDuAUU7FfO7l0/4iGzzvfUinng
o4N+LZfQYcTxmdwlkWOrfzCjtHDix6EznPO/LlxTsV+zfTJ/ijTjeXmjQjBAMB0G
A1UdDgQWBBQ64QmG1M8ZwpZ2dEl23OA1xmNjmjAOBgNVHQ8BAf8EBAMCAQYwDwYD
VR0TAQH/BAUwAwEB/zAKBggqhkjOPQQDAwNoADBlAjA2Z6EWCNzklwBBHU6+4WMB
zzuqQhFkoJ2UOQIReVx7Hfpkue4WQrO/isIJxOzksU0CMQDpKmFHjFJKS04YcPbW
RNZu9YO6bVi9JNlWSOrvxKJGgYhqOkbRqZtNyWHa0V1Xahg=
-----END CERTIFICATE-----
-----BEGIN CERTIFICATE-----
MIIFazCCA1OgAwIBAgIRAIIQz7DSQONZRGPgu2OCiwAwDQYJKoZIhvcNAQELBQAw
TzELMAkGA1UEBhMCVVMxKTAnBgNVBAoTIEludGVybmV0IFNlY3VyaXR5IFJlc2Vh
cmNoIEdyb3VwMRUwEwYDVQQDEwxJU1JHIFJvb3QgWDEwHhcNMTUwNjA0MTEwNDM4
WhcNMzUwNjA0MTEwNDM4WjBPMQswCQYDVQQGEwJVUzEpMCcGA1UEChMgSW50ZXJu
ZXQgU2VjdXJpdHkgUmVzZWFyY2ggR3JvdXAxFTATBgNVBAMTDElTUkcgUm9vdCBY
MTCCAiIwDQYJKoZIhvcNAQEBBQADggIPADCCAgoCggIBAK3oJHP0FDfzm54rVygc
h77ct984kIxuPOZXoHj3dcKi/vVqbvYATyjb3miGbESTtrFj/RQSa78f0uoxmyF+
0TM8ukj13Xnfs7j/EvEhmkvBioZxaUpmZmyPfjxwv60pIgbz5MDmgK7iS4+3mX6U
A5/TR5d8mUgjU+g4rk8Kb4Mu0UlXjIB0ttov0DiNewNwIRt18jA8+o+u3dpjq+sW
T8KOEUt+zwvo/7V3LvSye0rgTBIlDHCNAymg4VMk7BPZ7hm/ELNKjD+Jo2FR3qyH
B5T0Y3HsLuJvW5iB4YlcNHlsdu87kGJ55tukmi8mxdAQ4Q7e2RCOFvu396j3x+UC
B5iPNgiV5+I3lg02dZ77DnKxHZu8A/lJBdiB3QW0KtZB6awBdpUKD9jf1b0SHzUv
KBds0pjBqAlkd25HN7rOrFleaJ1/ctaJxQZBKT5ZPt0m9STJEadao0xAH0ahmbWn
OlFuhjuefXKnEgV4We0+UXgVCwOPjdAvBbI+e0ocS3MFEvzG6uBQE3xDk3SzynTn
jh8BCNAw1FtxNrQHusEwMFxIt4I7mKZ9YIqioymCzLq9gwQbooMDQaHWBfEbwrbw
qHyGO0aoSCqI3Haadr8faqU9GY/rOPNk3sgrDQoo//fb4hVC1CLQJ13hef4Y53CI
rU7m2Ys6xt0nUW7/vGT1M0NPAgMBAAGjQjBAMA4GA1UdDwEB/wQEAwIBBjAPBgNV
HRMBAf8EBTADAQH/MB0GA1UdDgQWBBR5tFnme7bl5AFzgAiIyBpY9umbbjANBgkq
hkiG9w0BAQsFAAOCAgEAVR9YqbyyqFDQDLHYGmkgJykIrGF1XIpu+ILlaS/V9lZL
ubhzEFnTIZd+50xx+7LSYK05qAvqFyFWhfFQDlnrzuBZ6brJFe+GnY+EgPbk6ZGQ
3BebYhtF8GaV0nxvwuo77x/Py9auJ/GpsMiu/X1+mvoiBOv/2X/qkSsisRcOj/KK
NFtY2PwByVS5uCbMiogziUwthDyC3+6WVwW6LLv3xLfHTjuCvjHIInNzktHCgKQ5
ORAzI4JMPJ+GslWYHb4phowim57iaztXOoJwTdwJx4nLCgdNbOhdjsnvzqvHu7Ur
TkXWStAmzOVyyghqpZXjFaH3pO3JLF+l+/+sKAIuvtd7u+Nxe5AW0wdeRlN8NwdC
jNPElpzVmbUq4JUagEiuTDkHzsxHpFKVK7q4+63SM1N95R1NbdWhscdCb+ZAJzVc
oyi3B43njTOQ5yOf+1CceWxG1bQVs5ZufpsMljq4Ui0/1lvh+wjChP4kqKOJ2qxq
4RgqsahDYVvTH9w7jXbyLeiNdd8XM2w9U/t7y0Ff/9yi0GE44Za4rF2LN9d11TPA
mRGunUHBcnWEvgJBQl9nJEiU0Zsnvgc/ubhPgXRR4Xq37Z0j4r7g1SgEEzwxA57d
emyPxgcYxn/eR44/KJ4EBs+lVDR3veyJm+kXQ99b21/+jh5Xos1AnX5iItreGCc=
-----END CERTIFICATE-----
)EOF";

static constexpr uint32_t kFirstCheckDelayMs = 15000;
static constexpr uint32_t kCheckIntervalMs = 24UL * 60 * 60 * 1000;
static constexpr uint32_t kHttpTimeoutMs = 20000;

static String g_latestVersion;
static String g_assetUrl;
static String g_error;
static size_t g_assetSize = 0;
static bool g_available = false;
static bool g_checkedOnce = false;
static uint32_t g_lastCheck = 0;
static UpdateProgressCallback g_progress = nullptr;

static void report(UpdateStage stage, uint8_t percent) {
  if (g_progress) g_progress(stage, percent);
}

static void setError(const String& message) {
  g_error = message;
  g_available = false;
  g_latestVersion = "";
  g_assetUrl = "";
  logWarn("update", "%s", message.c_str());
}

static long nextVersionPart(const String& text, int& index) {
  while (index < (int)text.length() && !isdigit((unsigned char)text[index])) ++index;
  long value = 0;
  while (index < (int)text.length() && isdigit((unsigned char)text[index])) {
    value = value * 10 + (text[index] - '0');
    ++index;
  }
  return value;
}

static bool versionNewer(const String& latest, const String& current) {
  int li = 0, ci = 0;
  for (int part = 0; part < 3; ++part) {
    const long a = nextVersionPart(latest, li);
    const long b = nextVersionPart(current, ci);
    if (a != b) return a > b;
  }
  return false;
}

class OtaSink : public Stream {
 public:
  explicit OtaSink(size_t expected) : expected_(expected) {}

  size_t write(uint8_t value) override { return write(&value, 1); }

  size_t write(const uint8_t* data, size_t len) override {
    if (!otaWrite(data, len)) {
      ok_ = false;
      return 0;
    }
    written_ += len;
    const uint8_t percent = expected_ ? (uint8_t)min((size_t)99, (written_ * 100) / expected_) : 0;
    if (percent >= lastPercent_ + 2 || percent == 99) {
      lastPercent_ = percent;
      report(UpdateStage::Downloading, percent);
    }
    return len;
  }

  int available() override { return 0; }
  int read() override { return -1; }
  int peek() override { return -1; }
  void flush() override {}
  bool ok() const { return ok_; }

 private:
  size_t expected_;
  size_t written_ = 0;
  uint8_t lastPercent_ = 0;
  bool ok_ = true;
};

void updateBegin() {
  g_latestVersion = "";
  g_assetUrl = "";
  g_error = "";
  g_available = false;
  g_checkedOnce = false;
  g_lastCheck = 0;
}

void updateSetProgressCallback(UpdateProgressCallback callback) { g_progress = callback; }

bool updateAvailable() { return g_available; }
const String& updateLatestVersion() { return g_latestVersion; }
const String& updateLastError() { return g_error; }

const char* updateStateName() {
  if (otaInProgress()) return "downloading";
  if (g_available) return "available";
  if (g_error.length()) return "error";
  return "idle";
}

static bool fetchRelease() {
  if (!wifiConnected()) {
    setError(tr("Wi-Fi is not connected", "Wi-Fi не подключён"));
    return false;
  }

  WiFiClientSecure client;
  client.setCACert(kRootCAs);

  HTTPClient http;
  http.setTimeout(kHttpTimeoutMs);
  http.setConnectTimeout(kHttpTimeoutMs);
  http.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);
  http.setUserAgent(String(AGENT_COMPANION_NAME) + "/" + AGENT_COMPANION_VERSION);
  if (!http.begin(client, kManifestUrl)) {
    setError("Update: begin failed");
    return false;
  }

  const int code = http.GET();
  if (code != HTTP_CODE_OK) {
    setError(String("Update HTTP ") + code);
    http.end();
    return false;
  }

  const String body = http.getString();
  http.end();
  if (!body.length()) {
    setError("Update: empty manifest");
    return false;
  }

  JsonDocument doc;
  const DeserializationError err = deserializeJson(doc, body);
  if (err) {
    setError(String("Update JSON: ") + err.c_str());
    return false;
  }

  String latest = doc["version"] | "";
  latest.trim();
  if (latest.startsWith("v")) latest = latest.substring(1);
  if (!latest.length()) {
    setError("Update: no version");
    return false;
  }

  const JsonObject asset = doc["assets"][AGENTFACE_RELEASE_ENV];
  const String file = asset["file"] | "";
  const size_t size = asset["size"] | 0;
  if (!file.length()) {
    setError(String("No asset for ") + AGENTFACE_RELEASE_ENV);
    return false;
  }

  g_latestVersion = latest;
  g_assetUrl = String(kReleaseBase) + latest + "/" + file;
  g_assetSize = size;
  g_error = "";
  g_available = versionNewer(latest, AGENT_COMPANION_VERSION);
  logInfo("update", "latest=%s current=%s asset=%u bytes%s", latest.c_str(),
          AGENT_COMPANION_VERSION, (unsigned)size, g_available ? " (update available)" : "");
  return true;
}

bool updateCheckNow() {
  if (otaInProgress()) return false;
  report(UpdateStage::Checking, 0);
  const bool ok = fetchRelease();
  g_checkedOnce = true;
  g_lastCheck = millis();
  return ok;
}

bool updateInstallNow() {
  if (otaInProgress()) {
    g_error = "another update is running";
    return false;
  }
  if (!g_available || !g_assetUrl.length()) {
    if (!updateCheckNow()) return false;
    if (!g_available) {
      g_error = tr("Already up to date", "Уже установлена последняя версия");
      return false;
    }
  }

  WiFiClientSecure client;
  client.setCACert(kRootCAs);

  HTTPClient http;
  http.setTimeout(30000);
  http.setConnectTimeout(kHttpTimeoutMs);
  http.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);
  http.setUserAgent(String(AGENT_COMPANION_NAME) + "/" + AGENT_COMPANION_VERSION);
  if (!http.begin(client, g_assetUrl)) {
    setError("Update: begin failed");
    return false;
  }

  const int code = http.GET();
  if (code != HTTP_CODE_OK) {
    setError(String("Update HTTP ") + code);
    http.end();
    return false;
  }

  const int contentLength = http.getSize();
  const size_t expected = contentLength > 0 ? (size_t)contentLength : g_assetSize;
  logInfo("update", "downloading %s (%u bytes)", g_assetUrl.c_str(), (unsigned)expected);

  if (!otaStart(expected, "")) {
    g_error = otaLastError();
    http.end();
    return false;
  }

  OtaSink sink(expected);
  const int written = http.writeToStream(&sink);
  http.end();

  const bool complete = written >= 0 && (expected == 0 || (size_t)written == expected) && sink.ok();
  if (!complete) {
    otaAbort();
    g_error = written < 0 ? String("Update: download failed") : String("Update: incomplete image");
    logWarn("update", "%s (%d/%u)", g_error.c_str(), written, (unsigned)expected);
    return false;
  }
  if (!otaFinish()) {
    g_error = otaLastError();
    return false;
  }

  g_available = false;
  report(UpdateStage::Downloading, 100);
  logInfo("update", "downloaded %s, restart required", g_latestVersion.c_str());
  return true;
}

void updateLoop() {
  if (!settings().autoUpdate || !wifiConnected() || otaInProgress()) return;
  if (millis() < kFirstCheckDelayMs) return;
  if (g_checkedOnce && millis() - g_lastCheck < kCheckIntervalMs) return;
  updateCheckNow();
}
