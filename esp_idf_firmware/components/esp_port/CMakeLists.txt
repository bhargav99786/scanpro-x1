file(GLOB_RECURSE SRC_FILES "*.cpp")

idf_component_register(SRCS ${SRC_FILES}
                    INCLUDE_DIRS "."
                    REQUIRES "freertos" "XPowersLib" "driver" "espressif__esp_codec_dev" "nvs_flash" "lwip" "esp_wifi" "esp_lcd_st7796" "esp_lcd_touch_ft6336" "espressif__esp_io_expander_tca9554")
