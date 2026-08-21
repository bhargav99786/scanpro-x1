#pragma once

#include <stdio.h>
#include "driver/i2c_master.h"

void esp_es8311_port_init(i2c_master_bus_handle_t bus_handle);
void esp_es8311_test(void);
esp_err_t esp_es8311_play(const uint8_t *data, size_t len);
int esp_es8311_record(uint8_t *data, size_t len);

