#pragma once

#include "esp_err.h"
#include <string>

typedef void (*gm65_scan_callback_t)(const std::string &sku);

/**
 * @brief Initialize GM65 barcode scanner on UART1 (GPIO 10 RX, GPIO 11 TX)
 */
esp_err_t gm65_scanner_init(gm65_scan_callback_t callback);
