#pragma once

#include "esp_err.h"
#include <string>

/**
 * @brief Initialize NVS, Wi-Fi Station, MQTT Client, and WebSocket Audio Client
 */
esp_err_t network_init(const char *ssid, const char *password);

/**
 * @brief Publish barcode scan event to MQTT topic device/{device_id}/scan
 */
bool network_publish_scan(const std::string &sku);

/**
 * @brief Check if Wi-Fi is connected
 */
bool network_is_wifi_connected(void);

/**
 * @brief Save new Wi-Fi credentials to NVS and reconnect immediately
 */
esp_err_t network_save_and_connect_wifi(const char *ssid, const char *password);

/**
 * @brief Get saved Wi-Fi credentials from NVS
 */
bool network_get_saved_wifi(char *out_ssid, size_t max_ssid_len, char *out_pass, size_t max_pass_len);

/**
 * @brief Set Push-To-Talk state for WebSocket microphone recording
 */
void network_set_ptt(bool pressed);

/**
 * @brief Publish online/offline device status and active user to MQTT topic device/{device_id}/status
 */
bool network_publish_device_status(bool is_online, const char *user);

/**
 * @brief Publish task completion notification to MQTT topic device/{device_id}/task_complete
 */
bool network_publish_task_complete(const char *task_id);

