#include "gm65_scanner.h"
#include "pins_config.h"
#include "driver/uart.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char *TAG = "GM65_SCANNER";
static const uart_port_t UART_NUM = UART_NUM_1;
static gm65_scan_callback_t scan_cb = NULL;

static void gm65_rx_task(void *pvParameters)
{
    uint8_t dtmp[256];
    std::string scan_buf;

    while (1) {
        int len = uart_read_bytes(UART_NUM, dtmp, sizeof(dtmp) - 1, pdMS_TO_TICKS(50));
        if (len > 0) {
            dtmp[len] = '\0';
            for (int i = 0; i < len; i++) {
                char c = (char)dtmp[i];
                if (c == '\r' || c == '\n') {
                    if (!scan_buf.empty()) {
                        ESP_LOGI(TAG, "Barcode Scanned: %s", scan_buf.c_str());
                        if (scan_cb) {
                            scan_cb(scan_buf);
                        }
                        scan_buf.clear();
                    }
                } else if (c >= 32 && c <= 126) {
                    scan_buf += c;
                }
            }
        }
    }
}

esp_err_t gm65_scanner_init(gm65_scan_callback_t callback)
{
    scan_cb = callback;

    uart_config_t uart_config = {
        .baud_rate = GM65_BAUD_RATE,
        .data_bits = UART_DATA_8_BITS,
        .parity    = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
        .source_clk = UART_SCLK_DEFAULT,
    };

    ESP_ERROR_CHECK(uart_driver_install(UART_NUM, 1024 * 2, 0, 0, NULL, 0));
    ESP_ERROR_CHECK(uart_param_config(UART_NUM, &uart_config));
    ESP_ERROR_CHECK(uart_set_pin(UART_NUM, GM65_TX_PIN, GM65_RX_PIN, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE));

    xTaskCreatePinnedToCore(gm65_rx_task, "gm65_rx_task", 4096, NULL, 4, NULL, 1);
    ESP_LOGI(TAG, "GM65 scanner UART1 initialized on RX GPIO %d, TX GPIO %d", GM65_RX_PIN, GM65_TX_PIN);

    return ESP_OK;
}
