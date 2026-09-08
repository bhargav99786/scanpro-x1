idf_component_register(
    SRCS 
        "main.cpp"
        "display_port.cpp"
        "esp_es8311_port.cpp"
        "gm65_scanner.cpp"
        "network_mqtt.cpp"
        "ota_task.cpp"
        "ble_scanner.cpp"
        "splash_logo.c"
    INCLUDE_DIRS 
        "."
    PRIV_REQUIRES 
        driver
        esp_wifi
        XPowersLib
        esp_port
        espressif__esp_lvgl_port
        espressif__esp_io_expander_tca9554
        esp_event
        nvs_flash
        mqtt
        esp_websocket_client
        esp_codec_dev
        esp_driver_i2s
        esp_driver_i2c
        esp_driver_gpio
        esp_driver_uart
        json
        freertos
        esp_https_ota
        app_update
        esp_http_client
        bt
)

target_compile_options(${COMPONENT_LIB} PRIVATE "-Wno-missing-field-initializers" "-Wno-unused-variable")

# ── NimBLE include paths (not auto-propagated from bt component in v5.4.x) ──
# These mirror the include_dirs the bt component adds when CONFIG_BT_NIMBLE_ENABLED=y
set(BT_PATH    "$ENV{IDF_PATH}/components/bt")
set(NIMBLE_BASE "${BT_PATH}/host/nimble/nimble")
target_include_directories(${COMPONENT_LIB} PRIVATE
    # ESP-IDF NimBLE port header (esp_nimble_cfg.h, esp_nimble_mem.h)
    "${BT_PATH}/host/nimble/port/include"
    # NimBLE ESP HCI
    "${BT_PATH}/host/nimble/esp-hci/include"
    # bt porting layer (npl OS abstraction, transport)
    "${BT_PATH}/porting/include"
    "${BT_PATH}/porting/npl/freertos/include"
    "${BT_PATH}/porting/transport/include"
    # NimBLE upstream porting layer
    "${NIMBLE_BASE}/porting/nimble/include"
    "${NIMBLE_BASE}/porting/npl/freertos/include"
    # NimBLE core + host
    "${NIMBLE_BASE}/nimble/include"
    "${NIMBLE_BASE}/nimble/host/include"
    "${NIMBLE_BASE}/nimble/host/util/include"
    # NimBLE transport (nimble/transport.h pulled by hci_common.h)
    "${NIMBLE_BASE}/nimble/transport/include"
    # NimBLE GAP service
    "${NIMBLE_BASE}/nimble/host/services/gap/include"
)
