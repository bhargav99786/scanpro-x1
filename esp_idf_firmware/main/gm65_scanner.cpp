#include "gm65_scanner.h"
#include "pins_config.h"
#include "driver/uart.h"
#include "esp_log.h"
#include "esp_es8311_port.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include <string.h>
#include "nvs_flash.h"
#include "nvs.h"

static const char *TAG = "GM65_SCANNER";

int gm65_light_percent = 100;
int gm65_laser_percent = 100;
bool gm65_1d_enabled = true;
bool gm65_2d_enabled = true;

static const uart_port_t UART_NUM = UART_NUM_1;
static gm65_scan_callback_t scan_cb = NULL;
static SemaphoreHandle_t gm65_tx_mutex = NULL;

// Forward declarations
static void gm65_save_to_eeprom(void);
static void gm65_write_register(uint16_t addr, uint8_t value);
static void gm65_apply_symbologies(bool en_1d, bool en_2d);

static void gm65_save_settings() {
    nvs_handle_t handle;
    if (nvs_open("scanner_cfg", NVS_READWRITE, &handle) == ESP_OK) {
        nvs_set_i32(handle, "light", gm65_light_percent);
        nvs_set_i32(handle, "laser", gm65_laser_percent);
        nvs_set_u8(handle, "1d_en", gm65_1d_enabled ? 1 : 0);
        nvs_set_u8(handle, "2d_en", gm65_2d_enabled ? 1 : 0);
        nvs_commit(handle);
        nvs_close(handle);
    }
}

// GM65 Symbology Code ID classifier (from GM65 User Manual Appendix B)
static bool is_gm65_1d_code_id(char c) {
    return (c == 'd' || c == 'c' || c == 'j' || c == 'b' || 
            c == 'i' || c == 'a' || c == 'e' || c == 'D' || 
            c == 'v' || c == 'H' || c == 'm' || c == 'R');
}

static bool is_gm65_2d_code_id(char c) {
    return (c == 'Q' || c == 'u' || c == 'r');
}

static void gm65_rx_task(void *pvParameters)
{
    uint8_t dtmp[256];
    std::string scan_buf;

    enum AckState {
        ACK_IDLE,
        ACK_SAW_02,
        ACK_SKIPPING
    };
    AckState ack_state = ACK_IDLE;
    int ack_bytes_left = 0;
    TickType_t last_char_tick = 0;

    while (1) {
        int len = uart_read_bytes(UART_NUM, dtmp, sizeof(dtmp) - 1, pdMS_TO_TICKS(50));
        TickType_t now = xTaskGetTickCount();

        // Clear scan_buf if it has been idle for > 400ms without newline
        if (!scan_buf.empty() && (now - last_char_tick > pdMS_TO_TICKS(400))) {
            ESP_LOGW(TAG, "Discarding stale/incomplete scan_buf (%d chars)", (int)scan_buf.length());
            scan_buf.clear();
        }

        if (len > 0) {
            for (int i = 0; i < len; i++) {
                uint8_t b = dtmp[i];

                // State machine to completely filter out GM65 command response / ACK packets:
                // GM65 ACK format: 0x02 0x00 0x00 0x01 0x00 <crc_h> <crc_l> (7 bytes total)
                if (ack_state == ACK_SKIPPING) {
                    ack_bytes_left--;
                    if (ack_bytes_left <= 0) {
                        ack_state = ACK_IDLE;
                    }
                    continue;
                }

                if (ack_state == ACK_SAW_02) {
                    if (b == 0x00) {
                        // Confirmed GM65 command response packet! Skip the remaining 5 bytes
                        ack_state = ACK_SKIPPING;
                        ack_bytes_left = 5;
                        continue;
                    } else {
                        // Not an ACK frame, reset to idle and process byte b
                        ack_state = ACK_IDLE;
                    }
                }

                if (b == 0x02) {
                    ack_state = ACK_SAW_02;
                    continue;
                }

                char c = (char)b;
                if (c == '\r' || c == '\n') {
                    if (!scan_buf.empty()) {
                        bool is_1d = false;
                        bool is_2d = false;
                        std::string clean_sku = scan_buf;

                        if (scan_buf.length() >= 2) {
                            char prefix = scan_buf[0];
                            if (is_gm65_2d_code_id(prefix)) {
                                is_2d = true;
                                clean_sku = scan_buf.substr(1);
                            } else if (is_gm65_1d_code_id(prefix)) {
                                is_1d = true;
                                clean_sku = scan_buf.substr(1);
                            }
                        }

                        // Strictly enforce toggles
                        if (is_1d && !gm65_1d_enabled) {
                            ESP_LOGW(TAG, "BLOCKED 1D barcode scan: '%s' (1D Scan toggle is OFF)", clean_sku.c_str());
                            beepError();
                            scan_buf.clear();
                            continue;
                        }

                        if (is_2d && !gm65_2d_enabled) {
                            ESP_LOGW(TAG, "BLOCKED 2D barcode scan: '%s' (2D Scan toggle is OFF)", clean_sku.c_str());
                            beepError();
                            scan_buf.clear();
                            continue;
                        }

                        ESP_LOGI(TAG, "Barcode Scanned (%s): %s", is_1d ? "1D" : (is_2d ? "2D" : "Raw"), clean_sku.c_str());
                        if (scan_cb) {
                            scan_cb(clean_sku);
                        }
                        scan_buf.clear();
                    }
                } else if (b >= 32 && b <= 126) {
                    scan_buf += c;
                    last_char_tick = now;
                }
            }
        }
    }
}

esp_err_t gm65_scanner_init(gm65_scan_callback_t callback)
{
    scan_cb = callback;
    gm65_tx_mutex = xSemaphoreCreateMutex();

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

    // Load and apply saved hardware settings (Light, Laser, 1D, 2D) on boot
    gm65_load_settings();

    return ESP_OK;
}

// ── GM65 Write Helpers ───────────────────────────────────────────────────────

// Generic GM65 config write: 7E 00 08 01 <addrH> <addrL> <len> <data> <CRCH> <CRCL>
// The GM65 protocol CRC-16 covers all bytes from 0x00 onward (index 1 to n-2).
static uint16_t gm65_crc16(const uint8_t *data, size_t len)
{
    uint16_t crc = 0x0000;
    for (size_t i = 0; i < len; i++) {
        crc ^= (uint16_t)data[i] << 8;
        for (int j = 0; j < 8; j++)
            crc = (crc & 0x8000) ? (crc << 1) ^ 0x1021 : (crc << 1);
    }
    return crc;
}

static void gm65_write_register(uint16_t addr, uint8_t value)
{
    uint8_t body[] = {0x00, 0x08, 0x01,
                      (uint8_t)(addr >> 8), (uint8_t)(addr & 0xFF),
                      value};
    uint16_t crc = gm65_crc16(body, sizeof(body));

    uint8_t frame[sizeof(body) + 3];
    frame[0] = 0x7E;
    memcpy(&frame[1], body, sizeof(body));
    frame[sizeof(body) + 1] = (uint8_t)(crc >> 8);
    frame[sizeof(body) + 2] = (uint8_t)(crc & 0xFF);

    if (gm65_tx_mutex) xSemaphoreTake(gm65_tx_mutex, portMAX_DELAY);
    uart_write_bytes(UART_NUM, (const char *)frame, sizeof(frame));
    if (gm65_tx_mutex) xSemaphoreGive(gm65_tx_mutex);

    ESP_LOGI(TAG, "UART TX: reg=0x%04X val=0x%02X (CRC=0x%04X)", addr, value, crc);
}

static void gm65_save_to_eeprom(void)
{
    // Save settlements to EEPROM: 7E 00 09 01 00 00 00 DE C8
    uint8_t body[] = {0x00, 0x09, 0x01, 0x00, 0x00, 0x00};
    uint16_t crc = gm65_crc16(body, sizeof(body)); // 0xDEC8
    uint8_t frame[sizeof(body) + 3];
    frame[0] = 0x7E;
    memcpy(&frame[1], body, sizeof(body));
    frame[sizeof(body) + 1] = (uint8_t)(crc >> 8);
    frame[sizeof(body) + 2] = (uint8_t)(crc & 0xFF);

    if (gm65_tx_mutex) xSemaphoreTake(gm65_tx_mutex, portMAX_DELAY);
    uart_write_bytes(UART_NUM, (const char *)frame, sizeof(frame));
    if (gm65_tx_mutex) xSemaphoreGive(gm65_tx_mutex);

    vTaskDelay(pdMS_TO_TICKS(50));
    ESP_LOGI(TAG, "UART: Sent Save to EEPROM (0x%04X)", crc);
}

// Zone Bit 0x0000 controls multiple settings at once:
// Bit 7: LED indicator (1=Open)
// Bit 6: Mute status (1=Buzzer On)
// Bits 5-4: Aiming light mode (00=Off, 01=Normal, 10=Always On)
// Bits 3-2: Lighting mode (00=Off, 01=Normal, 10=Always On)
static uint8_t gm65_zone00 = 0xD7; // Default: 1101 0111 (Buzzer/LED on, Standard aim/light, Sensor Mode)

void gm65_set_lighting(int percent)
{
    uint8_t bits = (percent == 0) ? 0x00 : (percent == 100) ? 0x08 : 0x04;
    gm65_zone00 = (gm65_zone00 & ~0x0C) | bits;
    gm65_write_register(0x0000, gm65_zone00);
    gm65_light_percent = percent;
    gm65_save_settings();
    ESP_LOGI(TAG, "Lighting: %d%% → zone00=0x%02X", percent, gm65_zone00);
}

void gm65_set_collimation(int percent)
{
    uint8_t bits = (percent == 0) ? 0x00 : (percent == 100) ? 0x20 : 0x10;
    gm65_zone00 = (gm65_zone00 & ~0x30) | bits;
    gm65_write_register(0x0000, gm65_zone00);
    gm65_laser_percent = percent;
    gm65_save_settings();
    ESP_LOGI(TAG, "Collimation: %d%% → zone00=0x%02X", percent, gm65_zone00);
}

void gm65_trigger_scan(void)
{
    uint8_t body[] = {0x00, 0x08, 0x01, 0x00, 0x02, 0x01};
    uint16_t crc = gm65_crc16(body, sizeof(body));
    uint8_t frame[sizeof(body) + 3];
    frame[0] = 0x7E;
    memcpy(&frame[1], body, sizeof(body));
    frame[sizeof(body) + 1] = (uint8_t)(crc >> 8);
    frame[sizeof(body) + 2] = (uint8_t)(crc & 0xFF);

    if (gm65_tx_mutex) xSemaphoreTake(gm65_tx_mutex, portMAX_DELAY);
    uart_write_bytes(UART_NUM, (const char *)frame, sizeof(frame));
    if (gm65_tx_mutex) xSemaphoreGive(gm65_tx_mutex);

    ESP_LOGI(TAG, "UART: Sent trigger-scan pulse (diagnostic)");
}

void gm65_factory_reset(void)
{
    gm65_write_register(0x00D9, 0x55);
    vTaskDelay(pdMS_TO_TICKS(500));
    
    gm65_zone00 = 0xD7; 
    gm65_write_register(0x0000, gm65_zone00);
    vTaskDelay(pdMS_TO_TICKS(10));
    
    gm65_write_register(0x000D, 0x00);
    vTaskDelay(pdMS_TO_TICKS(10));
    
    gm65_write_register(0x0060, 0x05); // Code ID prefix + CR tail
    vTaskDelay(pdMS_TO_TICKS(10));

    // Enable all symbologies
    gm65_apply_symbologies(true, true);

    ESP_LOGI(TAG, "UART: Sent Factory Reset + Sensor Mode + Serial Mode + Code ID Prefix + BRUTE ENABLE ALL");
}

static const uint16_t gm65_1d_regs[] = {
    0x002E, 0x002F, 0x0030, 0x0031, 0x0032, 0x0033, 0x0036, 0x0039, 0x003C, 
    0x0040, 0x0043, 0x0046, 0x0049, 0x004C, 0x004F, 0x0050, 0x0051
};

static const uint16_t gm65_2d_regs[] = {
    0x003F, 0x0054, 0x0055
};

static TaskHandle_t gm65_cfg_task_handle = NULL;

static void gm65_apply_symbologies(bool en_1d, bool en_2d)
{
    xTaskCreate([](void *arg) {
        bool en1 = gm65_1d_enabled;
        bool en2 = gm65_2d_enabled;

        // 1. Ensure Code ID prefix + CR tail enabled: 0x0060 = 0x05
        gm65_write_register(0x0060, 0x05);
        vTaskDelay(pdMS_TO_TICKS(20));

        // 2. Global barcode switch: 0x002C (0x00 = forbid all, 0x01 = allow according to symbologies)
        if (!en1 && !en2) {
            gm65_write_register(0x002C, 0x00);
            vTaskDelay(pdMS_TO_TICKS(20));
        } else {
            gm65_write_register(0x002C, 0x01);
            vTaskDelay(pdMS_TO_TICKS(20));
        }

        // 3. 2D symbologies: QR (0x003F), DM (0x0054), PDF417 (0x0055)
        uint8_t val2 = en2 ? 0x01 : 0x00;
        for (size_t i = 0; i < sizeof(gm65_2d_regs) / sizeof(gm65_2d_regs[0]); i++) {
            gm65_write_register(gm65_2d_regs[i], val2);
            vTaskDelay(pdMS_TO_TICKS(20));
        }

        // 4. 1D symbologies
        uint8_t val1 = en1 ? 0x01 : 0x00;
        for (size_t i = 0; i < sizeof(gm65_1d_regs) / sizeof(gm65_1d_regs[0]); i++) {
            gm65_write_register(gm65_1d_regs[i], val1);
            vTaskDelay(pdMS_TO_TICKS(20));
        }

        // 5. Commit to EEPROM
        gm65_save_to_eeprom();
        ESP_LOGI(TAG, "GM65 hardware symbologies applied & saved: 1D=%s, 2D=%s", 
                 en1 ? "ON" : "OFF", en2 ? "ON" : "OFF");

        gm65_cfg_task_handle = NULL;
        vTaskDelete(NULL);
    }, "gm65_cfg_task", 3072, NULL, 3, &gm65_cfg_task_handle);
}

void gm65_set_1d(bool enable)
{
    gm65_1d_enabled = enable;
    gm65_save_settings();
    ESP_LOGI(TAG, "1D Scanning state updated: %s", enable ? "Enabled" : "Disabled");
    gm65_apply_symbologies(gm65_1d_enabled, gm65_2d_enabled);
}

void gm65_set_2d(bool enable)
{
    gm65_2d_enabled = enable;
    gm65_save_settings();
    ESP_LOGI(TAG, "2D Scanning state updated: %s", enable ? "Enabled" : "Disabled");
    gm65_apply_symbologies(gm65_1d_enabled, gm65_2d_enabled);
}

void gm65_load_settings(void) {
    nvs_handle_t handle;
    if (nvs_open("scanner_cfg", NVS_READONLY, &handle) == ESP_OK) {
        int32_t val;
        uint8_t bval;
        if (nvs_get_i32(handle, "light", &val) == ESP_OK) gm65_light_percent = val;
        if (nvs_get_i32(handle, "laser", &val) == ESP_OK) gm65_laser_percent = val;
        if (nvs_get_u8(handle, "1d_en", &bval) == ESP_OK) gm65_1d_enabled = (bval != 0);
        if (nvs_get_u8(handle, "2d_en", &bval) == ESP_OK) gm65_2d_enabled = (bval != 0);
        nvs_close(handle);
        ESP_LOGI(TAG, "Scanner settings loaded from NVS: Light=%d, Laser=%d, 1D=%d, 2D=%d", 
                 gm65_light_percent, gm65_laser_percent, (int)gm65_1d_enabled, (int)gm65_2d_enabled);
    }

    gm65_set_lighting(gm65_light_percent);
    gm65_set_collimation(gm65_laser_percent);
    gm65_apply_symbologies(gm65_1d_enabled, gm65_2d_enabled);
}
