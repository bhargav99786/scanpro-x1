#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_system.h"
#include "esp_log.h"
#include "pins_config.h"
#include "esp_es8311_port.h"
#include "gm65_scanner.h"
#include "network_mqtt.h"
#include "esp_axp2101_port.h"
#include "driver/i2c.h"

#include "display_port.h"
#include "esp_lvgl_port.h"
#include "ui_screens.h"

static const char *TAG = "MAIN_APP";

// Stubs for UI logic
volatile bool is_ptt_pressed = false;
void devicePowerOff() {
    ESP_LOGI(TAG, "Device power off requested.");
}

// Callback executed whenever a barcode scan is decoded by GM65 scanner
static void on_barcode_scanned(const std::string &sku)
{
    ESP_LOGI(TAG, "---------------------------------------------");
    ESP_LOGI(TAG, "Barcode Scan Event: %s", sku.c_str());
    ESP_LOGI(TAG, "---------------------------------------------");

    if (!is_logged_in) {
        beepSuccess();
        if (lvgl_port_lock(-1)) {
            uiLoginViaScan(sku);
            lvgl_port_unlock();
        }
        return;
    }

    if (active_task != NULL) {
        bool match_found = false;
        bool all_done = true;
        
        for (int i = 0; i < active_task->item_count; i++) {
            TaskItem &item = active_task->items[i];
            if (sku == item.sku) {
                match_found = true;
                if (item.picked_qty < item.target_qty) {
                    item.picked_qty++;
                    beepSuccess(); // Success beep
                    if (lvgl_port_lock(-1)) {
                        update_task_detail_ui();
                        lvgl_port_unlock();
                    }
                    ESP_LOGI(TAG, "Task Item Picked: %s (%d/%d)", item.name, item.picked_qty, item.target_qty);
                } else {
                    beepError(); // Error beep (already picked)
                }
            }
            if (item.picked_qty < item.target_qty) {
                all_done = false;
            }
        }
        
        if (!match_found) {
            beepError(); // Error beep (not in task)
        } else if (all_done) {
            beepStartup(); // Special sound
            
            strncpy(active_task->status, "complete", sizeof(active_task->status) - 1);
            network_publish_task_complete(active_task->id);
            ESP_LOGI(TAG, "Task %s COMPLETED!", active_task->id);
            
            if (lvgl_port_lock(-1)) {
                active_task = NULL;
                _load_scr_direct(scr_tasks, "Tasks");
                update_tasks_ui();
                lvgl_port_unlock();
            }
        }
    } else {
        // Normal product scan flow
        beepSuccess();
        
        if (lvgl_port_lock(-1)) {
            uiShowScanResult(sku);
            lvgl_port_unlock();
        }

        bool sent = network_publish_scan(sku);
        if (sent) {
            ESP_LOGI(TAG, "Scan successfully published to server!");
        } else {
            ESP_LOGW(TAG, "MQTT disconnected - scan recorded locally.");
        }
    }
}

extern "C" void app_main(void)
{
    ESP_LOGI(TAG, "==================================================");
    ESP_LOGI(TAG, " Smart Barcode Scanner Firmware (Native ESP-IDF)  ");
    ESP_LOGI(TAG, "==================================================");

    // 1. Initialize Display, Touch, PMIC, and LVGL Subsystem
    esp_err_t disp_res = display_port_init();
    if (disp_res == ESP_OK) {
        ESP_LOGI(TAG, "Display Subsystem & LVGL Port: OK");
    } else {
        ESP_LOGE(TAG, "Display Subsystem Init Failed: %d", disp_res);
    }

    // 2. Initialize ES8311 Codec (Full Duplex I2S Mic + Speaker)
    esp_es8311_port_init(i2c_bus_handle);
    ESP_LOGI(TAG, "ES8311 Audio Subsystem: OK");

    // 3. Initialize GM65 Barcode Scanner (UART1 on GPIO 10/11)
    esp_err_t scan_res = gm65_scanner_init(on_barcode_scanned);
    if (scan_res == ESP_OK) {
        ESP_LOGI(TAG, "GM65 Barcode Scanner Subsystem: OK");
    } else {
        ESP_LOGE(TAG, "GM65 Barcode Scanner Init Failed: %d", scan_res);
    }

    // 4. Initialize Network (Wi-Fi, MQTT, WebSocket Intercom)
    esp_err_t net_res = network_init(DEFAULT_WIFI_SSID, DEFAULT_WIFI_PASS);
    if (net_res == ESP_OK) {
        ESP_LOGI(TAG, "Network & WebSocket Intercom Subsystem: OK");
    } else {
        ESP_LOGE(TAG, "Network Subsystem Init Failed: %d", net_res);
    }

    // 5. Initialize LVGL UI Screens under LVGL Port Lock
    if (lvgl_port_lock(-1)) {
        uiInit();
        lvgl_port_unlock();
    }

    ESP_LOGI(TAG, "System setup complete. Ready to process barcode scans.");

    // System Monitor Loop (LVGL tick and flush managed by esp_lvgl_port task)
    uint32_t last_log = 0;
    while (1) {
        vTaskDelay(pdMS_TO_TICKS(1000)); // Yield to other tasks

        uint32_t now = xTaskGetTickCount() * portTICK_PERIOD_MS;
        if (now - last_log > 10000) {
            last_log = now;
            ESP_LOGI(TAG, "[System Monitor] Wi-Fi: %s | Free Heap: %d B",
                     network_is_wifi_connected() ? "Connected" : "Disconnected",
                     (int)esp_get_free_heap_size());
        }
    }
}
