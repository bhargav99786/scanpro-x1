#include "display_port.h"
#include "pins_config.h"
#include "esp_log.h"
#include "esp_err.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "driver/i2c_master.h"
#include "esp_io_expander_tca9554.h"
#include "lvgl.h"
#include "esp_lvgl_port.h"
#include "esp_axp2101_port.h"
#include "esp_3inch5_lcd_port.h"

static const char *TAG = "DISPLAY_PORT";

#define EXAMPLE_PIN_I2C_SDA GPIO_NUM_8
#define EXAMPLE_PIN_I2C_SCL GPIO_NUM_7

// 270 degrees rotation for Landscape mode (480x320)
#define EXAMPLE_DISPLAY_ROTATION 270

#if EXAMPLE_DISPLAY_ROTATION == 90 || EXAMPLE_DISPLAY_ROTATION == 270
#define EXAMPLE_LCD_H_RES 480
#define EXAMPLE_LCD_V_RES 320
#else
#define EXAMPLE_LCD_H_RES 320
#define EXAMPLE_LCD_V_RES 480
#endif

#define LCD_BUFFER_SIZE (EXAMPLE_LCD_H_RES * EXAMPLE_LCD_V_RES / 8)
#define I2C_PORT_NUM 0

i2c_master_bus_handle_t i2c_bus_handle = NULL;
esp_lcd_panel_io_handle_t io_handle = NULL;
esp_lcd_panel_handle_t panel_handle = NULL;
esp_io_expander_handle_t expander_handle = NULL;
esp_lcd_touch_handle_t touch_handle = NULL;
lv_display_t *lvgl_disp = NULL;
lv_indev_t *lvgl_touch_indev = NULL;

static void i2c_bus_init(void)
{
    ESP_LOGI(TAG, "Initializing I2C Master Bus...");
    i2c_master_bus_config_t i2c_mst_config = {};
    i2c_mst_config.clk_source = I2C_CLK_SRC_DEFAULT;
    i2c_mst_config.i2c_port = (i2c_port_num_t)I2C_PORT_NUM;
    i2c_mst_config.scl_io_num = EXAMPLE_PIN_I2C_SCL;
    i2c_mst_config.sda_io_num = EXAMPLE_PIN_I2C_SDA;
    i2c_mst_config.glitch_ignore_cnt = 7;
    i2c_mst_config.flags.enable_internal_pullup = 1;

    ESP_ERROR_CHECK(i2c_new_master_bus(&i2c_mst_config, &i2c_bus_handle));
}

static void io_expander_init(void)
{
    ESP_LOGI(TAG, "Initializing TCA9554 IO Expander...");
    ESP_ERROR_CHECK(esp_io_expander_new_i2c_tca9554(i2c_bus_handle, ESP_IO_EXPANDER_I2C_TCA9554_ADDRESS_000, &expander_handle));
    ESP_ERROR_CHECK(esp_io_expander_set_dir(expander_handle, IO_EXPANDER_PIN_NUM_1, IO_EXPANDER_OUTPUT));
    ESP_ERROR_CHECK(esp_io_expander_set_dir(expander_handle, IO_EXPANDER_PIN_NUM_7, IO_EXPANDER_OUTPUT));
    
    // Hardware LCD reset pulse
    ESP_ERROR_CHECK(esp_io_expander_set_level(expander_handle, IO_EXPANDER_PIN_NUM_1, 0));
    vTaskDelay(pdMS_TO_TICKS(100));
    ESP_ERROR_CHECK(esp_io_expander_set_level(expander_handle, IO_EXPANDER_PIN_NUM_1, 1));
    vTaskDelay(pdMS_TO_TICKS(100));

    // Enable Speaker Power Amplifier (P7)
    ESP_ERROR_CHECK(esp_io_expander_set_level(expander_handle, IO_EXPANDER_PIN_NUM_7, 1));
    ESP_LOGI(TAG, "Speaker Power Amplifier (TCA9554 P7) Enabled!");
}

static void lv_port_init(void)
{
    ESP_LOGI(TAG, "Initializing esp_lvgl_port for Landscape (480x320)...");
    lvgl_port_cfg_t port_cfg = ESP_LVGL_PORT_INIT_CONFIG();
    lvgl_port_init(&port_cfg);

    ESP_LOGI(TAG, "Adding LCD screen to LVGL port...");
    lvgl_port_display_cfg_t display_cfg = {
        .io_handle = io_handle,
        .panel_handle = panel_handle,
        .control_handle = NULL,
        .buffer_size = LCD_BUFFER_SIZE,
        .double_buffer = true,
        .trans_size = 0,
        .hres = EXAMPLE_LCD_H_RES,
        .vres = EXAMPLE_LCD_V_RES,
        .monochrome = false,
        .rotation = {
            .swap_xy = 0,
            .mirror_x = 0,
            .mirror_y = 0,
        },
        .flags = {
            .buff_dma = 0,
            .buff_spiram = 1, // Store display buffers in PSRAM for stability
            .sw_rotate = 1,   // Use esp_lvgl_port software rotation to create 480x320 landscape layout
            .full_refresh = 0,
            .direct_mode = 0,
        },
    };

#if EXAMPLE_DISPLAY_ROTATION == 90
    display_cfg.rotation.swap_xy = 1;
    display_cfg.rotation.mirror_x = 1;
    display_cfg.rotation.mirror_y = 1;
#elif EXAMPLE_DISPLAY_ROTATION == 180
    display_cfg.rotation.swap_xy = 0;
    display_cfg.rotation.mirror_x = 0;
    display_cfg.rotation.mirror_y = 1;
#elif EXAMPLE_DISPLAY_ROTATION == 270
    display_cfg.rotation.swap_xy = 1;
    display_cfg.rotation.mirror_x = 0;
    display_cfg.rotation.mirror_y = 0;
#endif

    lvgl_disp = lvgl_port_add_disp(&display_cfg);
    const lvgl_port_touch_cfg_t touch_cfg = {
        .disp = lvgl_disp,
        .handle = touch_handle,
    };
    lvgl_touch_indev = lvgl_port_add_touch(&touch_cfg);
    ESP_LOGI(TAG, "LVGL Port setup complete for Landscape mode.");
}

esp_err_t display_port_init(void)
{
    ESP_LOGI(TAG, "==================================================");
    ESP_LOGI(TAG, "   Starting Waveshare 3.5\" Display Subsystem     ");
    ESP_LOGI(TAG, "==================================================");

    i2c_bus_init();
    io_expander_init();

    // 1. Initialize LCD SPI bus & ST7796 panel
    esp_3inch5_display_port_init(&io_handle, &panel_handle, LCD_BUFFER_SIZE);

    // 2. Initialize FT6336 Touch controller
    esp_3inch5_touch_port_init(&touch_handle, i2c_bus_handle, EXAMPLE_LCD_H_RES, EXAMPLE_LCD_V_RES, EXAMPLE_DISPLAY_ROTATION);

    // 3. Initialize PMIC (AXP2101) & enable 3.3V power rails
    esp_axp2101_port_init(i2c_bus_handle);
    vTaskDelay(pdMS_TO_TICKS(100));

    // 4. Initialize LEDC Backlight (80% brightness)
    esp_3inch5_brightness_port_init();
    esp_3inch5_brightness_port_set(80);

    // 5. Initialize LVGL port & attach display + touch
    lv_port_init();

    ESP_LOGI(TAG, "Display and LVGL port successfully initialized!");
    return ESP_OK;
}

void display_port_set_rotation(uint8_t rotation)
{
    if (!lvgl_disp) return;
    lv_disp_rot_t rot = LV_DISP_ROT_NONE;
    switch (rotation) {
        case 1: rot = LV_DISP_ROT_90; break;
        case 2: rot = LV_DISP_ROT_180; break;
        case 3: rot = LV_DISP_ROT_270; break;
        default: rot = LV_DISP_ROT_NONE; break;
    }
    if (lvgl_port_lock(-1)) {
        lv_disp_set_rotation(lvgl_disp, rot);
        lvgl_port_unlock();
    }
}
