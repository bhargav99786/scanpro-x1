#include "esp_es8311_port.h"
#include "pins_config.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/i2s_std.h"
#include "driver/gpio.h"
#include "esp_codec_dev.h"
#include "esp_codec_dev_defaults.h"
#include <math.h>
#include <string.h>

static const char *TAG = "ES8311_PORT";

static i2s_chan_handle_t tx_chan = NULL;
static i2s_chan_handle_t rx_chan = NULL;
static esp_codec_dev_handle_t output_dev = NULL;
static esp_codec_dev_handle_t input_dev = NULL;

esp_err_t esp_es8311_port_init(i2c_master_bus_handle_t i2c_bus)
{
    ESP_LOGI(TAG, "Initializing ES8311 Hardware Subsystem...");

    // 1. Enable PA_CTRL_PIN (GPIO 17) for Hardware Speaker Power Amplifier
    gpio_config_t io_conf = {};
    io_conf.intr_type = GPIO_INTR_DISABLE;
    io_conf.mode = GPIO_MODE_OUTPUT;
    io_conf.pin_bit_mask = (1ULL << PA_CTRL_PIN);
    io_conf.pull_down_en = GPIO_PULLDOWN_DISABLE;
    io_conf.pull_up_en = GPIO_PULLUP_ENABLE;
    gpio_config(&io_conf);
    gpio_set_level(PA_CTRL_PIN, 1);
    ESP_LOGI(TAG, "Speaker Amplifier GPIO17 set HIGH.");

    // 2. Initialize I2S Standard Master Channels (Full-Duplex TX + RX for ES8311 PLL Clocking)
    i2s_chan_config_t chan_cfg = I2S_CHANNEL_DEFAULT_CONFIG(I2S_NUM_0, I2S_ROLE_MASTER);
    chan_cfg.auto_clear = true;
    ESP_ERROR_CHECK(i2s_new_channel(&chan_cfg, &tx_chan, &rx_chan));

    i2s_std_config_t std_cfg = {
        .clk_cfg = I2S_STD_CLK_DEFAULT_CONFIG(AUDIO_SAMPLE_RATE),
        .slot_cfg = I2S_STD_PHILIPS_SLOT_DEFAULT_CONFIG(I2S_DATA_BIT_WIDTH_16BIT, I2S_SLOT_MODE_STEREO),
        .gpio_cfg = {
            .mclk = I2S_MCK_PIN,   // GPIO 12
            .bclk = I2S_BCK_PIN,   // GPIO 13
            .ws = I2S_LRCK_PIN,    // GPIO 15
            .dout = I2S_DOUT_PIN,  // GPIO 16 (ESP32 DOUT -> ES8311 SDIN, Playback)
            .din = I2S_DIN_PIN,    // GPIO 14 (ES8311 SDOUT -> ESP32 DIN, Recording)
            .invert_flags = {
                .mclk_inv = false,
                .bclk_inv = false,
                .ws_inv = false,
            },
        },
    };

    ESP_ERROR_CHECK(i2s_channel_init_std_mode(tx_chan, &std_cfg));
    ESP_ERROR_CHECK(i2s_channel_init_std_mode(rx_chan, &std_cfg));
    ESP_ERROR_CHECK(i2s_channel_enable(tx_chan));
    ESP_ERROR_CHECK(i2s_channel_enable(rx_chan));
    ESP_LOGI(TAG, "I2S Master channels initialized & enabled (MCK=12, BCK=13, WS=15, DOUT=16, DIN=14).");

    // 3. Initialize ES8311 Codec via esp_codec_dev
    audio_codec_i2c_cfg_t i2c_cfg = {
        .port = 0,
        .addr = ES8311_CODEC_DEFAULT_ADDR,
        .bus_handle = i2c_bus,
    };
    const audio_codec_ctrl_if_t *out_ctrl_if = audio_codec_new_i2c_ctrl(&i2c_cfg);

    audio_codec_i2s_cfg_t i2s_cfg = {
        .port = 0,
        .rx_handle = rx_chan,
        .tx_handle = tx_chan,
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

    // Create Output Device (Speaker - Mono)
    esp_codec_dev_cfg_t dev_cfg = {
        .dev_type = ESP_CODEC_DEV_TYPE_OUT,
        .codec_if = codec_if,
        .data_if = out_data_if,
    };
    output_dev = esp_codec_dev_new(&dev_cfg);
    esp_codec_set_disable_when_closed(output_dev, false);

    // Create Input Device (Microphone - Mono)
    dev_cfg.dev_type = ESP_CODEC_DEV_TYPE_IN;
    input_dev = esp_codec_dev_new(&dev_cfg);
    esp_codec_set_disable_when_closed(input_dev, false);

    esp_codec_dev_sample_info_t fs = {
        .bits_per_sample = 16,
        .channel = 1,
        .channel_mask = 0,
        .sample_rate = AUDIO_SAMPLE_RATE,
        .mclk_multiple = 0,
    };

    if (esp_codec_dev_open(output_dev, &fs) == ESP_CODEC_DEV_OK &&
        esp_codec_dev_open(input_dev, &fs) == ESP_CODEC_DEV_OK) {
        esp_codec_dev_set_out_vol(output_dev, AUDIO_DEFAULT_VOL);
        esp_codec_dev_set_out_mute(output_dev, false); // Explicitly un-mute DAC
        esp_codec_dev_set_in_gain(input_dev, MIC_GAIN_DB);
        esp_codec_dev_set_in_mute(input_dev, false);   // Explicitly un-mute ADC

        // Force register overrides for ES8311 hardware ADC unmute + moderate PGA gain (+30dB)
        esp_codec_dev_write_reg(input_dev, 0x14, 0x1A); // LIN1/RIN1 Analog Mic + PGA
        esp_codec_dev_write_reg(input_dev, 0x16, 0x55); // Moderate PGA Gain (+30dB)
        esp_codec_dev_write_reg(input_dev, 0x17, 0xBF); // ADC Digital Volume: 0xBF = 0dB

        ESP_LOGI(TAG, "ES8311 Codec Speaker + Microphone hardware initialized successfully!");
    } else {
        ESP_LOGE(TAG, "ES8311 Codec open failed!");
    }

    // Play startup beep tone
    audio_play_beep(1000, 150);
    return ESP_OK;
}

void audio_play_beep(uint16_t freq_hz, uint16_t duration_ms)
{
    if (!output_dev) return;

    size_t samples = (AUDIO_SAMPLE_RATE * duration_ms) / 1000;
    int16_t *buf = (int16_t *)malloc(samples * sizeof(int16_t));
    if (!buf) return;

    const uint32_t fade = AUDIO_SAMPLE_RATE / 100; // 10ms fade in/out
    float omega = 2.0f * M_PI * freq_hz / (float)AUDIO_SAMPLE_RATE;
    for (size_t i = 0; i < samples; i++) {
        int16_t s = (int16_t)(sinf(omega * i) * 20000.0f); // 20000 amplitude
        if      (i < fade)                   s = (int16_t)((float)s * i / fade);
        else if (i > samples - fade)         s = (int16_t)((float)s * (samples - i) / fade);
        buf[i] = s;
    }

    esp_codec_dev_write(output_dev, buf, samples * sizeof(int16_t));
    free(buf);
}

void beepSuccess()
{
    audio_play_beep(2500, 50);
}

void beepError()
{
    audio_play_beep(400, 100);
    vTaskDelay(pdMS_TO_TICKS(50));
    audio_play_beep(300, 150);
}

void beepStartup()
{
    audio_play_beep(800, 60);
    vTaskDelay(pdMS_TO_TICKS(40));
    audio_play_beep(1200, 60);
    vTaskDelay(pdMS_TO_TICKS(40));
    audio_play_beep(1600, 80);
}

int esp_es8311_play(const uint8_t *buf, size_t len)
{
    if (!output_dev || !buf || len == 0) return 0;
    return esp_codec_dev_write(output_dev, (void *)buf, (int)len);
}

static int16_t prev_mic_sample = 0;

int esp_es8311_record(uint8_t *buf, size_t len)
{
    if (!input_dev || !buf || len == 0) return -1;
    esp_err_t err = esp_codec_dev_read(input_dev, buf, (int)len);
    if (err != ESP_CODEC_DEV_OK) return -1;

    // Filter PCM data: Noise Gate + Low-Pass Filter to eliminate acoustic whistle
    int16_t *pcm16 = (int16_t *)buf;
    int samples = len / sizeof(int16_t);

    for (int i = 0; i < samples; i++) {
        int32_t raw_val = pcm16[i];
        int32_t abs_val = abs(raw_val);

        // Noise Gate: silence low-level background noise (< 300 amplitude)
        if (abs_val < 300) {
            raw_val = 0;
        }

        // 1st-order Low-Pass Filter (alpha = 0.6 sample, 0.4 prev_mic_sample)
        int32_t filtered_val = (raw_val * 6 + (int32_t)prev_mic_sample * 4) / 10;
        prev_mic_sample = (int16_t)filtered_val;

        // 2x boost for clear speech output
        filtered_val = filtered_val * 2;
        if (filtered_val > 32767) filtered_val = 32767;
        if (filtered_val < -32768) filtered_val = -32768;
        pcm16[i] = (int16_t)filtered_val;
    }

    return (int)len;
}
