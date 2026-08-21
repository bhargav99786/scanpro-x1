#pragma once

#include "esp_err.h"
#include <stdint.h>

#include "driver/i2c_master.h"

#ifdef __cplusplus
extern "C" {
#endif

extern i2c_master_bus_handle_t i2c_bus_handle;

// Initialize the I2C bus, IO Expander (TCA9554), SPI bus, LCD (ST7796), Touch (FT6336), and LVGL
esp_err_t display_port_init(void);

// Set screen rotation (0-3)
void display_port_set_rotation(uint8_t rotation);

#ifdef __cplusplus
}
#endif
