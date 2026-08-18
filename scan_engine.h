/*
  ============================================================================
  scan_engine.h — GM65 barcode module reader (UART, non-blocking)
  ----------------------------------------------------------------------------
  Most GM65 modules ship in "continuous scan" mode: point at a barcode and it
  auto-decodes and streams the result out its TX pin as ASCII + CR/LF, no
  trigger pulse required. This reader buffers incoming bytes and returns a
  complete scan string once a line terminator arrives.

  If your GM65 is configured for trigger mode instead, pulse GM65_TRIGGER_PIN
  low briefly to fire a scan (see triggerScan() below) — wire it to the
  module's trigger input if you want a physical scan button.
  ============================================================================
*/
#pragma once
#include <HardwareSerial.h>
#include "pins_config.h"

static HardwareSerial GM65Serial(2); // use UART2

class ScanEngine {
public:
  void begin() {
    GM65Serial.begin(GM65_BAUD, SERIAL_8N1, GM65_RX_PIN, GM65_TX_PIN);
#if GM65_TRIGGER_PIN >= 0
    pinMode(GM65_TRIGGER_PIN, OUTPUT);
    digitalWrite(GM65_TRIGGER_PIN, HIGH); // idle high, active-low trigger on most GM65 modules
#endif
  }

  void triggerScan() {
#if GM65_TRIGGER_PIN >= 0
    digitalWrite(GM65_TRIGGER_PIN, LOW);
    delay(60);
    digitalWrite(GM65_TRIGGER_PIN, HIGH);
#endif
  }

  // Call every loop(). Returns true exactly once per completed scan,
  // with the decoded string placed in outResult.
  bool poll(String &outResult) {
    while (GM65Serial.available()) {
      char c = (char)GM65Serial.read();
      if (c == '\r' || c == '\n') {
        if (_buf.length() > 0) {
          outResult = _buf;
          _buf = "";
          return true;
        }
      } else {
        _buf += c;
        if (_buf.length() > 128) _buf = ""; // safety: drop runaway garbage
      }
    }
    return false;
  }

private:
  String _buf = "";
};
