/**
 * ble_scanner.cpp
 *
 * NimBLE-based BLE advertising for ScanPro-X1.
 * The device advertises itself as "ScanPro-X1" so any mobile phone
 * running a BLE scanner app (e.g. nRF Connect, LightBlue) can see it.
 *
 * Uses the ESP-IDF NimBLE host (CONFIG_BT_NIMBLE_ENABLED must be y in sdkconfig).
 */

#include "ble_scanner.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_lvgl_port.h"
#include "ui_screens.h"

/* NimBLE host headers — paths verified against ESP-IDF v5.4.1 layout */
#include "nimble/nimble_port.h"
#include "nimble/nimble_port_freertos.h"
#include "host/ble_hs.h"
#include "host/util/util.h"
#include "services/gap/ble_svc_gap.h"
#include "services/gatt/ble_svc_gatt.h"

#include "esp_mac.h"

static const char *TAG = "BLE_SCANNER";

static char s_ble_device_name[32] = "ScanPro-X1";
#define BLE_ADV_INTERVAL_MS  500   // Advertising interval in ms (500ms = lower CPU usage, reduces UI lag)

static volatile bool s_adv_active  = false;
static volatile bool s_ble_ready   = false;

/* ── Forward declarations ─────────────────────────────────────────────────── */
static void _ble_on_sync(void);
static void _ble_on_reset(int reason);
static int  _gap_event_cb(struct ble_gap_event *event, void *arg);

/* ── NimBLE host task (runs the NimBLE event loop) ────────────────────────── */
static void _nimble_host_task(void *param)
{
    ESP_LOGI(TAG, "NimBLE host task started");
    nimble_port_run();          // blocks until nimble_port_stop() is called
    nimble_port_freertos_deinit();
}

/* ── Sync callback: called when NimBLE stack is ready ─────────────────────── */
static void _ble_on_sync(void)
{
    ESP_LOGI(TAG, "NimBLE stack synchronised — BLE ready");
    
    /* Ensure we have a valid Bluetooth MAC address generated */
    int rc = ble_hs_util_ensure_addr(0);
    if (rc != 0) {
        ESP_LOGE(TAG, "Failed to ensure BLE address: rc=%d", rc);
    }

    s_ble_ready = true;
    // Auto-start advertising as soon as the stack is ready
    if (s_adv_active) {
        bleStartAdvertising();
    }
}

static void _ble_on_reset(int reason)
{
    ESP_LOGE(TAG, "NimBLE reset! reason=%d", reason);
    s_ble_ready = false;
}

/* ── GAP event callback ────────────────────────────────────────────────────── */
static int _gap_event_cb(struct ble_gap_event *event, void *arg)
{
    switch (event->type) {
        case BLE_GAP_EVENT_CONNECT:
            ESP_LOGI(TAG, "BLE connection established (handle=%d)", event->connect.conn_handle);
            if (lvgl_port_lock(-1)) {
                uiSetBleConnected(true);
                lvgl_port_unlock();
            }
            /* Restart advertising so other devices can still see us */
            bleStartAdvertising();
            break;
        case BLE_GAP_EVENT_DISCONNECT:
            ESP_LOGI(TAG, "BLE disconnected (reason=%d) — restarting advertising",
                     event->disconnect.reason);
            if (lvgl_port_lock(-1)) {
                uiSetBleConnected(false);
                lvgl_port_unlock();
            }
            bleStartAdvertising();
            break;
        case BLE_GAP_EVENT_ADV_COMPLETE:
            ESP_LOGI(TAG, "Advertising cycle complete — restarting");
            bleStartAdvertising();
            break;
        default:
            break;
    }
    return 0;
}

/* ── Public API ────────────────────────────────────────────────────────────── */

extern "C" void ble_store_config_init(void);

void ble_scanner_init(void)
{
    ESP_LOGI(TAG, "Initialising NimBLE BLE stack...");

    /* nimble_port_init calls esp_bt_controller_init + nimble host init */
    esp_err_t ret = nimble_port_init();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "nimble_port_init failed: %s", esp_err_to_name(ret));
        return;
    }

    /* Configure the NimBLE host */
    ble_hs_cfg.sync_cb  = _ble_on_sync;
    ble_hs_cfg.reset_cb = _ble_on_reset;
    
    /* Configure Security Manager for "Just Works" pairing */
    ble_hs_cfg.sm_io_cap = BLE_SM_IO_CAP_NO_IO;    // No keyboard or display
    ble_hs_cfg.sm_oob_data_flag = 0;
    ble_hs_cfg.sm_bonding = 1;                     // Allow bonding
    ble_hs_cfg.sm_mitm = 0;                        // Disable MITM to prevent PIN request
    ble_hs_cfg.sm_sc = 1;                          // Enable Secure Connections
    ble_hs_cfg.sm_our_key_dist = BLE_SM_PAIR_KEY_DIST_ENC | BLE_SM_PAIR_KEY_DIST_ID;
    ble_hs_cfg.sm_their_key_dist = BLE_SM_PAIR_KEY_DIST_ENC | BLE_SM_PAIR_KEY_DIST_ID;

    ble_svc_gap_init();
    ble_svc_gatt_init();

    /* Initialize the BLE store so keys can be saved (prevents pairing rejection) */
    ble_store_config_init();

    /* Read MAC address to generate unique device name with last 4 hex digits */
    uint8_t mac[6] = {0};
    if (esp_read_mac(mac, ESP_MAC_BT) == ESP_OK) {
        snprintf(s_ble_device_name, sizeof(s_ble_device_name), "ScanPro-X1-%02X%02X", mac[4], mac[5]);
    } else if (esp_read_mac(mac, ESP_MAC_WIFI_STA) == ESP_OK) {
        snprintf(s_ble_device_name, sizeof(s_ble_device_name), "ScanPro-X1-%02X%02X", mac[4], mac[5]);
    } else {
        snprintf(s_ble_device_name, sizeof(s_ble_device_name), "ScanPro-X1");
    }

    int rc = ble_svc_gap_device_name_set(s_ble_device_name);
    if (rc != 0) {
        ESP_LOGE(TAG, "ble_svc_gap_device_name_set failed: rc=%d", rc);
    }

    /* Start the NimBLE host task on core 0 */
    nimble_port_freertos_init(_nimble_host_task);

    ESP_LOGI(TAG, "NimBLE BLE stack init complete. Device name: %s", s_ble_device_name);
}

void bleStartAdvertising(void)
{
    if (!s_ble_ready) {
        /* Stack not ready yet — mark intent so _ble_on_sync will start adv */
        s_adv_active = true;
        ESP_LOGW(TAG, "bleStartAdvertising called before stack ready — will auto-start when sync fires");
        return;
    }

    /* Stop any ongoing advertising first */
    ble_gap_adv_stop();

    /* ── Build advertising data ──────────────────────────────────────────── */
    struct ble_hs_adv_fields fields = {};

    /* Flags: General Discoverable Mode + BR/EDR Not Supported */
    fields.flags                 = BLE_HS_ADV_F_DISC_GEN | BLE_HS_ADV_F_BREDR_UNSUP;

    /* Complete Local Name — visible in every BLE scanner */
    fields.name                  = (uint8_t *)s_ble_device_name;
    fields.name_len              = strlen(s_ble_device_name);
    fields.name_is_complete      = 1;

    /* Advertise as a "Generic" appearance */
    fields.appearance            = 0x0000;
    fields.appearance_is_present = 1;

    /* Tx Power (let the stack fill it in) */
    fields.tx_pwr_lvl            = BLE_HS_ADV_TX_PWR_LVL_AUTO;
    fields.tx_pwr_lvl_is_present = 1;

    int rc = ble_gap_adv_set_fields(&fields);
    if (rc != 0) {
        ESP_LOGE(TAG, "ble_gap_adv_set_fields error: rc=%d", rc);
        return;
    }

    /* ── Build advertising parameters ───────────────────────────────────── */
    struct ble_gap_adv_params adv_params = {};
    adv_params.conn_mode    = BLE_GAP_CONN_MODE_UND;  /* Undirected connectable */
    adv_params.disc_mode    = BLE_GAP_DISC_MODE_GEN;  /* General discoverable   */
    adv_params.itvl_min     = BLE_GAP_ADV_ITVL_MS(BLE_ADV_INTERVAL_MS);
    adv_params.itvl_max     = BLE_GAP_ADV_ITVL_MS(BLE_ADV_INTERVAL_MS + 10);

    uint8_t own_addr_type;
    rc = ble_hs_id_infer_auto(0, &own_addr_type);
    if (rc != 0) {
        ESP_LOGE(TAG, "error determining address type; rc=%d", rc);
        return;
    }

    rc = ble_gap_adv_start(own_addr_type, NULL, BLE_HS_FOREVER,
                           &adv_params, _gap_event_cb, NULL);
    if (rc == 0) {
        s_adv_active = true;
        ESP_LOGI(TAG, "✓ BLE advertising started — device '%s' is now visible to phones", s_ble_device_name);
    } else if (rc == BLE_HS_EALREADY) {
        ESP_LOGW(TAG, "BLE already advertising");
        s_adv_active = true;
    } else {
        ESP_LOGE(TAG, "ble_gap_adv_start failed: rc=%d", rc);
        s_adv_active = false;
    }
}

void bleStopAdvertising(void)
{
    s_adv_active = false;
    if (!s_ble_ready) return;
    ble_gap_adv_stop();
    ESP_LOGI(TAG, "BLE advertising stopped — device is no longer visible");
}

bool bleIsAdvertising(void)
{
    return s_adv_active && s_ble_ready;
}

const char* bleGetDeviceName(void)
{
    return s_ble_device_name;
}
