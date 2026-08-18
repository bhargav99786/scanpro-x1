/*
  ============================================================================
  display_touch.h — LCD (Arduino_GFX / ST7796) + touch (FT6336) + LVGL v8 glue
  ----------------------------------------------------------------------------
  Requires (Arduino Library Manager, "Install Online"):
    - GFX_Library_for_Arduino  (by moononournation)   v1.5.x
    - lvgl                                             v8.4.x
  ============================================================================
*/
#pragma once
#include <Arduino_GFX_Library.h>
#include <lvgl.h>
#include "pins_config.h"
#include "tca9554.h"
#include "ft6336_touch.h"
#include "logo.h"

static TCA9554 ioExpander;
static FT6336  touch;

// LCD_CS and LCD_RST live on the IO-expander. Since the LCD is the only
// device on this SPI bus, we assert CS permanently (active-low) once at
// boot via the expander, then let Arduino_GFX drive the bus normally with
// its own CS argument disabled (GFX_NOT_DEFINED).
static Arduino_DataBus *bus = new Arduino_ESP32SPI(
    LCD_DC /* DC */, GFX_NOT_DEFINED /* CS - handled by expander */,
    LCD_SPI_SCLK /* SCK */, LCD_SPI_MOSI /* MOSI */, LCD_SPI_MISO /* MISO */);

static Arduino_GFX *gfx = new Arduino_ST7796(
    bus, GFX_NOT_DEFINED /* RST - handled by expander */,
    1 /* rotation: 1 = landscape 480x320, use 0 for portrait 320x480 */,
    true /* IPS */, LCD_HOR_RES, LCD_VER_RES);

// ---- LVGL draw buffer ----
#define LVGL_BUF_LINES 40
// Buffer width must match the wider dimension (480) for landscape
static lv_disp_draw_buf_t drawBuf;
static lv_color_t lvBuf1[LCD_VER_RES * LVGL_BUF_LINES];
static lv_disp_drv_t dispDrv;
static lv_indev_drv_t indevDrv;

static void lvgl_flush_cb(lv_disp_drv_t *disp, const lv_area_t *area, lv_color_t *color_p) {
  uint32_t w = area->x2 - area->x1 + 1;
  uint32_t h = area->y2 - area->y1 + 1;
  gfx->draw16bitRGBBitmap(area->x1, area->y1, (uint16_t *)&color_p->full, w, h);
  lv_disp_flush_ready(disp);
}

#include <Preferences.h>

static uint8_t current_rotation = 1; // 1 = Landscape (90°), 3 = Landscape Flipped (270°), 0 = Portrait (0°), 2 = Portrait Flipped (180°)

static void lvgl_touch_cb(lv_indev_drv_t *indev, lv_indev_data_t *data) {
  TouchPoint p = touch.read();
  if (p.touched) {
    data->state = LV_INDEV_STATE_PR;
    if (current_rotation == 1) {        // 90° Landscape
      data->point.x = p.y;
      data->point.y = LCD_HOR_RES - 1 - p.x;
    } else if (current_rotation == 3) { // 270° Flipped Landscape
      data->point.x = LCD_VER_RES - 1 - p.y;
      data->point.y = p.x;
    } else if (current_rotation == 0) { // 0° Portrait
      data->point.x = p.x;
      data->point.y = p.y;
    } else if (current_rotation == 2) { // 180° Flipped Portrait
      data->point.x = LCD_HOR_RES - 1 - p.x;
      data->point.y = LCD_VER_RES - 1 - p.y;
    }
  } else {
    data->state = LV_INDEV_STATE_REL;
  }
}

inline void setScreenRotation(uint8_t rot) {
  current_rotation = rot % 4;
  gfx->setRotation(current_rotation);

  if (current_rotation == 1 || current_rotation == 3) {
    dispDrv.hor_res = LCD_VER_RES; // 480
    dispDrv.ver_res = LCD_HOR_RES; // 320
  } else {
    dispDrv.hor_res = LCD_HOR_RES; // 320
    dispDrv.ver_res = LCD_VER_RES; // 480
  }
  lv_disp_t *disp = lv_disp_get_default();
  if (disp) {
    lv_disp_drv_update(disp, &dispDrv);
  }
  lv_obj_invalidate(lv_scr_act());
}

inline void loadScreenRotation() {
  Preferences prefs;
  prefs.begin("display", true);
  uint8_t r = prefs.getUChar("rot", 1);
  prefs.end();
  setScreenRotation(r);
}

inline void saveScreenRotation(uint8_t rot) {
  setScreenRotation(rot);
  Preferences prefs;
  prefs.begin("display", false);
  prefs.putUChar("rot", current_rotation);
  prefs.end();
  Serial.printf("[display] Saved rotation: %d\n", current_rotation);
}

inline bool displayTouchInit() {
  Wire.begin(I2C_SDA, I2C_SCL);
  Wire.setClock(400000); // 400kHz Fast I2C bus speed for smooth touch & IO responses

  // Backlight is on direct GPIO 6
  pinMode(GFX_BL, OUTPUT);
  digitalWrite(GFX_BL, HIGH);

  bool expanderOK = ioExpander.begin();
  if (expanderOK) {
    // Reset pulse then release (LCD RST is on expander P1)
    ioExpander.setLcdRST(false);
    delay(20);
    ioExpander.setLcdRST(true);
    delay(120);
  } else {
    Serial.println("[display] WARNING: TCA9554 expander not found — "
                    "check I2C_SDA/I2C_SCL and TCA9554_I2C_ADDR in pins_config.h");
  }

  if (!gfx->begin()) {
    Serial.println("[display] gfx->begin() failed — check SPI pins in pins_config.h");
    return false;
  }
  // ---- Splash screen: white background + centered logo (800ms fast boot) ----
  gfx->fillScreen(WHITE);
  // Center the logo on the 480x320 landscape display
  int16_t logo_x = (LCD_VER_RES - LOGO_W) / 2;
  int16_t logo_y = (LCD_HOR_RES - LOGO_H) / 2;
  gfx->draw16bitRGBBitmap(logo_x, logo_y, (uint16_t *)logo_pixels, LOGO_W, LOGO_H);
  delay(800);

  bool touchOK = touch.begin();
  if (!touchOK) {
    Serial.println("[display] WARNING: FT6336 touch chip not found on I2C — "
                    "check FT6336_I2C_ADDR / wiring.");
  }

  // ---- LVGL init ----
  lv_init();
  lv_disp_draw_buf_init(&drawBuf, lvBuf1, NULL, LCD_VER_RES * LVGL_BUF_LINES);

  lv_disp_drv_init(&dispDrv);
  // Swap native width/height because GFX is set to rotation 1 (Landscape)
  dispDrv.hor_res = LCD_VER_RES;
  dispDrv.ver_res = LCD_HOR_RES;
  dispDrv.flush_cb = lvgl_flush_cb;
  dispDrv.draw_buf = &drawBuf;
  lv_disp_drv_register(&dispDrv);

  lv_indev_drv_init(&indevDrv);
  indevDrv.type = LV_INDEV_TYPE_POINTER;
  indevDrv.read_cb = lvgl_touch_cb;
  lv_indev_drv_register(&indevDrv);

  return true;
}
