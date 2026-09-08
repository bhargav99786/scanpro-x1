#include "ota_task.h"
#include "esp_log.h"
#include "esp_system.h"
#include "esp_ota_ops.h"
#include "esp_http_client.h"
#include "esp_partition.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <string>
#include "esp_lvgl_port.h"
#include "ui_screens.h"
#include "network_mqtt.h"

static const char *TAG = "OTA_TASK";

static void ota_worker_task(void *pvParameter) {
    std::string url = (char*)pvParameter;
    ESP_LOGI(TAG, "Starting resilient OTA update from %s", url.c_str());

    const esp_partition_t *update_partition = esp_ota_get_next_update_partition(NULL);
    if (!update_partition) {
        ESP_LOGE(TAG, "No valid OTA update partition found!");
        free(pvParameter);
        vTaskDelete(NULL);
        return;
    }

    ESP_LOGI(TAG, "Target OTA partition: subtype %d at offset 0x%lx (size %lu)",
             update_partition->subtype, update_partition->address, update_partition->size);

    esp_ota_handle_t ota_handle = 0;
    esp_err_t err = esp_ota_begin(update_partition, OTA_WITH_SEQUENTIAL_WRITES, &ota_handle);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "esp_ota_begin failed (%s)", esp_err_to_name(err));
        free(pvParameter);
        vTaskDelete(NULL);
        return;
    }

    const size_t BUF_SIZE = 8192;
    char *buffer = (char *)malloc(BUF_SIZE);
    if (!buffer) {
        ESP_LOGE(TAG, "Failed to allocate %d bytes for OTA chunk buffer", BUF_SIZE);
        esp_ota_abort(ota_handle);
        free(pvParameter);
        vTaskDelete(NULL);
        return;
    }

    int total_bytes_written = 0;
    int64_t total_image_size = -1;
    int last_progress = -1;
    int retry_count = 0;
    int resume_failures = 0;
    const int MAX_RETRIES = 180; // Allow up to ~3-5 minutes of Wi-Fi recovery

    while (total_image_size < 0 || total_bytes_written < total_image_size) {
        // 1. If Wi-Fi is lost, wait and show status on screen
        if (!network_is_wifi_connected()) {
            ESP_LOGW(TAG, "Wi-Fi disconnected mid-OTA! Waiting for Wi-Fi to recover... (Current progress: %d%%)", last_progress >= 0 ? last_progress : 0);
            if (lvgl_port_lock(-1)) {
                char msg[64];
                snprintf(msg, sizeof(msg), "Wi-Fi Lost (%d%%)\nWaiting to reconnect...", last_progress >= 0 ? last_progress : 0);
                ui_update_ota_status(msg);
                lvgl_port_unlock();
            }

            while (!network_is_wifi_connected()) {
                vTaskDelay(pdMS_TO_TICKS(1000));
            }

            ESP_LOGI(TAG, "Wi-Fi reconnected! Resuming OTA...");
            if (lvgl_port_lock(-1)) {
                ui_update_ota_status("Wi-Fi Recovered!\nResuming download...");
                lvgl_port_unlock();
            }
            vTaskDelay(pdMS_TO_TICKS(500)); // Allow network stack to settle
        }

        // If resume has failed multiple times, fall back to restarting from byte 0
        if (resume_failures >= 3 && total_bytes_written > 0) {
            ESP_LOGW(TAG, "Resume failed %d times. Restarting download from byte 0 for clean OTA.", resume_failures);
            esp_ota_abort(ota_handle);
            ota_handle = 0;
            err = esp_ota_begin(update_partition, OTA_WITH_SEQUENTIAL_WRITES, &ota_handle);
            if (err != ESP_OK) {
                ESP_LOGE(TAG, "esp_ota_begin restart failed (%s)", esp_err_to_name(err));
                break;
            }
            total_bytes_written = 0;
            resume_failures = 0;
            if (lvgl_port_lock(-1)) {
                ui_update_ota_progress(0);
                ui_update_ota_status("Restarting Download\nfrom 0%...");
                lvgl_port_unlock();
            }
        }

        ESP_LOGI(TAG, "Opening HTTP connection (written: %d, target: %lld)...", total_bytes_written, total_image_size);

        esp_http_client_config_t config = {};
        config.url = url.c_str();
        config.timeout_ms = 4000; // 4s timeout to detect drop quickly
        config.keep_alive_enable = false;
        config.buffer_size = BUF_SIZE;

        esp_http_client_handle_t client = esp_http_client_init(&config);
        if (!client) {
            ESP_LOGE(TAG, "Failed to initialize HTTP client");
            vTaskDelay(pdMS_TO_TICKS(1000));
            if (++retry_count > MAX_RETRIES) break;
            continue;
        }

        // If resuming, request Range
        if (total_bytes_written > 0) {
            char range_header[64];
            snprintf(range_header, sizeof(range_header), "bytes=%d-", total_bytes_written);
            esp_http_client_set_header(client, "Range", range_header);
            ESP_LOGI(TAG, "Sending Range header: %s", range_header);
        }

        err = esp_http_client_open(client, 0);
        if (err != ESP_OK) {
            ESP_LOGE(TAG, "Failed to open HTTP connection: %s", esp_err_to_name(err));
            esp_http_client_cleanup(client);
            vTaskDelay(pdMS_TO_TICKS(1500));
            if (++retry_count > MAX_RETRIES) break;
            continue;
        }

        int content_length = esp_http_client_fetch_headers(client);
        int status_code = esp_http_client_get_status_code(client);
        ESP_LOGI(TAG, "HTTP Response: Status %d, Content-Length %d", status_code, content_length);

        if (status_code == 206) {
            // Partial Content - Range accepted
            if (total_image_size < 0) {
                total_image_size = total_bytes_written + content_length;
            }
            ESP_LOGI(TAG, "Resuming from byte %d / %lld", total_bytes_written, total_image_size);
            if (lvgl_port_lock(-1)) {
                ui_update_ota_status("Downloading Update...\nDo Not Turn Off!");
                lvgl_port_unlock();
            }
        } else if (status_code == 200) {
            // Full Content - Server responded from byte 0
            if (total_bytes_written > 0) {
                ESP_LOGW(TAG, "Server did not honor Range, restarting from byte 0");
                esp_ota_abort(ota_handle);
                ota_handle = 0;
                err = esp_ota_begin(update_partition, OTA_WITH_SEQUENTIAL_WRITES, &ota_handle);
                if (err != ESP_OK) {
                    ESP_LOGE(TAG, "Re-begin OTA failed: %s", esp_err_to_name(err));
                    esp_http_client_close(client);
                    esp_http_client_cleanup(client);
                    break;
                }
                total_bytes_written = 0;
                if (lvgl_port_lock(-1)) {
                    ui_update_ota_progress(0);
                    lvgl_port_unlock();
                }
            }
            total_image_size = content_length;
            if (lvgl_port_lock(-1)) {
                ui_update_ota_status("Downloading Update...\nDo Not Turn Off!");
                lvgl_port_unlock();
            }
        } else {
            ESP_LOGE(TAG, "Unexpected HTTP status %d. Will retry...", status_code);
            esp_http_client_close(client);
            esp_http_client_cleanup(client);
            resume_failures++;
            vTaskDelay(pdMS_TO_TICKS(1500));
            if (++retry_count > MAX_RETRIES) break;
            continue;
        }

        retry_count = 0;

        // Read stream and write to flash
        while (1) {
            if (!network_is_wifi_connected()) {
                ESP_LOGW(TAG, "Wi-Fi disconnected during chunk transfer!");
                break;
            }

            int data_read = esp_http_client_read(client, buffer, BUF_SIZE);
            if (data_read > 0) {
                err = esp_ota_write(ota_handle, (const void *)buffer, data_read);
                if (err != ESP_OK) {
                    ESP_LOGE(TAG, "esp_ota_write failed (%s) at offset %d", esp_err_to_name(err), total_bytes_written);
                    resume_failures++;
                    break;
                }
                total_bytes_written += data_read;

                if (total_image_size > 0) {
                    int progress = (int)(((int64_t)total_bytes_written * 100) / total_image_size);
                    if (progress > 100) progress = 100;
                    if (progress != last_progress) {
                        last_progress = progress;
                        if (lvgl_port_lock(-1)) {
                            ui_update_ota_progress(progress);
                            lvgl_port_unlock();
                        }
                    }
                }

                if (total_image_size > 0 && total_bytes_written >= total_image_size) {
                    break; // Complete
                }
            } else if (data_read == 0) {
                ESP_LOGI(TAG, "Stream ended. Written %d / %lld bytes", total_bytes_written, total_image_size);
                break;
            } else {
                ESP_LOGW(TAG, "Stream read timeout/error (%d). Disconnected at %d bytes.", data_read, total_bytes_written);
                break;
            }
        }

        esp_http_client_close(client);
        esp_http_client_cleanup(client);

        if (total_image_size > 0 && total_bytes_written >= total_image_size) {
            ESP_LOGI(TAG, "Download finished! Total bytes: %d", total_bytes_written);
            break;
        }

        vTaskDelay(pdMS_TO_TICKS(500));
    }

    free(buffer);

    if (total_image_size > 0 && total_bytes_written >= total_image_size) {
        if (lvgl_port_lock(-1)) {
            ui_update_ota_progress(100);
            ui_update_ota_status("Verifying Image\n& Writing Boot...");
            lvgl_port_unlock();
        }

        err = esp_ota_end(ota_handle);
        if (err == ESP_OK) {
            err = esp_ota_set_boot_partition(update_partition);
            if (err == ESP_OK) {
                ESP_LOGI(TAG, "OTA Update Successful! Rebooting in 1s...");
                if (lvgl_port_lock(-1)) {
                    ui_update_ota_status("Update Complete!\nRebooting now...");
                    lvgl_port_unlock();
                }
                vTaskDelay(pdMS_TO_TICKS(1000));
                esp_restart();
            } else {
                ESP_LOGE(TAG, "esp_ota_set_boot_partition failed: %s", esp_err_to_name(err));
            }
        } else {
            ESP_LOGE(TAG, "esp_ota_end failed (image verification): %s", esp_err_to_name(err));
            if (lvgl_port_lock(-1)) {
                ui_update_ota_status("Update Failed!\nImage Corrupted");
                lvgl_port_unlock();
            }
        }
    } else {
        ESP_LOGE(TAG, "OTA Update aborted. Written %d of %lld bytes", total_bytes_written, total_image_size);
        if (ota_handle) esp_ota_abort(ota_handle);
        if (lvgl_port_lock(-1)) {
            ui_update_ota_status("Update Aborted!\nNetwork Timeout");
            lvgl_port_unlock();
        }
    }

    free(pvParameter);
    vTaskDelete(NULL);
}

extern "C" void ota_start_task(const char* url) {
    char *url_copy = strdup(url);
    xTaskCreate(ota_worker_task, "ota_worker", 8192, url_copy, 5, NULL);
}
