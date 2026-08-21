#include "esp_es8311_port.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/i2s_std.h"
#include "driver/gpio.h"
#include "esp_codec_dev.h"
#include "esp_codec_dev_defaults.h"
#include <math.h>
#include <string.h>

static const char *TAG = "ES8311_COMP_PORT";

#define COMP_I2S_MCK_PIN       (gpio_num_t)12
#define COMP_I2S_BCK_PIN       (gpio_num_t)13
#define COMP_I2S_LRCK_PIN      (gpio_num_t)15
#define COMP_I2S_DOUT_PIN      (gpio_num_t)16   // ESP32 Data OUT -> ES8311 SDIN
#define COMP_I2S_DIN_PIN       (gpio_num_t)14   // ES8311 SDOUT -> ESP32 Data IN
#define COMP_PA_CTRL_PIN       (gpio_num_t)17

#define COMP_AUDIO_SAMPLE_RATE 16000
#define COMP_AUDIO_DEFAULT_VOL 90
#define COMP_MIC_GAIN_DB       30.0f

static i2s_chan_handle_t tx_chan_comp = NULL;
static i2s_chan_handle_t rx_chan_comp = NULL;
static esp_codec_dev_handle_t output_dev_comp = NULL;
static esp_codec_dev_handle_t input_dev_comp = NULL;

void esp_es8311_port_init(i2c_master_bus_handle_t bus_handle)
{
    ESP_LOGI(TAG, "Initializing ES8311 Hardware Subsystem (Component)...");

    // 1. Enable PA_CTRL_PIN (GPIO 17)
    gpio_config_t io_conf = {};
    io_conf.intr_type = GPIO_INTR_DISABLE;
    io_conf.mode = GPIO_MODE_OUTPUT;
    io_conf.pin_bit_mask = (1ULL << COMP_PA_CTRL_PIN);
    io_conf.pull_down_en = GPIO_PULLDOWN_DISABLE;
    io_conf.pull_up_en = GPIO_PULLUP_ENABLE;
    gpio_config(&io_conf);
    gpio_set_level(COMP_PA_CTRL_PIN, 1);

    // 2. Initialize I2S Master Channels
    i2s_chan_config_t chan_cfg = I2S_CHANNEL_DEFAULT_CONFIG(I2S_NUM_0, I2S_ROLE_MASTER);
    chan_cfg.auto_clear = true;
    ESP_ERROR_CHECK(i2s_new_channel(&chan_cfg, &tx_chan_comp, &rx_chan_comp));

    i2s_std_config_t std_cfg = {
        .clk_cfg = I2S_STD_CLK_DEFAULT_CONFIG(COMP_AUDIO_SAMPLE_RATE),
        .slot_cfg = I2S_STD_PHILIPS_SLOT_DEFAULT_CONFIG(I2S_DATA_BIT_WIDTH_16BIT, I2S_SLOT_MODE_STEREO),
        .gpio_cfg = {
            .mclk = COMP_I2S_MCK_PIN,
            .bclk = COMP_I2S_BCK_PIN,
            .ws = COMP_I2S_LRCK_PIN,
            .dout = COMP_I2S_DOUT_PIN, // GPIO 16
            .din = COMP_I2S_DIN_PIN,   // GPIO 14
            .invert_flags = {
                .mclk_inv = false,
                .bclk_inv = false,
                .ws_inv = false,
            },
        },
    };

    ESP_ERROR_CHECK(i2s_channel_init_std_mode(tx_chan_comp, &std_cfg));
    ESP_ERROR_CHECK(i2s_channel_init_std_mode(rx_chan_comp, &std_cfg));
    ESP_ERROR_CHECK(i2s_channel_enable(tx_chan_comp));
    ESP_ERROR_CHECK(i2s_channel_enable(rx_chan_comp));

    // 3. Initialize ES8311 Codec via esp_codec_dev
    audio_codec_i2c_cfg_t i2c_cfg = {
        .port = 0,
        .addr = ES8311_CODEC_DEFAULT_ADDR,
        .bus_handle = bus_handle,
    };
    const audio_codec_ctrl_if_t *out_ctrl_if = audio_codec_new_i2c_ctrl(&i2c_cfg);

    audio_codec_i2s_cfg_t i2s_cfg = {
        .port = 0,
        .rx_handle = rx_chan_comp,
        .tx_handle = tx_chan_comp,
        .clk_src = 0,
    };
    const audio_codec_data_if_t *out_data_if = audio_codec_new_i2s_data(&i2s_cfg);

    es8311_codec_cfg_t es8311_cfg = {
        .ctrl_if = out_ctrl_if,
        .gpio_if = audio_codec_new_gpio(),
        .codec_mode = ESP_CODEC_DEV_WORK_MODE_BOTH,
        .pa_pin = -1,
        .pa_reverted = false,
        .master_mode = false,
        .use_mclk = true,
        .digital_mic = false,
    };
    es8311_cfg.hw_gain.pa_voltage = 5.0;
    es8311_cfg.hw_gain.codec_dac_voltage = 3.3;

    const audio_codec_if_t *codec_if = es8311_codec_new(&es8311_cfg);

    esp_codec_dev_cfg_t dev_cfg = {
        .dev_type = ESP_CODEC_DEV_TYPE_OUT,
        .codec_if = codec_if,
        .data_if = out_data_if,
    };
    output_dev_comp = esp_codec_dev_new(&dev_cfg);
    esp_codec_set_disable_when_closed(output_dev_comp, false);

    dev_cfg.dev_type = ESP_CODEC_DEV_TYPE_IN;
    input_dev_comp = esp_codec_dev_new(&dev_cfg);
    esp_codec_set_disable_when_closed(input_dev_comp, false);

    esp_codec_dev_sample_info_t fs = {
        .bits_per_sample = 16,
        .channel = 1,
        .channel_mask = 0,
        .sample_rate = COMP_AUDIO_SAMPLE_RATE,
        .mclk_multiple = 0,
    };

    if (esp_codec_dev_open(output_dev_comp, &fs) == ESP_CODEC_DEV_OK &&
        esp_codec_dev_open(input_dev_comp, &fs) == ESP_CODEC_DEV_OK) {
        esp_codec_dev_set_out_vol(output_dev_comp, COMP_AUDIO_DEFAULT_VOL);
        esp_codec_dev_set_out_mute(output_dev_comp, false);
        esp_codec_dev_set_in_gain(input_dev_comp, COMP_MIC_GAIN_DB);
        esp_codec_dev_set_in_mute(input_dev_comp, false);

        // Hardware overrides for ADC path & PGA gain
        esp_codec_dev_write_reg(input_dev_comp, 0x14, 0x1A);
        esp_codec_dev_write_reg(input_dev_comp, 0x16, 0x55);
        esp_codec_dev_write_reg(input_dev_comp, 0x17, 0xBF);
        ESP_LOGI(TAG, "ES8311 Codec component initialized (Mic + Speaker).");
    }
}

esp_err_t esp_es8311_play(const uint8_t *data, size_t len)
{
    if (!output_dev_comp || !data || len == 0) return ESP_FAIL;
    return esp_codec_dev_write(output_dev_comp, (void *)data, (int)len);
}

static int16_t prev_comp_mic_sample = 0;

int esp_es8311_record(uint8_t *data, size_t len)
{
    if (!input_dev_comp || !data || len == 0) return 0;
    esp_err_t err = esp_codec_dev_read(input_dev_comp, data, (int)len);
    if (err != ESP_CODEC_DEV_OK) return 0;

    int16_t *pcm16 = (int16_t *)data;
    int samples = len / sizeof(int16_t);

    for (int i = 0; i < samples; i++) {
        int32_t raw_val = pcm16[i];
        int32_t abs_val = abs(raw_val);
        if (abs_val < 300) raw_val = 0;

        int32_t filtered_val = (raw_val * 6 + (int32_t)prev_comp_mic_sample * 4) / 10;
        prev_comp_mic_sample = (int16_t)filtered_val;

        filtered_val = filtered_val * 2;
        if (filtered_val > 32767) filtered_val = 32767;
        if (filtered_val < -32768) filtered_val = -32768;
        pcm16[i] = (int16_t)filtered_val;
    }

    return (int)len;
}