#include "display_port.h"
#include "pins_config.h"
#include "esp_log.h"
#include "esp_err.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "driver/i2c_master.h"
#include "driver/spi_master.h"
#include "esp_io_expander_tca9554.h"
#include "lvgl.h"
#include "esp_lvgl_port.h"
#include "esp_axp2101_port.h"
#include "esp_3inch5_lcd_port.h"
#include "nvs_flash.h"
#include "nvs.h"

static const char *TAG = "DISPLAY_PORT";

#define EXAMPLE_PIN_I2C_SDA GPIO_NUM_8
#define EXAMPLE_PIN_I2C_SCL GPIO_NUM_7

// Base rotation is 0 degrees (Portrait 320x480) to avoid hardware/software rotation conflicts
#define EXAMPLE_DISPLAY_ROTATION 0

extern lv_disp_t *lvgl_disp;

static void (*original_touch_read_cb)(lv_indev_drv_t * indev_drv, lv_indev_data_t * data) = NULL;

static void my_touch_read_cb(lv_indev_drv_t * indev_drv, lv_indev_data_t * data)
{
    if (original_touch_read_cb) {
        original_touch_read_cb(indev_drv, data);
    }
    
    if (lvgl_disp) {
        int16_t raw_x = data->point.x;
        int16_t raw_y = data->point.y;
        
        lv_disp_rot_t rot = lv_disp_get_rotation(lvgl_disp);

        // FT6336 raw touch controller reports coordinates directly matching
        // the display panel in base Portrait orientation (X: 0..319, Y: 0..479).
        // No mirroring is needed. LVGL's indev_pointer_proc handles all
        // orientation transformations (ROT_NONE, ROT_90, ROT_180, ROT_270).
        data->point.x = raw_x;
        data->point.y = raw_y;

        if (data->state == LV_INDEV_STATE_PRESSED) {
            ESP_LOGI("TOUCH_TEST", "PRESSED | Rot:%d | Raw:(%d,%d) -> Out:(%d,%d)",
                     (int)rot, raw_x, raw_y, data->point.x, data->point.y);
        }
    }
}

#if EXAMPLE_DISPLAY_ROTATION == 90 || EXAMPLE_DISPLAY_ROTATION == 270
#define EXAMPLE_LCD_H_RES 480
#define EXAMPLE_LCD_V_RES 320
#else
#define EXAMPLE_LCD_H_RES 320
#define EXAMPLE_LCD_V_RES 480
#endif

// Buffer size: 20 lines (480 * 20 = 9600 pixels = 19,200 bytes per buffer).
// Allocated in INTERNAL DMA RAM at boot (buff_spiram = 0, buff_dma = 1).
// With double buffering, 2 x 19.2 KB = 38.4 KB total is allocated in internal DMA RAM.
// Because the buffers are inside internal DMA RAM, the SPI master driver never needs
// to dynamically allocate bounce buffers at runtime, completely eliminating heap fragmentation
// failures (panel_io_spi_tx_color: spi transmit (queue) color failed).
#define LCD_BUFFER_LINES 20
#define LCD_BUFFER_SIZE (480 * LCD_BUFFER_LINES)
#define I2C_PORT_NUM 0

static void my_disp_wait_cb(lv_disp_drv_t *drv)
{
    // Yield to FreeRTOS scheduler while waiting for DMA transmission to finish.
    // This allows the IDLE task on Core 1 to run and reset the Task Watchdog!
    vTaskDelay(pdMS_TO_TICKS(1));
}

i2c_master_bus_handle_t i2c_bus_handle = NULL;
esp_lcd_panel_io_handle_t io_handle = NULL;
esp_lcd_panel_handle_t panel_handle = NULL;
esp_io_expander_handle_t expander_handle = NULL;
esp_lcd_touch_handle_t touch_handle = NULL;
lv_disp_t *lvgl_disp = NULL;
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
    
    // CRITICAL: Pin LVGL task to Core 1 (APP_CPU). 
    // The BLE stack (NimBLE) is heavily pinned to Core 0 (PRO_CPU) and causes 
    // extreme lag and jitter if the LVGL rendering task is left floating or on Core 0.
    port_cfg.task_affinity = 1; 
    port_cfg.task_max_sleep_ms = 30; // Fast 30ms wake-up when idle to eliminate touch latency
    
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
            .mirror_x = 1,
            .mirror_y = 0,
        },
        .flags = {
            .buff_dma = 1,
            .buff_spiram = 0, // Allocate in internal DMA RAM so SPI DMA transfers directly with zero runtime allocation!

            .sw_rotate = 0,   // Hardware rotation directly via ST7796 registers (zero extra malloc, prevents freeze!)
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
    if (lvgl_disp) {
        if (lvgl_disp->driver) {
            lvgl_disp->driver->wait_cb = my_disp_wait_cb;
        }
        // Ensure default active screen is pure black with no borders
        lv_obj_t *act_scr = lv_disp_get_scr_act(lvgl_disp);
        if (act_scr) {
            lv_obj_set_style_bg_color(act_scr, lv_color_black(), 0);
            lv_obj_set_style_bg_opa(act_scr, LV_OPA_COVER, 0);
            lv_obj_set_style_border_width(act_scr, 0, 0);
        }
    }

    const lvgl_port_touch_cfg_t touch_cfg = {
        .disp = lvgl_disp,
        .handle = touch_handle,
    };
    lvgl_touch_indev = lvgl_port_add_touch(&touch_cfg);
    
    // Override touch read_cb to handle orientation coordinates natively
    if (lvgl_touch_indev && lvgl_touch_indev->driver) {
        original_touch_read_cb = lvgl_touch_indev->driver->read_cb;
        lvgl_touch_indev->driver->read_cb = my_touch_read_cb;
    }

    ESP_LOGI(TAG, "LVGL Port setup complete.");
}

esp_err_t display_port_init(void)
{
    ESP_LOGI(TAG, "==================================================");
    ESP_LOGI(TAG, "   Starting Waveshare 3.5\" Display Subsystem     ");
    ESP_LOGI(TAG, "==================================================");

    i2c_bus_init();

    // Initialize SPI bus manually here BEFORE the IO expander reset pulse.
    // This is because initializing the SPI peripheral can glitch the SPI clock line.
    // Since LCD_CS is tied to ground, this glitch misaligns the ST7796 SPI state machine.
    // By initializing SPI *before* pulsing LCD_RST on the TCA9554, the reset pulse
    // clears the glitch and the display will properly receive initialization commands!
    spi_bus_config_t buscfg = {};
    buscfg.sclk_io_num = LCD_SPI_SCLK;
    buscfg.mosi_io_num = LCD_SPI_MOSI;
    buscfg.miso_io_num = LCD_SPI_MISO;
    buscfg.quadwp_io_num = -1;
    buscfg.quadhd_io_num = -1;
    buscfg.max_transfer_sz = EXAMPLE_LCD_H_RES * EXAMPLE_LCD_V_RES * sizeof(uint16_t);
    ESP_ERROR_CHECK(spi_bus_initialize(SPI2_HOST, &buscfg, SPI_DMA_CH_AUTO));

    // 1. Initialize IO Expander and perform hardware reset pulse (fixes SPI glitch)
    io_expander_init();

    // 2. Initialize LCD SPI bus & ST7796 panel. Pass full frame max transfer size!
    esp_3inch5_display_port_init(&io_handle, &panel_handle, EXAMPLE_LCD_H_RES * EXAMPLE_LCD_V_RES * sizeof(uint16_t));

    // 3. Initialize FT6336 Touch controller
    esp_3inch5_touch_port_init(&touch_handle, i2c_bus_handle, EXAMPLE_LCD_H_RES, EXAMPLE_LCD_V_RES, EXAMPLE_DISPLAY_ROTATION);

    // 4. Initialize PMIC (AXP2101) & enable 3.3V power rails
    esp_axp2101_port_init(i2c_bus_handle);
    vTaskDelay(pdMS_TO_TICKS(100));

    // 5. Initialize LEDC Backlight (Keep at 0% until splash logo renders to prevent white screen)
    esp_3inch5_brightness_port_init();
    esp_3inch5_brightness_port_set(0);

    // 6. Initialize LVGL port & attach display + touch
    lv_port_init();

    ESP_LOGI(TAG, "Display and LVGL port successfully initialized!");
    return ESP_OK;
}

#include "esp_lvgl_port.h"

static lv_timer_t *g_rotation_timer = NULL;

static void save_rotation_nvs_task(void *arg)
{
    uint8_t rotation = (uint8_t)(uintptr_t)arg;
    nvs_handle_t handle;
    if (nvs_open("display", NVS_READWRITE, &handle) == ESP_OK) {
        nvs_set_u8(handle, "rotation", rotation);
        nvs_commit(handle);
        nvs_close(handle);
        ESP_LOGI(TAG, "Saved display rotation to NVS: %d", rotation);
    }
    vTaskDelete(NULL);
}

static void rotation_deferred_cb(lv_timer_t *timer)
{
    uint8_t rotation = (uint8_t)(uintptr_t)timer->user_data;
    g_rotation_timer = NULL;

    lv_disp_rot_t rot = LV_DISP_ROT_NONE;
    switch (rotation) {
        case 1: rot = LV_DISP_ROT_90; break;
        case 2: rot = LV_DISP_ROT_180; break;
        case 3: rot = LV_DISP_ROT_270; break;
        default: rot = LV_DISP_ROT_NONE; break;
    }

    if (lvgl_disp) {
        // Clear any active touch / press tracking before resizing the screen
        lv_indev_reset(NULL, NULL);

        lv_disp_set_rotation(lvgl_disp, rot);

        // Reset indev again so new coordinates start completely fresh
        lv_indev_reset(NULL, NULL);
    }

    // Save to NVS in a background worker task so flash write never blocks graphics
    xTaskCreate(save_rotation_nvs_task, "save_rot_nvs", 2048, (void*)(uintptr_t)rotation, 2, NULL);
}

void display_port_set_rotation_locked(uint8_t rotation)
{
    // Save to NVS immediately so even if power is switched off immediately, rotation is persisted
    nvs_handle_t handle;
    if (nvs_open("display", NVS_READWRITE, &handle) == ESP_OK) {
        nvs_set_u8(handle, "rotation", rotation);
        nvs_commit(handle);
        nvs_close(handle);
        ESP_LOGI(TAG, "Saved display rotation to NVS immediately: %d", rotation);
    }

    // If a rotation timer is already pending, delete it first
    if (g_rotation_timer) {
        lv_timer_del(g_rotation_timer);
        g_rotation_timer = NULL;
    }
    // Schedule rotation 150ms later inside the LVGL thread.
    // This allows the switch/button animation to complete visibly on screen and touch release to finish!
    g_rotation_timer = lv_timer_create(rotation_deferred_cb, 150, (void*)(uintptr_t)rotation);
    if (g_rotation_timer) {
        lv_timer_set_repeat_count(g_rotation_timer, 1);
    }
}

static void display_port_apply_rotation_direct(uint8_t rotation)
{
    lv_disp_rot_t rot = LV_DISP_ROT_NONE;
    switch (rotation) {
        case 1: rot = LV_DISP_ROT_90; break;
        case 2: rot = LV_DISP_ROT_180; break;
        case 3: rot = LV_DISP_ROT_270; break;
        default: rot = LV_DISP_ROT_NONE; break;
    }
    if (lvgl_disp) {
        lv_indev_reset(NULL, NULL);
        lv_disp_set_rotation(lvgl_disp, rot);
        lv_indev_reset(NULL, NULL);
    }
}

void display_port_set_rotation(uint8_t rotation)
{
    if (lvgl_port_lock(-1)) {
        display_port_apply_rotation_direct(rotation);
        lvgl_port_unlock();
    }
    // Save to NVS
    nvs_handle_t handle;
    if (nvs_open("display", NVS_READWRITE, &handle) == ESP_OK) {
        nvs_set_u8(handle, "rotation", rotation);
        nvs_commit(handle);
        nvs_close(handle);
    }
}

void display_port_load_rotation()
{
    uint8_t rotation = 1; // Default Landscape mode
    nvs_handle_t handle;
    if (nvs_open("display", NVS_READONLY, &handle) == ESP_OK) {
        nvs_get_u8(handle, "rotation", &rotation);
        nvs_close(handle);
    }
    ESP_LOGI(TAG, "Loading display rotation: %d", rotation);
    if (lvgl_port_lock(-1)) {
        display_port_apply_rotation_direct(rotation);
        lvgl_port_unlock();
    }
}

void display_port_set_backlight(uint8_t brightness)
{
    esp_3inch5_brightness_port_set(brightness);
}

