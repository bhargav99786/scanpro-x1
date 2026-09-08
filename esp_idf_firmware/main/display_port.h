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

// Set screen rotation (0-3) when NOT in LVGL context
void display_port_set_rotation(uint8_t rotation);

// Set screen rotation (0-3) when inside LVGL callback (lock already held)
void display_port_set_rotation_locked(uint8_t rotation);

// Load screen rotation from NVS and apply it
void display_port_load_rotation();

// Set display backlight brightness (0-100%)
void display_port_set_backlight(uint8_t brightness);

#ifdef __cplusplus
}
#endif
