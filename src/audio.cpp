#include <Arduino.h>
#include <math.h>
#include "audio.h"
#include "config.h"
#include "hardware_profile.h"
#include "locale.h"

static bool g_enabled = VOICE_ENABLED_BY_DEFAULT;

#if defined(PROFILE_M5STACK_BASIC)

#include <M5Unified.h>
#include "voice_data.h"

static bool g_speakerAwake = false;
static bool g_shutdownRequested = false;
static uint32_t g_quietSince = 0;

static uint8_t safeVolume() {
  return SPEAKER_VOLUME > 80 ? 80 : SPEAKER_VOLUME;
}

static bool speakerWake() {
  if (g_speakerAwake) return true;
  if (!M5.Speaker.begin()) {
    Serial.println("[audio] Speaker.begin failed");
    return false;
  }
  M5.Speaker.setVolume(safeVolume());
  g_speakerAwake = true;
  g_shutdownRequested = false;
  g_quietSince = 0;
  return true;
}

static void requestSpeakerSleep() {
  if (!g_speakerAwake) return;
  g_shutdownRequested = true;
  g_quietSince = 0;
}

static void m5Tone(int frequency, int durationMs) {
  if (!g_enabled || !speakerWake()) return;
  M5.Speaker.tone(frequency, durationMs);
  delay(durationMs + 12);
}

static void eventTone(AgentState state) {
  if (!g_enabled) return;
  switch (state) {
    case AgentState::Welcome: m5Tone(900, 40); m5Tone(1350, 60); break;
    case AgentState::Done: m5Tone(950, 45); m5Tone(1350, 45); m5Tone(1800, 70); break;
    case AgentState::Attention: m5Tone(1650, 65); m5Tone(1650, 65); break;
    case AgentState::Error: m5Tone(700, 80); m5Tone(430, 120); break;
    case AgentState::Abort: m5Tone(650, 60); m5Tone(520, 90); break;
    default: break;
  }
  requestSpeakerSleep();
}

void audioBegin() {
  M5.Speaker.end();
  g_speakerAwake = false;
  g_shutdownRequested = false;
  g_quietSince = 0;
}

void audioLoop() {
  if (!g_speakerAwake) return;
  if (M5.Speaker.isPlaying()) {
    g_quietSince = 0;
    return;
  }
  if (!g_shutdownRequested) return;
  if (g_quietSince == 0) {
    g_quietSince = millis();
    return;
  }
  if (millis() - g_quietSince >= 1000) {
    M5.Speaker.end();
    g_speakerAwake = false;
    g_shutdownRequested = false;
    g_quietSince = 0;
  }
}

void audioSetEnabled(bool enabled) {
  g_enabled = enabled;
  if (!g_enabled && g_speakerAwake) {
    M5.Speaker.stop();
    requestSpeakerSleep();
  }
}

bool audioIsEnabled() { return g_enabled; }

void audioToggle() {
  g_enabled = !g_enabled;
  if (!g_enabled) {
    if (g_speakerAwake) {
      M5.Speaker.stop();
      requestSpeakerSleep();
    }
    return;
  }
  m5Tone(1000, 45);
  m5Tone(1450, 65);
  requestSpeakerSleep();
}

void audioButtonClick() {
  if (!g_enabled) return;
  m5Tone(1700, 22);
  requestSpeakerSleep();
}

static void play(const uint8_t* data, size_t len) {
  if (!g_enabled || !speakerWake()) return;
  if (!M5.Speaker.playWav(data, len, 1, -1, true)) {
    Serial.println("[audio] playWav rejected");
  }
  requestSpeakerSleep();
}

void audioForState(AgentState state) {
  // The bundled speech pack is Russian. With an English UI we intentionally
  // use short event tones instead of speaking the wrong language.
  if (!localeIsRussian()) {
    eventTone(state);
    return;
  }
  switch (state) {
    case AgentState::Welcome:   play(voice_welcome, voice_welcome_len); break;
    case AgentState::Done:      play(voice_done, voice_done_len); break;
    case AgentState::Attention: play(voice_attention, voice_attention_len); break;
    case AgentState::Error:     play(voice_error, voice_error_len); break;
    case AgentState::Abort:     play(voice_abort, voice_abort_len); break;
    default: break;
  }
}

#else // generic ESP32 audio backends

#if AGENT_AUDIO_BACKEND == AGENT_AUDIO_MAX98357
  #include <driver/i2s.h>
  static bool g_i2sReady = false;

  static bool ensureI2s() {
    if (g_i2sReady) return true;
    i2s_config_t cfg = {};
    cfg.mode = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_TX);
    cfg.sample_rate = 16000;
    cfg.bits_per_sample = I2S_BITS_PER_SAMPLE_16BIT;
    cfg.channel_format = I2S_CHANNEL_FMT_ONLY_LEFT;
    cfg.communication_format = I2S_COMM_FORMAT_STAND_I2S;
    cfg.intr_alloc_flags = ESP_INTR_FLAG_LEVEL1;
    cfg.dma_buf_count = 4;
    cfg.dma_buf_len = 128;
    cfg.use_apll = false;
    cfg.tx_desc_auto_clear = true;
    cfg.fixed_mclk = 0;
    if (i2s_driver_install(I2S_NUM_0, &cfg, 0, nullptr) != ESP_OK) return false;
    i2s_pin_config_t pins = {};
    pins.bck_io_num = I2S_BCLK_PIN;
    pins.ws_io_num = I2S_LRCLK_PIN;
    pins.data_out_num = I2S_DATA_PIN;
    pins.data_in_num = I2S_PIN_NO_CHANGE;
    if (i2s_set_pin(I2S_NUM_0, &pins) != ESP_OK) return false;
    i2s_zero_dma_buffer(I2S_NUM_0);
    g_i2sReady = true;
    return true;
  }

  static void genericTone(int frequency, int durationMs) {
    if (!g_enabled || !ensureI2s()) return;
    constexpr int sampleRate = 16000;
    constexpr int chunk = 128;
    int16_t buf[chunk];
    int remaining = sampleRate * durationMs / 1000;
    int phase = 0;
    while (remaining > 0) {
      int n = remaining > chunk ? chunk : remaining;
      for (int i = 0; i < n; ++i) {
        float a = 2.0f * PI * frequency * phase / sampleRate;
        buf[i] = (int16_t)(sinf(a) * 5200.0f);
        ++phase;
      }
      size_t written = 0;
      i2s_write(I2S_NUM_0, buf, n * sizeof(int16_t), &written, portMAX_DELAY);
      remaining -= n;
    }
    i2s_zero_dma_buffer(I2S_NUM_0);
  }

#elif AGENT_AUDIO_BACKEND == AGENT_AUDIO_PIEZO

  static void genericTone(int frequency, int durationMs) {
    if (!g_enabled || PIEZO_PIN < 0) return;
    tone(PIEZO_PIN, frequency, durationMs);
    delay(durationMs + 10);
    noTone(PIEZO_PIN);
  }

#else

  static void genericTone(int frequency, int durationMs) {
    (void)frequency; (void)durationMs;
  }

#endif

static void genericEvent(AgentState state) {
  switch (state) {
    case AgentState::Welcome: genericTone(900, 40); genericTone(1350, 60); break;
    case AgentState::Done: genericTone(950, 45); genericTone(1350, 45); genericTone(1800, 70); break;
    case AgentState::Attention: genericTone(1650, 70); genericTone(1650, 70); break;
    case AgentState::Error: genericTone(700, 90); genericTone(430, 120); break;
    case AgentState::Abort: genericTone(650, 60); genericTone(520, 90); break;
    default: break;
  }
}

void audioBegin() {
#if AGENT_AUDIO_BACKEND == AGENT_AUDIO_PIEZO
  if (PIEZO_PIN >= 0) pinMode(PIEZO_PIN, OUTPUT);
#endif
}
void audioLoop() {}
void audioSetEnabled(bool enabled) { g_enabled = enabled; }
bool audioIsEnabled() { return g_enabled; }
void audioToggle() {
  g_enabled = !g_enabled;
  if (g_enabled) { genericTone(1000, 40); genericTone(1450, 60); }
}
void audioButtonClick() { genericTone(1700, 20); }
void audioForState(AgentState state) { if (g_enabled) genericEvent(state); }

#endif
