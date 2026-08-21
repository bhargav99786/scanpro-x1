/*
  ============================================================================
  tca9554.h — minimal driver for the onboard TCA9554 I2C IO-expander
  ----------------------------------------------------------------------------
  Waveshare wires LCD_CS, LCD_RST and LCD backlight (and sometimes SD_CS /
  touch reset) through this 8-bit expander instead of direct ESP32 GPIOs, to
  save pins. Standard registers:
    0x00 = Input Port register  (read pin states)
    0x01 = Output Port register (write pin states)
    0x03 = Configuration register (1 = input, 0 = output; default all-input)
  ============================================================================
*/
#pragma once
#include <Wire.h>
#include "pins_config.h"

class TCA9554 {
public:
  bool begin(uint8_t addr = TCA9554_I2C_ADDR) {
    _addr = addr;
    // Configure all 8 pins as outputs (0 = output) since we only drive
    // CS/RST/BL/etc from the MCU side.
    return writeReg(0x03, 0x00);
  }

  void digitalWrite(uint8_t pin, bool level) {
    if (level) _outputShadow |= (1 << pin);
    else       _outputShadow &= ~(1 << pin);
    writeReg(0x01, _outputShadow);
  }

  // Convenience helpers matching pins_config.h names
  void setLcdRST(bool level) { digitalWrite(EXPANDER_LCD_RST, level); }
  void setSpeakerPA(bool level) { digitalWrite(7, level); } // Pin P7 controls Speaker PA on 3.5" LCD board

private:
  uint8_t _addr = TCA9554_I2C_ADDR;
  uint8_t _outputShadow = 0xFF; // default high (most expander outputs are active-low CS/RST)

  bool writeReg(uint8_t reg, uint8_t val) {
    Wire.beginTransmission(_addr);
    Wire.write(reg);
    Wire.write(val);
    return Wire.endTransmission() == 0;
  }
};
