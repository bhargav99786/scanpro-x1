#pragma once
#include "esp_err.h"
#include "driver/i2c_master.h"
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

esp_err_t esp_es8311_port_init(i2c_master_bus_handle_t i2c_bus);
void audio_play_beep(uint16_t freq_hz, uint16_t duration_ms);
void beepSuccess();
void beepError();
void beepStartup();
int esp_es8311_play(const uint8_t *buf, size_t len);
int esp_es8311_record(uint8_t *buf, size_t len);

#ifdef __cplusplus
}
#endif
