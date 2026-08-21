/*
  ============================================================================
  smart_barcode_scanner.ino
  ----------------------------------------------------------------------------
  Test firmware for: Waveshare ESP32-S3-Touch-LCD-3.5 + GM65 barcode module
  Framework: Arduino IDE

  What this does:
    - Boots the 3.5" ST7796 LCD + FT6336 touch via LVGL (two screens: Home, Scan)
    - Reads barcode scans from a GM65 module over UART
    - Connects to WiFi and publishes each scan to an MQTT broker on
      device/{DEVICE_ID}/scan — matching the server architecture already
      designed (Postgres scans table, dedup by uuid)
    - Shows WiFi status and live scan results on screen

  BEFORE YOU BUILD — 3 things to edit:
    1. pins_config.h  — fill in the "VERIFY" pin numbers from your board's
                         official demo package (instructions are in that file)
    2. network.h      — set WIFI_SSID / WIFI_PASSWORD / MQTT_HOST / DEVICE_ID
    3. Arduino IDE board settings:
         Board: "ESP32S3 Dev Module"
         USB CDC On Boot: "Enabled"   (needed for Serial prints over the
                                        Type-C port on this board)
         Partition Scheme: "16M Flash (3MB APP/9.9MB FATFS)"
         PSRAM: "OPI PSRAM"
         Flash Size: 16MB

  Required libraries (Arduino Library Manager, Install Online):
    - GFX_Library_for_Arduino   (moononournation)   v1.5.x
    - lvgl                                            v8.4.x  (NOT v9.x)
    - PubSubClient               (Nick O'Leary)
  ============================================================================
*/
#include <Arduino.h>
#include "pins_config.h"
#include "display_touch.h"
#include "ble_conn.h"
#include "ui_screens.h"
#include "scan_engine.h"
#include "audio.h"
#include "network.h"
#include <XPowersLib.h>

XPowersAXP2101 PMU;    // AXP2101 PMIC — battery gauge
static bool pmuOk = false;
volatile bool is_ptt_pressed = false;

void devicePowerOff() {
  Serial.println("[power] Shutting down device via AXP2101 PMIC...");
  if (pmuOk) {
    PMU.shutdown();
  } else {
    esp_deep_sleep_start();
  }
}

ScanEngine scanner;

uint32_t lastWifiStatusUpdate = 0;
uint32_t lastBatteryUpdate = 0;

void updateBatteryStatus() {
  if (!pmuOk) {
    // AXP2101 not found – fall back to a static placeholder
    uiSetBatteryLevel(100, false);
    return;
  }

  int percent = PMU.getBatteryPercent();   // 0-100, -1 if no battery
  bool charging = PMU.isCharging();        // true while VBUS charges battery
  bool hasBatt  = PMU.isBatteryConnect();

  Serial.printf("[bat] AXP2101: %d%%, charging=%d, connected=%d\n",
                percent, charging, hasBatt);

  if (!hasBatt || percent < 0) {
    // No physical battery – just show plugged-in icon
    uiSetBatteryLevel(100, true);
    return;
  }

  if (percent < 0)   percent = 0;
  if (percent > 100) percent = 100;

  uiSetBatteryLevel((uint8_t)percent, charging);
}

void mqttCallback(char* topic, byte* payload, unsigned int length) {
  Serial.printf("[mqtt] Message arrived on topic: %s\n", topic);
  if (String(topic).endsWith("/tasks")) {
    // Payload is a JSON array of tasks
    StaticJsonDocument<4096> doc;
    DeserializationError error = deserializeJson(doc, payload, length);
    if (error) {
      Serial.printf("[mqtt] Failed to parse tasks JSON: %s\n", error.c_str());
      return;
    }
    
    current_task_count = 0;
    JsonArray arr = doc.as<JsonArray>();
    for (JsonObject t : arr) {
      if (current_task_count >= MAX_TASKS) break;
      
      strncpy(current_tasks[current_task_count].id, t["id"] | "", 31);
      current_tasks[current_task_count].id[31] = '\0';
      
      strncpy(current_tasks[current_task_count].name, t["name"] | "Unknown Task", 31);
      current_tasks[current_task_count].name[31] = '\0';
      
      String prio = t["prio"] | "Low";
      strncpy(current_tasks[current_task_count].prio, prio.c_str(), 15);
      current_tasks[current_task_count].prio[15] = '\0';
      
      String statusStr = t["status"] | "active";
      strncpy(current_tasks[current_task_count].status, statusStr.c_str(), 15);
      current_tasks[current_task_count].status[15] = '\0';
      
      if (prio == "High") current_tasks[current_task_count].prio_color = COLOR_DANGER;
      else if (prio == "Med" || prio == "Medium") current_tasks[current_task_count].prio_color = COLOR_SAFFRON;
      else current_tasks[current_task_count].prio_color = COLOR_GREEN;
      
      // Parse items
      current_tasks[current_task_count].item_count = 0;
      if (t["items"].is<JsonArray>()) {
        JsonArray itemsArr = t["items"].as<JsonArray>();
        for (JsonObject itm : itemsArr) {
          if (current_tasks[current_task_count].item_count >= MAX_ITEMS_PER_TASK) break;
          int idx = current_tasks[current_task_count].item_count;
          strncpy(current_tasks[current_task_count].items[idx].sku, itm["sku"] | "", 31);
          current_tasks[current_task_count].items[idx].sku[31] = '\0';
          strncpy(current_tasks[current_task_count].items[idx].name, itm["name"] | "Unknown", 31);
          current_tasks[current_task_count].items[idx].name[31] = '\0';
          current_tasks[current_task_count].items[idx].target_qty = itm["target_qty"] | 1;
          current_tasks[current_task_count].items[idx].picked_qty = itm["picked_qty"] | 0;
          current_tasks[current_task_count].item_count++;
        }
      }
      
      current_task_count++;
    }
    
    // Update the UI if we are on the tasks screen
    Serial.printf("[mqtt] Updated %d tasks\n", current_task_count);
    update_tasks_ui();
  } else if (String(topic) == "config/users") {
    StaticJsonDocument<2048> doc;
    DeserializationError error = deserializeJson(doc, payload, length);
    if (error) {
      Serial.printf("[mqtt] Failed to parse users JSON: %s\n", error.c_str());
      return;
    }
    
    global_user_count = 0;
    JsonArray arr = doc.as<JsonArray>();
    for (JsonObject u : arr) {
      if (global_user_count >= MAX_USERS) break;
      
      strncpy(global_users[global_user_count].id, u["id"] | "", 31);
      global_users[global_user_count].id[31] = '\0';
      
      strncpy(global_users[global_user_count].name, u["name"] | "", 31);
      global_users[global_user_count].name[31] = '\0';
      
      strncpy(global_users[global_user_count].role, u["role"] | "", 31);
      global_users[global_user_count].role[31] = '\0';
      
      strncpy(global_users[global_user_count].pin, u["pin"] | "1234", 7);
      global_users[global_user_count].pin[7] = '\0';
      
      global_user_count++;
    }
    Serial.printf("[mqtt] Updated %d users\n", global_user_count);
  } else if (String(topic) == "config/inventory") {
    StaticJsonDocument<4096> doc; // Inventory might be larger
    DeserializationError error = deserializeJson(doc, payload, length);
    if (error) {
      Serial.printf("[mqtt] Failed to parse inventory JSON: %s\n", error.c_str());
      return;
    }
    
    global_inventory_count = 0;
    JsonArray arr = doc.as<JsonArray>();
    for (JsonObject item : arr) {
      if (global_inventory_count >= MAX_INVENTORY) break;
      
      strncpy(global_inventory[global_inventory_count].sku, item["sku"] | "", 31);
      global_inventory[global_inventory_count].sku[31] = '\0';
      
      strncpy(global_inventory[global_inventory_count].name, item["name"] | "Unknown", 31);
      global_inventory[global_inventory_count].name[31] = '\0';
      
      String qtyStr = String(item["qty"] | 0);
      strncpy(global_inventory[global_inventory_count].qty, qtyStr.c_str(), 15);
      global_inventory[global_inventory_count].qty[15] = '\0';
      
      global_inventory_count++;
    }
    Serial.printf("[mqtt] Updated %d inventory items\n", global_inventory_count);
    update_inventory_ui();
  }
}


#include <freertos/ringbuf.h>

RingbufHandle_t audio_rx_ringbuf = NULL;

// Dedicated FreeRTOS task for handling duplex audio (Playback + Recording)
void audioTask(void *pvParameters) {
  extern volatile bool is_ptt_pressed;
  extern WebSocketsClient audioWs;
  
  const size_t CHUNK_SIZE = 2048;
  static uint8_t audio_buf[CHUNK_SIZE]; // Microphone recording buffer
  static uint8_t mono_buf[CHUNK_SIZE / 2];
  
  while (true) {
    // 1. Process WebSocket network loop (triggers audioWsEvent on incoming binary audio)
    audioWs.loop();
    
    // 2. Play incoming audio from RingBuffer to ES8311 Speaker
    if (audio_rx_ringbuf) {
      size_t item_size = 0;
      uint8_t *rx_data = (uint8_t *)xRingbufferReceive(audio_rx_ringbuf, &item_size, 0);
      if (rx_data && item_size > 0) {
        size_t frames = item_size / 2;
        int16_t *mono16 = (int16_t *)rx_data;
        int16_t *stereo16 = (int16_t *)malloc(frames * 4);
        if (stereo16) {
          for (size_t i = 0; i < frames; i++) {
            stereo16[i * 2]     = mono16[i]; // Left
            stereo16[i * 2 + 1] = mono16[i]; // Right
          }
          audioPlayChunk((const uint8_t *)stereo16, frames * 4);
          free(stereo16);
        }
        vRingbufferReturnItem(audio_rx_ringbuf, (void *)rx_data);
      }
    }
    
    // 3. Record microphone audio and send over WebSocket when PTT button is held
    if (is_ptt_pressed && WiFi.status() == WL_CONNECTED) {
      size_t bytes_read = audioRecordChunk(audio_buf, CHUNK_SIZE);
      if (bytes_read > 0) {
        size_t frames = bytes_read / 4; // 16-bit stereo = 4 bytes per frame
        int16_t* in16 = (int16_t*)audio_buf;
        int16_t* out16 = (int16_t*)mono_buf;
        for (size_t i = 0; i < frames; i++) {
          int16_t l = in16[i * 2];
          int16_t r = in16[i * 2 + 1];
          out16[i] = (abs(l) >= abs(r)) ? l : r;
        }
        audioWs.sendBIN(mono_buf, frames * 2);
        vTaskDelay(1 / portTICK_PERIOD_MS);
      } else {
        vTaskDelay(2 / portTICK_PERIOD_MS);
      }
    } else {
      vTaskDelay(2 / portTICK_PERIOD_MS); // Yield to keep UI smooth
    }
  }
}

void setup() {
  Serial.begin(115200);
  delay(300);
  Serial.println("\n[boot] Smart Barcode Scanner - test firmware starting");

  // ── 0. Initialize Wire & AXP2101 PMIC FIRST so display 3.3V power rails are ON ──
  Wire.begin(I2C_SDA, I2C_SCL);
  Wire.setClock(400000);

  pmuOk = PMU.init(Wire, I2C_SDA, I2C_SCL, AXP2101_SLAVE_ADDRESS);
  if (pmuOk) {
    PMU.enableGauge();          // turn on the fuel-gauge coulomb counter
    PMU.setDC1Voltage(3300);   PMU.enableDC1();   // CRITICAL: 3.3V peripheral power rail!
    PMU.setALDO1Voltage(3300); PMU.enableALDO1();
    PMU.setALDO2Voltage(3300); PMU.enableALDO2();
    PMU.setALDO3Voltage(3300); PMU.enableALDO3();
    PMU.setALDO4Voltage(3300); PMU.enableALDO4();
    PMU.setDLDO1Voltage(3300); PMU.enableDLDO1();
    PMU.setDLDO2Voltage(3300); PMU.enableDLDO2();
    Serial.println("[boot] AXP2101 PMIC initialized — DC1 & ALDO/DLDO 3.3V power rails active");
    delay(100); // Allow power rails to stabilize
  } else {
    Serial.println("[boot] WARNING: AXP2101 PMIC not found");
  }

  // ── 1. Initialize Display & Touch Subsystem ──────────────────────────────────
  if (!displayTouchInit()) {
    Serial.println("[boot] Display init failed - halting. Check pins_config.h");
    while (1) delay(1000);
  }
  // Load saved display rotation & Wi-Fi credentials from NVS
  loadScreenRotation();
  loadWifiCredentials();

  uiInit();
  // Seed default offline user (ID 123, PIN 123)
  strncpy(global_users[0].id, "123", 31);
  strncpy(global_users[0].name, "Operator 123", 31);
  strncpy(global_users[0].role, "Operator", 31);
  strncpy(global_users[0].pin, "123", 7);
  global_user_count = 1;

  updateBatteryStatus(); // Show real battery % immediately at boot
  Serial.println("[boot] Display + LVGL ready");

  scanner.begin();
  Serial.println("[boot] GM65 scanner UART ready");

  // Create RingBuffer for non-blocking incoming audio playback
  audio_rx_ringbuf = xRingbufferCreate(16384, RINGBUF_TYPE_BYTEBUF);

  // ── Audio codec (Wire already started by displayTouchInit) ────────────────
  if (!audioInit()) {
    Serial.println("[boot] WARNING: ES8311 audio init failed — beep disabled");
  }

  // ── BLE server ─────────────────────────────────────────────────────────────
  bleInit();

  mqtt.setBufferSize(2048);
  mqtt.setCallback(mqttCallback);

  // Start Wi-Fi asynchronously in background (non-blocking)
  wifiStartAsync();
  uiSetWifiStatus("WiFi: connecting...");
  updateBatteryStatus();

  // Initialize Audio WebSocket once safely with device_id (it auto-reconnects)
  extern WebSocketsClient audioWs;
  extern void audioWsEvent(WStype_t type, uint8_t * payload, size_t length);
  String audioPath = String("/audio?client_type=esp32&device_id=") + DEVICE_ID;
  audioWs.begin("192.168.0.112", 3030, audioPath.c_str());
  audioWs.onEvent(audioWsEvent);
  audioWs.setReconnectInterval(5000);

  // Start the dedicated Audio Task to prevent DMA buffer overflows!
  xTaskCreatePinnedToCore(
    audioTask,       // Task function
    "AudioTask",     // Name
    16384,           // Stack size (16KB)
    NULL,            // Parameters
    2,               // Priority (Higher than loop!)
    NULL,            // Task handle
    1                // Core 1 (App Core)
  );

  beepStartup();   // ascending triple chirp — system fully ready
  Serial.println("[boot] Setup complete - point GM65 at a barcode to test");
}

void loop() {
  // LVGL tick & timer handling
  uint32_t now = millis();
#if defined(LV_TICK_CUSTOM) && !LV_TICK_CUSTOM
  static uint32_t lastTick = millis();
  lv_tick_inc(now - lastTick);
  lastTick = now;
#endif
  lv_timer_handler();

  // Keep MQTT connection alive
  mqttLoop();

  vTaskDelay(2 / portTICK_PERIOD_MS); // Yield to FreeRTOS scheduler to prevent UI touch lag

  // Refresh status labels roughly once a second
  if (now - lastWifiStatusUpdate > 1000) {
    lastWifiStatusUpdate = now;
    if (bleIsConnected()) {
      uiSetWifiStatus(LV_SYMBOL_BLUETOOTH " BLE: Connected");
    } else if (bleIsAdvertising()) {
      uiSetWifiStatus(LV_SYMBOL_BLUETOOTH " BLE: Adv...");
    } else if (WiFi.status() == WL_CONNECTED) {
      uiSetWifiStatus(mqtt.connected() ? LV_SYMBOL_WIFI " WiFi: OK" : LV_SYMBOL_WIFI " WiFi: Connecting...");
    } else {
      uiSetWifiStatus(LV_SYMBOL_WIFI " WiFi: Off");
    }
    uiUpdateConnScreen();
  }

  // Update battery level indicator every 5 seconds
  if (now - lastBatteryUpdate > 5000) {
    lastBatteryUpdate = now;
    updateBatteryStatus();
  }

  // Poll the GM65 for a completed scan
  String sku;
  if (scanner.poll(sku)) {
    Serial.printf("[scan] decoded: %s\n", sku.c_str());

    if (!is_logged_in) {
      // ── Login screen active: treat scan as a QR badge login attempt ──────────
      // QR format: "USER:<username>:<pin>"  e.g.  "USER:bhargav:1234"
      uiLoginViaScan(sku);
    } else {
      if (active_task != NULL) {
        // We are in Task Picking Mode
        bool match_found = false;
        bool all_done = true;
        
        for (int i = 0; i < active_task->item_count; i++) {
          if (String(active_task->items[i].sku) == sku) {
            match_found = true;
            if (active_task->items[i].picked_qty < active_task->items[i].target_qty) {
              active_task->items[i].picked_qty++;
              beepSuccess();
              update_task_detail_ui();
            } else {
              // Already fully picked this item
              beepError();
            }
          }
          if (active_task->items[i].picked_qty < active_task->items[i].target_qty) {
            all_done = false;
          }
        }
        
        if (!match_found) {
          beepError(); // Scanned item not in task
        } else if (all_done) {
          // Task completed!
          beepStartup(); // Special sound
          
          if (mqtt.connected()) {
            StaticJsonDocument<128> doc;
            doc["task_id"] = active_task->id;
            char payload[128];
            serializeJson(doc, payload);
            mqtt.publish(("device/" + String(DEVICE_ID) + "/task_complete").c_str(), payload);
          }
          
          // Return to task list
          active_task = NULL;
          lv_scr_load_anim(scr_tasks, LV_SCR_LOAD_ANIM_FADE_ON, 150, 0, false);
        }
      } else {
        // ── Normal product scan flow ─────────────────────────────────────────────
        beepSuccess();          // instant audio feedback
        bleNotifyScan(sku);     // notify via BLE if connected
        uiShowScanResult(sku);
        if (mqtt.connected()) {
          publishScan(sku);
        } else {
          Serial.println("[scan] MQTT not connected - scan shown locally only");
        }
      }
    }
  }

  delay(2);
}
