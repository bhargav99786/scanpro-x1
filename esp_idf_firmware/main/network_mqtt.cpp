#include "network_mqtt.h"
#include "pins_config.h"
#include "esp_es8311_port.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "mqtt_client.h"
#include "esp_websocket_client.h"
#include "esp_lvgl_port.h"
#include "ui_screens.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "cJSON.h"
#include "freertos/ringbuf.h"

static const char *TAG = "NETWORK_MQTT";
static bool wifi_connected = false;
static esp_mqtt_client_handle_t mqtt_client = NULL;
static esp_websocket_client_handle_t ws_client = NULL;
static RingbufHandle_t audio_rx_ringbuf = NULL;
static volatile bool is_ptt_active = false;

// ── Audio RX Task: PC/Browser → ESP32 Speaker (plays audio sent by Start Talking button) ──────
static void audio_rx_task(void *pvParameters)
{
    ESP_LOGI("AUDIO_RX", "Speaker RX task started.");
    while (1) {
        if (audio_rx_ringbuf) {
            size_t item_size = 0;
            uint8_t *rx_data = (uint8_t *)xRingbufferReceive(audio_rx_ringbuf, &item_size, pdMS_TO_TICKS(30));
            if (rx_data && item_size > 0) {
                esp_es8311_play(rx_data, item_size);
                vRingbufferReturnItem(audio_rx_ringbuf, (void *)rx_data);
            } else {
                vTaskDelay(pdMS_TO_TICKS(5));
            }
        } else {
            vTaskDelay(pdMS_TO_TICKS(20));
        }
    }
}

// ── Audio TX Task: ESP32 Mic → Server/Browser Speaker ──────────────────────
// Streams VAD-gated mic audio ONLY when the on-screen "TAP TO TALK" toggle is ON.
// When the toggle is OFF, all mic frames are silently discarded (no network traffic).
#define VAD_THRESHOLD 800  // 16-bit PCM: ~2.4% of full scale, filters background noise
static void audio_tx_task(void *pvParameters)
{
    uint8_t mic_buf[1024];
    // Warm-up read: kick-starts the ADC DMA pipeline
    esp_es8311_record(mic_buf, sizeof(mic_buf));
    ESP_LOGI("AUDIO_TX", "Mic TX task started (PTT-gated + VAD).");

    while (1) {
        // If PTT toggle is OFF, drain the ADC buffer and sleep — no transmission
        if (!is_ptt_active) {
            esp_es8311_record(mic_buf, sizeof(mic_buf)); // keep ADC DMA running
            vTaskDelay(pdMS_TO_TICKS(20));
            continue;
        }

        int bytes_read = esp_es8311_record(mic_buf, sizeof(mic_buf));
        if (bytes_read <= 0) { vTaskDelay(pdMS_TO_TICKS(5)); continue; }

        // VAD: measure peak amplitude — drop silent frames even when PTT is active
        int16_t *pcm = (int16_t *)mic_buf;
        int samples = bytes_read / sizeof(int16_t);
        int16_t peak = 0;
        for (int i = 0; i < samples; i++) {
            int16_t abs_val = pcm[i] < 0 ? -pcm[i] : pcm[i];
            if (abs_val > peak) peak = abs_val;
        }

        if (peak > VAD_THRESHOLD && ws_client && esp_websocket_client_is_connected(ws_client)) {
            esp_websocket_client_send_bin(ws_client, (const char *)mic_buf, bytes_read, pdMS_TO_TICKS(50));
        }
    }
}

// ── WebSocket Event Handler ──────────────────────────────────────────────────
static void websocket_event_handler(void *handler_args, esp_event_base_t base, int32_t event_id, void *event_data)
{
    esp_websocket_event_data_t *data = (esp_websocket_event_data_t *)event_data;
    switch (event_id) {
        case WEBSOCKET_EVENT_CONNECTED:
            ESP_LOGI(TAG, "WebSocket Audio Client Connected to Server!");
            break;
        case WEBSOCKET_EVENT_DISCONNECTED:
            ESP_LOGI(TAG, "WebSocket Audio Client Disconnected");
            break;
        case WEBSOCKET_EVENT_DATA:
            if ((data->op_code == 0x02 || data->op_code == 0x00) && data->data_len > 0 && data->data_ptr != NULL) { // Binary or continuation audio chunk
                if (audio_rx_ringbuf) {
                    UBaseType_t res = xRingbufferSend(audio_rx_ringbuf, data->data_ptr, data->data_len, pdMS_TO_TICKS(10));
                    if (res != pdTRUE) {
                        ESP_LOGW(TAG, "Audio RingBuffer full! Dropped %d bytes", data->data_len);
                    }
                }
            }
            break;
        default:
            break;
    }
}

// ── NVS Wi-Fi Helpers ────────────────────────────────────────────────────────
bool network_get_saved_wifi(char *out_ssid, size_t max_ssid_len, char *out_pass, size_t max_pass_len)
{
    if (!out_ssid || max_ssid_len == 0) return false;
    nvs_handle_t handle;
    if (nvs_open("wifi_store", NVS_READONLY, &handle) != ESP_OK) {
        return false;
    }
    size_t s_len = max_ssid_len;
    size_t p_len = max_pass_len;
    esp_err_t res1 = nvs_get_str(handle, "ssid", out_ssid, &s_len);
    esp_err_t res2 = nvs_get_str(handle, "pass", out_pass, &p_len);
    nvs_close(handle);
    return (res1 == ESP_OK && s_len > 1);
}

esp_err_t network_save_and_connect_wifi(const char *ssid, const char *password)
{
    if (!ssid || strlen(ssid) == 0) return ESP_ERR_INVALID_ARG;

    // Save to NVS
    nvs_handle_t handle;
    if (nvs_open("wifi_store", NVS_READWRITE, &handle) == ESP_OK) {
        nvs_set_str(handle, "ssid", ssid);
        nvs_set_str(handle, "pass", password ? password : "");
        nvs_commit(handle);
        nvs_close(handle);
        ESP_LOGI(TAG, "Saved Wi-Fi credentials to NVS: SSID='%s'", ssid);
    }

    // Reconfigure Wi-Fi STA
    wifi_config_t wifi_config = {};
    strncpy((char *)wifi_config.sta.ssid, ssid, sizeof(wifi_config.sta.ssid) - 1);
    if (password) {
        strncpy((char *)wifi_config.sta.password, password, sizeof(wifi_config.sta.password) - 1);
    }
    wifi_config.sta.threshold.authmode = (password && strlen(password) > 0) ? WIFI_AUTH_WPA2_PSK : WIFI_AUTH_OPEN;
    wifi_config.sta.pmf_cfg.capable = true;
    wifi_config.sta.pmf_cfg.required = false;
    wifi_config.sta.scan_method = WIFI_FAST_SCAN;
    wifi_config.sta.sort_method = WIFI_CONNECT_AP_BY_SIGNAL;

    ESP_LOGI(TAG, "Reconnecting Wi-Fi STA to SSID: %s", ssid);
    esp_wifi_disconnect();
    esp_wifi_set_config(WIFI_IF_STA, &wifi_config);
    esp_wifi_connect();

    return ESP_OK;
}

// ── Wi-Fi Event Handler ──────────────────────────────────────────────────────
static void wifi_event_handler(void *arg, esp_event_base_t event_base, int32_t event_id, void *event_data)
{
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) {
        ESP_LOGI(TAG, "Wi-Fi STA Started. Connecting to AP...");
        if (lvgl_port_lock(-1)) {
            uiSetWifiStatus(LV_SYMBOL_WIFI " Connecting...");
            lvgl_port_unlock();
        }
        esp_wifi_connect();
    } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
        wifi_event_sta_disconnected_t *disc = (wifi_event_sta_disconnected_t *)event_data;
        wifi_connected = false;
        ESP_LOGE(TAG, "Wi-Fi Disconnected! Reason code: %d.", disc ? disc->reason : -1);
        
        if (lvgl_port_lock(-1)) {
            uiSetWifiStatus(LV_SYMBOL_WIFI " Disconnected");
            lvgl_port_unlock();
        }
        esp_wifi_connect();
    } else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        ip_event_got_ip_t *event = (ip_event_got_ip_t *)event_data;
        char ip_str[32];
        snprintf(ip_str, sizeof(ip_str), LV_SYMBOL_WIFI " " IPSTR, IP2STR(&event->ip_info.ip));
        ESP_LOGI(TAG, "Wi-Fi Connected! IP: %s", ip_str);
        wifi_connected = true;

        if (lvgl_port_lock(-1)) {
            uiSetWifiStatus(ip_str);
            lvgl_port_unlock();
        }

        if (mqtt_client) {
            esp_mqtt_client_start(mqtt_client);
        }
        if (ws_client) {
            esp_websocket_client_start(ws_client);
        }
    }
}

// ── MQTT Event Handler ───────────────────────────────────────────────────────
static void mqtt_event_handler(void *handler_args, esp_event_base_t base, int32_t event_id, void *event_data)
{
    esp_mqtt_event_handle_t event = (esp_mqtt_event_handle_t)event_data;
    switch ((esp_mqtt_event_id_t)event_id) {
    case MQTT_EVENT_CONNECTED: {
        ESP_LOGI(TAG, "MQTT Broker connected successfully!");
        std::string task_topic = "device/" + std::string(DEVICE_ID) + "/tasks";
        esp_mqtt_client_subscribe(mqtt_client, task_topic.c_str(), 1);
        esp_mqtt_client_subscribe(mqtt_client, "config/inventory", 1);
        esp_mqtt_client_subscribe(mqtt_client, "config/users", 1);

        // Publish device online status
        network_publish_device_status(true, is_logged_in ? logged_in_user : "No Login");
        break;
    }
    case MQTT_EVENT_DATA: {
        static std::string current_topic;
        static std::string current_payload;

        if (event->topic_len > 0) {
            current_topic = std::string(event->topic, event->topic_len);
            current_payload.clear();
        }

        current_payload.append(event->data, event->data_len);
        ESP_LOGI(TAG, "MQTT Chunk Received — Topic: %s (Chunk: %d, Total: %d)", current_topic.c_str(), event->data_len, event->total_data_len);

        if (current_payload.length() >= event->total_data_len) {
            ESP_LOGI(TAG, "MQTT Message Fully Received! (Len: %d)", current_payload.length());

            std::string task_topic = "device/" + std::string(DEVICE_ID) + "/tasks";
            if (current_topic == task_topic) {
                ESP_LOGI(TAG, "Parsing real-time tasks update from server...");
                ui_update_tasks_from_json(current_payload.c_str());
            } else if (current_topic == "config/inventory") {
                ESP_LOGI(TAG, "Parsing inventory update from server...");
                ui_update_inventory_from_json(current_payload.c_str());
            } else if (current_topic == "config/users") {
                ESP_LOGI(TAG, "Parsing users update from server...");
                ui_update_users_from_json(current_payload.c_str());
            }
        }
        break;
    }
    default:
        break;
    }
}

esp_err_t network_init(const char *ssid, const char *password)
{
    // 1. Initialize NVS
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    // 2. Initialize TCP/IP and Event Loop
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    esp_netif_create_default_wifi_sta();

    // 3. Wi-Fi Config
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &wifi_event_handler, NULL, NULL));
    ESP_ERROR_CHECK(esp_event_handler_instance_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &wifi_event_handler, NULL, NULL));

    char saved_ssid[64] = {0};
    char saved_pass[64] = {0};
    const char *target_ssid = NULL;
    const char *target_pass = NULL;

    if (network_get_saved_wifi(saved_ssid, sizeof(saved_ssid), saved_pass, sizeof(saved_pass))) {
        target_ssid = saved_ssid;
        target_pass = saved_pass;
        ESP_LOGI(TAG, "Loaded Wi-Fi credentials from NVS: SSID='%s'", target_ssid);
    } else {
        target_ssid = (ssid && strlen(ssid) > 0) ? ssid : DEFAULT_WIFI_SSID;
        target_pass = (password && strlen(password) > 0) ? password : DEFAULT_WIFI_PASS;
        ESP_LOGI(TAG, "No saved Wi-Fi credentials in NVS, using fallback SSID='%s'", target_ssid);
    }

    wifi_config_t wifi_config = {};
    strncpy((char *)wifi_config.sta.ssid, target_ssid, sizeof(wifi_config.sta.ssid) - 1);
    strncpy((char *)wifi_config.sta.password, target_pass, sizeof(wifi_config.sta.password) - 1);

    // Configure security authmode & PMF for maximum compatibility with modern routers
    wifi_config.sta.threshold.authmode = (strlen(target_pass) > 0) ? WIFI_AUTH_WPA2_PSK : WIFI_AUTH_OPEN;
    wifi_config.sta.pmf_cfg.capable = true;
    wifi_config.sta.pmf_cfg.required = false;
    wifi_config.sta.scan_method = WIFI_FAST_SCAN;
    wifi_config.sta.sort_method = WIFI_CONNECT_AP_BY_SIGNAL;

    ESP_LOGI(TAG, "Configuring Wi-Fi for SSID: %s", target_ssid);
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config));
    ESP_ERROR_CHECK(esp_wifi_start());

    // 4. Create Audio Tasks (Bidirectional: VAD-gated ESP32 Mic→PC + PC→ESP32 Speaker)
    audio_rx_ringbuf = xRingbufferCreate(16384, RINGBUF_TYPE_BYTEBUF);
    xTaskCreatePinnedToCore(audio_rx_task, "audio_rx_task", 4096, NULL, 5, NULL, 1);
    xTaskCreatePinnedToCore(audio_tx_task, "audio_tx_task", 4096, NULL, 5, NULL, 1);

    // 5. Initialize MQTT Client
    static std::string static_mqtt_uri = "mqtt://" + std::string(DEFAULT_MQTT_HOST) + ":" + std::to_string(DEFAULT_MQTT_PORT);
    esp_mqtt_client_config_t mqtt_cfg = {};
    mqtt_cfg.broker.address.uri = static_mqtt_uri.c_str();
    mqtt_cfg.buffer.size = 4096; // Prevent chunking of large JSON payloads (e.g. tasks/inventory)
    mqtt_client = esp_mqtt_client_init(&mqtt_cfg);
    if (mqtt_client) {
        esp_mqtt_client_register_event(mqtt_client, MQTT_EVENT_ANY, mqtt_event_handler, NULL);
    }

    // 6. Initialize WebSocket Client for Audio
    static std::string static_ws_uri = "ws://" + std::string(DEFAULT_MQTT_HOST) + ":3030/audio?client_type=esp32&device_id=" + std::string(DEVICE_ID);
    esp_websocket_client_config_t ws_cfg = {};
    ws_cfg.uri = static_ws_uri.c_str();

    ws_client = esp_websocket_client_init(&ws_cfg);
    esp_websocket_register_events(ws_client, WEBSOCKET_EVENT_ANY, websocket_event_handler, (void *)ws_client);

    if (wifi_connected && ws_client) {
        ESP_LOGI(TAG, "Wi-Fi already connected, starting WebSocket audio client immediately...");
        esp_websocket_client_start(ws_client);
    }

    return ESP_OK;
}

bool network_publish_scan(const std::string &sku)
{
    if (!wifi_connected || !mqtt_client) return false;

    cJSON *root = cJSON_CreateObject();
    cJSON_AddStringToObject(root, "device_id", DEVICE_ID);
    cJSON_AddStringToObject(root, "sku", sku.c_str());
    cJSON_AddNumberToObject(root, "ts", esp_log_timestamp());
    char *payload = cJSON_PrintUnformatted(root);

    std::string topic = "device/" + std::string(DEVICE_ID) + "/scan";
    int msg_id = esp_mqtt_client_publish(mqtt_client, topic.c_str(), payload, 0, 1, 0);

    cJSON_Delete(root);
    free(payload);

    ESP_LOGI(TAG, "Published scan %s to MQTT topic %s (msg_id=%d)", sku.c_str(), topic.c_str(), msg_id);
    return msg_id >= 0;
}

bool network_publish_device_status(bool is_online, const char *user)
{
    if (!wifi_connected || !mqtt_client) return false;

    cJSON *root = cJSON_CreateObject();
    cJSON_AddStringToObject(root, "status", is_online ? "online" : "offline");
    cJSON_AddStringToObject(root, "user", (user && strlen(user) > 0) ? user : "No Login");
    cJSON_AddNumberToObject(root, "ts", (double)esp_log_timestamp());

    char *json_str = cJSON_PrintUnformatted(root);
    std::string topic = "device/" + std::string(DEVICE_ID) + "/status";

    int msg_id = esp_mqtt_client_publish(mqtt_client, topic.c_str(), json_str, 0, 1, 1); // retain = 1

    cJSON_Delete(root);
    free(json_str);

    ESP_LOGI(TAG, "Published device status (online=%d, user='%s') to topic %s (msg_id=%d)",
             is_online, user ? user : "", topic.c_str(), msg_id);
    return msg_id >= 0;
}

bool network_publish_task_complete(const char *task_id)
{
    if (!wifi_connected || !mqtt_client || !task_id) return false;

    cJSON *root = cJSON_CreateObject();
    cJSON_AddStringToObject(root, "task_id", task_id);
    cJSON_AddStringToObject(root, "device_id", DEVICE_ID);

    char *json_str = cJSON_PrintUnformatted(root);
    std::string topic = "device/" + std::string(DEVICE_ID) + "/task_complete";

    int msg_id = esp_mqtt_client_publish(mqtt_client, topic.c_str(), json_str, 0, 1, 0);

    cJSON_Delete(root);
    free(json_str);

    ESP_LOGI(TAG, "Published task completion (task_id='%s') to topic %s (msg_id=%d)",
             task_id, topic.c_str(), msg_id);
    return msg_id >= 0;
}

bool network_is_wifi_connected(void)
{
    return wifi_connected;
}

void network_set_ptt(bool pressed)
{
    is_ptt_active = pressed;
}

