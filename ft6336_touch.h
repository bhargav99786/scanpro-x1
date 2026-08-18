/*
  ============================================================================
  ft6336_touch.h — minimal register-level driver for the FT6336 touch chip
  ----------------------------------------------------------------------------
  FT6336 shares the same register map as the common FT6206/FT6x36 family:
    0x02        = TD_STATUS (number of touch points, low nibble)
    0x03        = TOUCH1_XH (bits 3:0 = X high nibble; bits 7:6 = event flag)
    0x04        = TOUCH1_XL
    0x05        = TOUCH1_YH
    0x06        = TOUCH1_YL
  Reading over I2C at up to 400kHz is reliable; no external library needed.
  ============================================================================
*/
#pragma once
#include <Wire.h>
#include "pins_config.h"

struct TouchPoint {
  bool touched;
  int16_t x;
  int16_t y;
};

class FT6336 {
public:
  bool begin(uint8_t addr = FT6336_I2C_ADDR) {
    _addr = addr;
    Wire.beginTransmission(_addr);
    return Wire.endTransmission() == 0; // true if chip acks on the bus
  }

  TouchPoint read() {
    TouchPoint p{false, 0, 0};
    uint8_t buf[5];
    if (!readRegs(0x02, buf, 5)) return p;

    uint8_t touchCount = buf[0] & 0x0F;
    if (touchCount == 0) return p;

    uint16_t x = ((buf[1] & 0x0F) << 8) | buf[2];
    uint16_t y = ((buf[3] & 0x0F) << 8) | buf[4];

    p.touched = true;
    p.x = x;
    p.y = y;
    return p;
  }

private:
  uint8_t _addr = FT6336_I2C_ADDR;

  bool readRegs(uint8_t startReg, uint8_t* buf, uint8_t len) {
    Wire.beginTransmission(_addr);
    Wire.write(startReg);
    if (Wire.endTransmission(false) != 0) return false;
    uint8_t got = Wire.requestFrom((int)_addr, (int)len);
    if (got != len) return false;
    for (uint8_t i = 0; i < len; i++) buf[i] = Wire.read();
    return true;
  }
};
