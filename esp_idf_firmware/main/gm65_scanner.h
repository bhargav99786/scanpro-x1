#pragma once

#include "esp_err.h"
#include <string>

typedef void (*gm65_scan_callback_t)(const std::string &sku);

/**
 * @brief Initialize GM65 barcode scanner on UART1 (GPIO 10 RX, GPIO 11 TX)
 */
esp_err_t gm65_scanner_init(gm65_scan_callback_t callback);

void gm65_set_lighting(int percent);
void gm65_set_collimation(int percent);
void gm65_trigger_scan(void);
void gm65_factory_reset(void);
void gm65_set_1d(bool enable);
void gm65_set_2d(bool enable);
void gm65_load_settings(void);

extern int gm65_light_percent;
extern int gm65_laser_percent;
extern bool gm65_1d_enabled;
extern bool gm65_2d_enabled;
