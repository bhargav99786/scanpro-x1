/*
  ============================================================================
  audio.h — ES8311 codec + I2S beep feedback
  ----------------------------------------------------------------------------
  Pins come from pins_config.h:
    I2S_MCK_PIN, I2S_BCK_PIN, I2S_LRCK_PIN, I2S_DOUT_PIN, I2S_DIN_PIN
    AUDIO_SAMPLE_RATE, AUDIO_MCLK_MULT, AUDIO_VOLUME
  I2C is the shared bus already started by display_touch.h (Wire.begin).

  Public API:
    bool audioInit()        — call once in setup() AFTER Wire.begin()
    void beepSuccess()      — short high-pitched chirp on successful scan
    void beepStartup()      — ascending triple chirp on boot
  ============================================================================
*/
#pragma once
#include <ESP_I2S.h>
#include <Wire.h>
#include "es8311.h"
#include "pins_config.h"
#include <math.h>

// ── Private constants ────────────────────────────────────────────────────────
#define _AUDIO_MCLK_HZ   (AUDIO_SAMPLE_RATE * AUDIO_MCLK_MULT)
#define _AUDIO_AMPLITUDE  20000   // 16-bit PCM peak (max 32767)
#define _AUDIO_CHANNELS   2

// ── Module-private objects ───────────────────────────────────────────────────
static I2SClass        _i2s;
static es8311_handle_t _es_handle = NULL;
static bool            _audio_ready = false;

// ── Low-level tone generator ─────────────────────────────────────────────────
static void _playBeep(float freq_hz, uint32_t duration_ms)
{
    if (!_audio_ready) return;

    const uint32_t num_samples = (AUDIO_SAMPLE_RATE * duration_ms) / 1000;
    const size_t   buf_bytes   = num_samples * _AUDIO_CHANNELS * sizeof(int16_t);

    int16_t *buf = (int16_t *)malloc(buf_bytes);
    if (!buf) {
        Serial.printf("[AUDIO] malloc failed (%u B)\n", (unsigned)buf_bytes);
        return;
    }

    const uint32_t fade = AUDIO_SAMPLE_RATE / 100;  // 10 ms fade in/out

    for (uint32_t i = 0; i < num_samples; i++) {
        int16_t s = (int16_t)(_AUDIO_AMPLITUDE *
                               sinf(2.0f * M_PI * freq_hz * i / AUDIO_SAMPLE_RATE));
        if      (i < fade)                   s = (int16_t)((float)s * i / fade);
        else if (i > num_samples - fade)     s = (int16_t)((float)s * (num_samples - i) / fade);
        buf[i * 2 + 0] = s;  // L
        buf[i * 2 + 1] = s;  // R
    }

    _i2s.write((uint8_t *)buf, buf_bytes);
    free(buf);
}

// ═════════════════════════════════════════════════════════════════════════════
//  Public API
// ═════════════════════════════════════════════════════════════════════════════

// Call once in setup() — Wire must already be started before this.
inline bool audioInit()
{
    // ── ES8311 codec ──────────────────────────────────────────────────────────
    _es_handle = es8311_create(I2C_NUM_0, ES8311_ADDRRES_0);
    if (!_es_handle) {
        Serial.println("[AUDIO] ES8311 create failed — check I2C wiring");
        return false;
    }

    const es8311_clock_config_t clk = {
        .mclk_inverted      = false,
        .sclk_inverted      = false,
        .mclk_from_mclk_pin = true,
        .mclk_frequency     = _AUDIO_MCLK_HZ,
        .sample_frequency   = AUDIO_SAMPLE_RATE
    };

    if (es8311_init(_es_handle, &clk,
                    ES8311_RESOLUTION_16, ES8311_RESOLUTION_16) != ESP_OK) {
        Serial.println("[AUDIO] ES8311 init failed");
        return false;
    }
    es8311_voice_volume_set(_es_handle, AUDIO_VOLUME, NULL);
    es8311_microphone_config(_es_handle, false);
    Serial.printf("[AUDIO] ES8311 OK — volume %d%%\n", AUDIO_VOLUME);

    // ── I2S ──────────────────────────────────────────────────────────────────
    _i2s.setPins(I2S_BCK_PIN, I2S_LRCK_PIN, I2S_DOUT_PIN, I2S_DIN_PIN, I2S_MCK_PIN);

    if (!_i2s.begin(I2S_MODE_STD,
                    AUDIO_SAMPLE_RATE,
                    I2S_DATA_BIT_WIDTH_16BIT,
                    I2S_SLOT_MODE_STEREO,
                    I2S_STD_SLOT_BOTH)) {
        Serial.println("[AUDIO] I2S begin failed");
        return false;
    }
    Serial.println("[AUDIO] I2S OK");

    _audio_ready = true;
    return true;
}

// Short rising beep — played immediately on every successful barcode scan
inline void beepSuccess()
{
    _playBeep(2500, 50);
}

// Ascending triple chirp played once at the end of setup()
inline void beepStartup()
{
    _playBeep(800,  60); delay(40);
    _playBeep(1200, 60); delay(40);
    _playBeep(1600, 80);
}
