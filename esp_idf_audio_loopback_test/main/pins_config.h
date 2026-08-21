#pragma once
#include "driver/gpio.h"

// ---------- I2C Shared Bus (SDA=8, SCL=7) ----------
#define I2C_SDA_PIN       (gpio_num_t)8
#define I2C_SCL_PIN       (gpio_num_t)7

// ---------- ES8311 Audio Codec (I2S) ----------
#define I2S_MCK_PIN       (gpio_num_t)12
#define I2S_BCK_PIN       (gpio_num_t)13
#define I2S_LRCK_PIN      (gpio_num_t)15

// I2S Data Pins (Playback DOUT=16, Recording DIN=14 matching Waveshare factory code)
#define I2S_DOUT_PIN      (gpio_num_t)16   // ESP32 DOUT -> ES8311 SDIN
#define I2S_DIN_PIN       (gpio_num_t)14   // ES8311 SDOUT -> ESP32 DIN

// Onboard Speaker Power Amplifier Enable Pin
#define PA_CTRL_PIN       (gpio_num_t)17

#define AUDIO_SAMPLE_RATE 16000
#define AUDIO_VOLUME      90.0f
#define MIC_GAIN_DB       30.0f
