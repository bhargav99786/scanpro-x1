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

#include "nvs_flash.h"
#include "display_port.h"
#include "esp_lvgl_port.h"
#include "ui_screens.h"
#include "ble_scanner.h"

static const char *TAG = "MAIN_APP";

// Stubs for UI logic
volatile bool is_ptt_pressed = false;
void devicePowerOff() {
    ESP_LOGI(TAG, "Device power off requested. Commanding AXP2101 PMIC to shut down...");
    esp_axp2101_power_off();
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

    if (active_task == NULL) {
        bool started_task = false;
        bool is_item = false;
        if (lvgl_port_lock(-1)) {
            started_task = uiTryStartTask(sku, is_item);
            lvgl_port_unlock();
        }

        if (started_task) {
            ESP_LOGI(TAG, "Task started directly via scan!");
            if (!is_item) {
                beepStartup(); // Just opened task via Task ID
                return;
            }
            // If it is an item, fall through to process the pick
        } else {
            // Ignore non-task barcodes when not currently in an active task
            beepError();
            ESP_LOGW(TAG, "Scanned barcode %s is not a known Task ID or Item. Ignoring.", sku.c_str());
            return;
        }
    }

    if (active_task != NULL) {
        bool match_found = false;
        bool all_done = true;
        
        for (int i = 0; i < active_task->item_count; i++) {
            TaskItem &item = active_task->items[i];
            if (sku == item.sku) {
                match_found = true;
                if (item.picked_qty >= item.target_qty) {
                    beepError(); // Item is already fully picked!
                    if (lvgl_port_lock(-1)) {
                        uiShowTaskError("Item already fully picked!");
                        lvgl_port_unlock();
                    }
                    ESP_LOGW(TAG, "Item %s is already fully picked (%d/%d). Ignoring scan.", item.name, item.picked_qty, item.target_qty);
                    return;
                }

                selected_task_item_sku = item.sku; // Open ones, tens, hundreds buttons for this scanned item!
                item.picked_qty++;
                beepSuccess(); // Success beep
                if (lvgl_port_lock(-1)) {
                    update_task_detail_ui();
                    lvgl_port_unlock();
                }
                ESP_LOGI(TAG, "Task Item Scanned: %s (%d/%d)", item.name, item.picked_qty, item.target_qty);
            }
            if (item.picked_qty < item.target_qty) {
                all_done = false;
            }
        }
        
        if (!match_found) {
            beepError(); // Error beep (not in task)
            if (lvgl_port_lock(-1)) {
                uiShowTaskError("Item not in this task!");
                lvgl_port_unlock();
            }
            return;
        } else if (all_done) {
            beepStartup(); // Special sound
            ESP_LOGI(TAG, "Task %s COMPLETED!", active_task->id);
            if (lvgl_port_lock(-1)) {
                complete_and_deduct_task(active_task);
                _load_scr_direct(scr_tasks, "Tasks");
                update_tasks_ui();
                lvgl_port_unlock();
            }
            return;
        }
    }
}

extern "C" void app_main(void)
{
    ESP_LOGI(TAG, "==================================================");
    ESP_LOGI(TAG, " Smart Barcode Scanner Firmware (Native ESP-IDF)  ");
    ESP_LOGI(TAG, "==================================================");

    // 0. Initialize NVS Flash Subsystem FIRST so display rotation and hardware preferences load
    esp_err_t nvs_ret = nvs_flash_init();
    if (nvs_ret == ESP_ERR_NVS_NO_FREE_PAGES || nvs_ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        nvs_ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(nvs_ret);
    ESP_LOGI(TAG, "NVS Flash Subsystem Initialized: OK");

    // 1. Initialize Display, Touch, PMIC, and LVGL Subsystem
    esp_err_t disp_res = display_port_init();
    if (disp_res == ESP_OK) {
        ESP_LOGI(TAG, "Display Subsystem & LVGL Port: OK");
        
        // Load saved display rotation preference immediately
        display_port_load_rotation();

        // Immediately show ONLY the direct logo centered on screen
        if (lvgl_port_lock(-1)) {
            uiShowSplash();
            lv_refr_now(NULL); // Immediately render and DMA-transfer the splash logo to display
            lvgl_port_unlock();
        }

        // Small delay to allow SPI DMA frame transfer to finish
        vTaskDelay(pdMS_TO_TICKS(50));

        // Turn on backlight directly onto the logo (zero white screen, direct logo only!)
        display_port_set_backlight(80);
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

    // 4.5 Initialize Bluetooth LE Advertising Stack
    vTaskDelay(pdMS_TO_TICKS(500)); // Allow Wi-Fi PHY to settle to prevent Watchdog Triggers
    ble_scanner_init();

    // 5. Initialize LVGL UI Screens under LVGL Port Lock
    if (lvgl_port_lock(-1)) {
        uiInit();
        
        // Force all screens to run their resize callbacks to match the boot orientation,
        // which prevents overlapping UI elements when booting into Portrait mode.
        uiApplyBootOrientation();
        
        lvgl_port_unlock();
    }

    ESP_LOGI(TAG, "System setup complete. Ready to process barcode scans.");

    // System Monitor Loop (LVGL tick and flush managed by esp_lvgl_port task)
    uint32_t last_log = 0;
    while (1) {
        vTaskDelay(pdMS_TO_TICKS(10)); // Yield to other tasks

        uint32_t now = xTaskGetTickCount() * portTICK_PERIOD_MS;
        if (now - last_log > 10000) {
            last_log = now;
            ESP_LOGI(TAG, "[System Monitor] Wi-Fi: %s | Free Heap: %d B",
                     network_is_wifi_connected() ? "Connected" : "Disconnected",
                     (int)esp_get_free_heap_size());
        }
    }
}
