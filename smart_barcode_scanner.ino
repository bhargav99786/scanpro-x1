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
#include "network.h"
#include "audio.h"
#include <XPowersLib.h>

XPowersAXP2101 PMU;    // AXP2101 PMIC — battery gauge
static bool pmuOk = false;

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


void setup() {
  Serial.begin(115200);
  delay(300);
  Serial.println("\n[boot] Smart Barcode Scanner - test firmware starting");

  if (!displayTouchInit()) {
    Serial.println("[boot] Display init failed - halting. Check pins_config.h");
    while (1) delay(1000);
  }
  // Load saved display rotation & Wi-Fi credentials from NVS
  loadScreenRotation();
  loadWifiCredentials();

  // ── AXP2101 PMIC init (I2C shared with touch; Wire already started) ─────────
  pmuOk = PMU.init(Wire, I2C_SDA, I2C_SCL, AXP2101_SLAVE_ADDRESS);
  if (pmuOk) {
    PMU.enableGauge();          // turn on the fuel-gauge coulomb counter
    Serial.println("[boot] AXP2101 PMIC found – fuel gauge active");
  } else {
    Serial.println("[boot] AXP2101 not found – battery % will show 100%%");
  }

  uiInit();
  updateBatteryStatus(); // Show real battery % immediately at boot
  Serial.println("[boot] Display + LVGL ready");

  scanner.begin();
  Serial.println("[boot] GM65 scanner UART ready");

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
