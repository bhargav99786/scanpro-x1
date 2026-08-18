/*
  ============================================================================
  pins_config.h
  ----------------------------------------------------------------------------
  Waveshare ESP32-S3-Touch-LCD-3.5 (ST7796 SPI LCD + FT6336 I2C touch +
  TCA9554 I2C IO-expander for LCD_CS/LCD_RST/Backlight).

  ⚠️ IMPORTANT — READ THIS FIRST ⚠️
  I could not verify the exact GPIO numbers Waveshare wired on your specific
  board revision from public documentation alone (Waveshare's own wiki only
  shows macro names like LCD_DC/LCD_CS/SPI_SCLK in example code, not the
  numeric values, and does not publish a plain-text pin table for this exact
  model). Rather than guess and hand you numbers that might silently fail,
  every value below is a PLACEHOLDER commented "VERIFY".

  How to get the real numbers (2 minutes):
    1. Download the official demo package (link is on the product wiki page,
       "Resources" -> "Demo"):
       https://files.waveshare.com/wiki/ESP32-S3-Touch-LCD-3.5/ESP32-S3-Touch-LCD-3.5-Demo.zip
    2. Open Arduino/08_gfx_helloworld/ (or any 0x_gfx_*11_lvgl_* example)
    3. Find the file that defines LCD_DC, LCD_CS, SPI_SCLK, SPI_MOSI,
       SPI_MISO, LCD_RST, I2C_SDA, I2C_SCL,TP_INT (touch interrupt) —
       it's usually named pin_config.h or similar inside that example folder.
    4. Copy those exact numbers into the VERIFY lines below.

  Everything else in this project (LVGL screens, GM65 scanning, WiFi/MQTT)
  does NOT depend on these exact numbers and will work unchanged once you
  drop in the correct values here.
  ============================================================================
*/

#pragma once

// ---------- LCD SPI bus (ST7796, 320x480) ----------
#define LCD_SPI_SCLK      5    
#define LCD_SPI_MOSI      1    
#define LCD_SPI_MISO      2    
#define LCD_DC            3    
#define GFX_BL            6
#define LCD_HOR_RES       320
#define LCD_VER_RES       480

// TCA9554 IO-expander (I2C)
#define TCA9554_I2C_ADDR  0x20  
#define EXPANDER_LCD_RST  1     // TCA9554 P1

// ---------- I2C bus (touch FT6336, IMU, RTC, PMIC, IO-expander) ----------
#define I2C_SDA           8    
#define I2C_SCL           7    
#define TP_INT            -1   
#define FT6336_I2C_ADDR   0x38 

// ---------- GM65 barcode scanner module (UART1) ----------
#define GM65_RX_PIN       10   // GM65 TX  → ESP32 GPIO10
#define GM65_TX_PIN       11   // GM65 RX  → ESP32 GPIO11
#define GM65_BAUD         9600
#define GM65_TRIGGER_PIN  -1

// ---------- Onboard buttons & Battery ADC ----------
#define BTN_BOOT_PIN      0    // ESP32-S3 BOOT button, GPIO0
#define BATTERY_ADC_PIN   4    // Battery voltage ADC pin (GPIO4)

// ---------- ES8311 Audio Codec (I2S + I2C) ----------
// I2C shared with touch (SDA=8, SCL=7 above)
#define I2S_MCK_PIN       12
#define I2S_BCK_PIN       13
#define I2S_LRCK_PIN      15
#define I2S_DOUT_PIN      16   // ESP32 → ES8311 DAC
#define I2S_DIN_PIN       14   // ES8311 ADC → ESP32 (unused)
#define AUDIO_SAMPLE_RATE 44100
#define AUDIO_MCLK_MULT   256
#define AUDIO_VOLUME      80   // 0–100
