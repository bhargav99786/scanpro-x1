#pragma once
#include "driver/gpio.h"

// ---------- LCD SPI bus (ST7796, 320x480) ----------
#define LCD_SPI_SCLK      (gpio_num_t)5
#define LCD_SPI_MOSI      (gpio_num_t)1
#define LCD_SPI_MISO      (gpio_num_t)-1
#define LCD_DC            (gpio_num_t)3
#define GFX_BL            (gpio_num_t)6
#define LCD_HOR_RES       480
#define LCD_VER_RES       320

// TCA9554 IO-expander (I2C)
#define TCA9554_I2C_ADDR  0x20  
#define EXPANDER_LCD_RST  1     // TCA9554 P1
#define EXPANDER_PA_CTRL  7     // TCA9554 P7 (Speaker PA)

// ---------- I2C bus (touch FT6336, TCA9554, ES8311) ----------
#define I2C_SDA_PIN       (gpio_num_t)8
#define I2C_SCL_PIN       (gpio_num_t)7
#define I2C_NUM_PORT      I2C_NUM_0

// ---------- GM65 barcode scanner module (UART1) ----------
#define GM65_RX_PIN       (gpio_num_t)10   // GM65 TX  → ESP32 GPIO10
#define GM65_TX_PIN       (gpio_num_t)11   // GM65 RX  → ESP32 GPIO11
#define GM65_BAUD_RATE    9600

// ---------- ES8311 Audio Codec (I2S + I2C) ----------
#define I2S_MCK_PIN       (gpio_num_t)12
#define I2S_BCK_PIN       (gpio_num_t)13
#define I2S_LRCK_PIN      (gpio_num_t)15
#define I2S_DOUT_PIN      (gpio_num_t)16   // ESP32 Data OUT -> ES8311 SDIN (GPIO16, Speaker/Playback)
#define I2S_DIN_PIN       (gpio_num_t)14   // ES8311 SDOUT -> ESP32 Data IN (GPIO14, Microphone/Recording)
#define PA_CTRL_PIN       (gpio_num_t)17   // Onboard Speaker Power Amplifier Enable (GPIO17)

#define AUDIO_SAMPLE_RATE 16000
#define AUDIO_MCLK_MULT   256
#define AUDIO_DEFAULT_VOL 90
#define MIC_GAIN_DB       30.0f

// ---------- Wi-Fi & MQTT Fallback Config ----------
#define DEFAULT_WIFI_SSID "waveshare"
#define DEFAULT_WIFI_PASS "12345678"
#define DEFAULT_MQTT_HOST "192.168.0.112"
#define DEFAULT_MQTT_PORT 1883
#define DEVICE_ID         "scanpro-test-01"
