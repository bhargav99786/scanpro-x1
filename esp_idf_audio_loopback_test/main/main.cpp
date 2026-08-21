/*
  ============================================================================
  MIC & SPEAKER DIAGNOSTIC AND DIRECT LOOPBACK TEST FOR ESP32-S3 TOUCH LCD 3.5
  ----------------------------------------------------------------------------
  Match exact working factory setup:
  1. I2S Pins: MCK=12, BCK=13, LRCK=15, DOUT=16 (Playback TX), DIN=14 (Record RX)
  2. Speaker PA Enable: TCA9554 IO Expander P7 HIGH + GPIO17 HIGH
  3. ES8311 ADC Gain & Unmute Registers: 0x14=0x1A, 0x16=0x07, 0x17=0x00
  4. Diagnostic PCM sample logging in serial console.
  ============================================================================
*/

#include <stdio.h>
#include <math.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "esp_system.h"
#include "esp_log.h"
#include "driver/gpio.h"
#include "driver/i2c_master.h"
#include "driver/i2s_std.h"
#include "esp_codec_dev.h"
#include "esp_codec_dev_defaults.h"
#include "pins_config.h"

static const char *TAG = "AUDIO_TEST";

static i2s_chan_handle_t tx_chan = NULL;
static i2s_chan_handle_t rx_chan = NULL;
static esp_codec_dev_handle_t output_dev = NULL;
static esp_codec_dev_handle_t input_dev = NULL;

static QueueHandle_t audio_loopback_queue = NULL;

static void enable_speaker_pa(i2c_master_bus_handle_t i2c_bus)
{
    // 1. Set GPIO 17 HIGH
    gpio_config_t io_conf = {};
    io_conf.intr_type = GPIO_INTR_DISABLE;
    io_conf.mode = GPIO_MODE_OUTPUT;
    io_conf.pin_bit_mask = (1ULL << PA_CTRL_PIN);
    io_conf.pull_down_en = GPIO_PULLDOWN_DISABLE;
    io_conf.pull_up_en = GPIO_PULLUP_ENABLE;
    gpio_config(&io_conf);
    gpio_set_level(PA_CTRL_PIN, 1);
    ESP_LOGI(TAG, "Speaker PA GPIO 17 driven HIGH.");

    // 2. Set Pin P7 HIGH on TCA9554 IO Expander (I2C Address 0x20)
    i2c_device_config_t dev_cfg = {};
    dev_cfg.dev_addr_length = I2C_ADDR_BIT_LEN_7;
    dev_cfg.device_address = 0x20;
    dev_cfg.scl_speed_hz = 100000;

    i2c_master_dev_handle_t exp_dev = NULL;
    if (i2c_master_bus_add_device(i2c_bus, &dev_cfg, &exp_dev) == ESP_OK) {
        // Reg 3 (Config): Set P7 as Output (0x00)
        uint8_t cfg_cmd[2] = {0x03, 0x00};
        i2c_master_transmit(exp_dev, cfg_cmd, 2, 100);
        // Reg 1 (Output): Set P7 HIGH (0x80)
        uint8_t out_cmd[2] = {0x01, 0x80};
        i2c_master_transmit(exp_dev, out_cmd, 2, 100);
        ESP_LOGI(TAG, "TCA9554 IO Expander Pin P7 set HIGH (Speaker Amp ON).");
    }
}

static void play_test_tone(uint16_t freq_hz, uint16_t duration_ms)
{
    if (!output_dev) return;

    size_t samples = (AUDIO_SAMPLE_RATE * duration_ms) / 1000;
    int16_t *buf = (int16_t *)malloc(samples * sizeof(int16_t));
    if (!buf) return;

    const uint32_t fade = AUDIO_SAMPLE_RATE / 100;
    float omega = 2.0f * M_PI * freq_hz / (float)AUDIO_SAMPLE_RATE;
    for (size_t i = 0; i < samples; i++) {
        int16_t s = (int16_t)(sinf(omega * i) * 20000.0f);
        if      (i < fade)           s = (int16_t)((float)s * i / fade);
        else if (i > samples - fade) s = (int16_t)((float)s * (samples - i) / fade);
        buf[i] = s;
    }

    esp_codec_dev_write(output_dev, buf, samples * sizeof(int16_t));
    free(buf);
}

// -----------------------------------------------------------------------------
// Task 1: Microphone Capture Task (Reads raw PCM from ES8311 ADC)
// -----------------------------------------------------------------------------
static void mic_capture_task(void *pvParameters)
{
    const size_t chunk_size = 1024;
    uint8_t data[1024];

    // Initial read to trigger codec input driver enable
    esp_codec_dev_read(input_dev, data, chunk_size);

    // Apply official esp_codec_dev mic settings
    esp_codec_dev_set_in_mute(input_dev, false);
    esp_codec_dev_set_in_gain(input_dev, MIC_GAIN_DB);

    // Force register overrides for ES8311 hardware ADC unmute + moderate PGA gain (+30dB)
    esp_codec_dev_write_reg(input_dev, 0x14, 0x1A); // LIN1/RIN1 Analog Mic + PGA
    esp_codec_dev_write_reg(input_dev, 0x16, 0x55); // Moderate PGA Gain (+30dB)
    esp_codec_dev_write_reg(input_dev, 0x17, 0xBF); // ADC Digital Volume: 0xBF = 0dB

    ESP_LOGI(TAG, "Microphone capture task active with Noise Gate & Anti-Echo Filter.");
    uint32_t last_log = 0;
    static int16_t prev_sample = 0;

    while (1) {
        int ret = esp_codec_dev_read(input_dev, data, chunk_size);
        if (ret == ESP_CODEC_DEV_OK) {
            int16_t *pcm16 = (int16_t *)data;
            int samples = chunk_size / sizeof(int16_t);

            // 1. Noise Gate Threshold (Suppresses low-level ambient room feedback)
            // 2. Low-Pass Filter (Dampens high frequency whistling/screeching echo)
            int16_t max_amp = 0;
            for (int i = 0; i < samples; i++) {
                int32_t raw_val = pcm16[i];
                int32_t abs_val = abs(raw_val);
                if (abs_val > max_amp) max_amp = abs_val;

                // Noise Gate: silence low-level background noise (< 300 amplitude)
                if (abs_val < 300) {
                    raw_val = 0;
                }

                // 1st-order Low-Pass Filter (alpha = 0.6 sample, 0.4 prev_sample)
                int32_t filtered_val = (raw_val * 6 + (int32_t)prev_sample * 4) / 10;
                prev_sample = (int16_t)filtered_val;

                // 2x boost for clear speaker playback
                filtered_val = filtered_val * 2;
                if (filtered_val > 32767) filtered_val = 32767;
                if (filtered_val < -32768) filtered_val = -32768;
                pcm16[i] = (int16_t)filtered_val;
            }

            // Push audio frame to playback queue
            if (audio_loopback_queue) {
                xQueueSend(audio_loopback_queue, data, 0);
            }

            uint32_t now = xTaskGetTickCount() * portTICK_PERIOD_MS;
            if (now - last_log >= 1000) {
                last_log = now;
                ESP_LOGI(TAG, "[MIC CAPTURE] Peak Level: %d / 32767 | Filtered Samples: [%d, %d, %d, %d]",
                         max_amp, pcm16[0], pcm16[1], pcm16[2], pcm16[3]);
            }
        } else {
            vTaskDelay(pdMS_TO_TICKS(5));
        }
    }
}

// -----------------------------------------------------------------------------
// Task 2: Speaker Playback Task (Plays PCM to ES8311 DAC Speaker)
// -----------------------------------------------------------------------------
static void speaker_playback_task(void *pvParameters)
{
    uint8_t data[1024];
    esp_codec_dev_set_out_vol(output_dev, AUDIO_VOLUME);
    esp_codec_dev_set_out_mute(output_dev, false);

    ESP_LOGI(TAG, "Speaker playback task active. Output Volume: %.1f", AUDIO_VOLUME);

    uint32_t frames_played = 0;
    uint32_t last_log = 0;

    while (1) {
        if (xQueueReceive(audio_loopback_queue, data, portMAX_DELAY) == pdTRUE) {
            int err = esp_codec_dev_write(output_dev, data, sizeof(data));
            if (err == ESP_CODEC_DEV_OK) {
                frames_played++;
            } else {
                ESP_LOGE(TAG, "esp_codec_dev_write failed: %d", err);
            }

            uint32_t now = xTaskGetTickCount() * portTICK_PERIOD_MS;
            if (now - last_log >= 2000) {
                last_log = now;
                ESP_LOGI(TAG, "[SPEAKER PLAYBACK] Frames Played: %lu", (unsigned long)frames_played);
            }
        }
    }
}

extern "C" void app_main(void)
{
    ESP_LOGI(TAG, "Starting Standalone ESP32-S3 Mic-to-Speaker Audio Test...");

    // 1. Initialize Shared I2C Master Bus (SDA=8, SCL=7)
    i2c_master_bus_config_t i2c_mst_config = {};
    i2c_mst_config.clk_source = I2C_CLK_SRC_DEFAULT;
    i2c_mst_config.i2c_port = I2C_NUM_0;
    i2c_mst_config.scl_io_num = I2C_SCL_PIN;
    i2c_mst_config.sda_io_num = I2C_SDA_PIN;
    i2c_mst_config.glitch_ignore_cnt = 7;
    i2c_mst_config.flags.enable_internal_pullup = true;

    i2c_master_bus_handle_t i2c_bus = NULL;
    ESP_ERROR_CHECK(i2c_new_master_bus(&i2c_mst_config, &i2c_bus));
    ESP_LOGI(TAG, "I2C Master Bus initialized.");

    // 2. Enable Speaker Power Amplifier (GPIO 17 + TCA9554 Expander Pin P7)
    enable_speaker_pa(i2c_bus);

    // 3. Initialize I2S Master Channels (Full-Duplex TX + RX)
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
            .dout = I2S_DOUT_PIN,  // GPIO 16 (Playback data output)
            .din = I2S_DIN_PIN,    // GPIO 14 (Recording data input)
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
    ESP_LOGI(TAG, "I2S Channels initialized (MCK=12, BCK=13, WS=15, DOUT=16, DIN=14).");

    // 4. Initialize ES8311 Codec via esp_codec_dev
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
        esp_codec_dev_set_out_vol(output_dev, AUDIO_VOLUME);
        esp_codec_dev_set_out_mute(output_dev, false);
        esp_codec_dev_set_in_gain(input_dev, MIC_GAIN_DB);
        esp_codec_dev_set_in_mute(input_dev, false);
        ESP_LOGI(TAG, "ES8311 Codec Speaker + Mic hardware initialized!");
    } else {
        ESP_LOGE(TAG, "ES8311 Codec open failed!");
        return;
    }

    // Play startup beep tone to verify speaker output hardware
    ESP_LOGI(TAG, "Testing speaker playback hardware with startup beep tones...");
    play_test_tone(800, 100);  vTaskDelay(pdMS_TO_TICKS(50));
    play_test_tone(1200, 100); vTaskDelay(pdMS_TO_TICKS(50));
    play_test_tone(1600, 150); vTaskDelay(pdMS_TO_TICKS(100));

    // Create Queue for decoupling Mic Capture and Speaker Playback
    audio_loopback_queue = xQueueCreate(20, 1024);

    // Launch concurrent FreeRTOS tasks
    xTaskCreatePinnedToCore(mic_capture_task, "mic_capture_task", 4096, NULL, 5, NULL, 1);
    xTaskCreatePinnedToCore(speaker_playback_task, "speaker_playback_task", 4096, NULL, 5, NULL, 1);
}
