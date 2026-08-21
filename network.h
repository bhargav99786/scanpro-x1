/*
  ============================================================================
  network.h — WiFi connect + MQTT publish (matches the server architecture:
  topic device/{device_id}/scan, JSON payload with uuid for idempotency)
  ----------------------------------------------------------------------------
  Requires (Arduino Library Manager): PubSubClient (by Nick O'Leary)
  ============================================================================
*/
#pragma once
#include <WiFi.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>
#include <WebSocketsClient.h> // For audio streaming

#include "pins_config.h"

// ---- WiFi & MQTT Configuration ----
inline char wifi_ssid[64]     = DEFAULT_WIFI_SSID;
inline char wifi_password[64] = DEFAULT_WIFI_PASS;
static const char *MQTT_HOST     = "192.168.0.112"; // your MQTT broker IP/hostname
static const uint16_t MQTT_PORT  = 1883;             // use 8883 + WiFiClientSecure for TLS in production
static const char *DEVICE_ID     = "scanpro-test-01";
// ------------------------------------------

static WiFiClient wifiClient;
static PubSubClient mqtt(wifiClient);
static WebSocketsClient audioWs;

inline bool publishDeviceStatus(bool loggedIn, const char* userId);

#include <freertos/FreeRTOS.h>
#include <freertos/ringbuf.h>

extern RingbufHandle_t audio_rx_ringbuf;

inline void audioWsEvent(WStype_t type, uint8_t * payload, size_t length) {
  switch(type) {
    case WStype_DISCONNECTED:
      Serial.println("[audio_ws] Disconnected!");
      break;
    case WStype_CONNECTED:
      Serial.println("[audio_ws] Connected to url: /audio");
      break;
    case WStype_BIN: {
      static uint32_t lastLog = 0;
      if (millis() - lastLog > 2000) {
        Serial.printf("[audio_ws] Receiving incoming audio stream (%u B)...\n", (unsigned)length);
        lastLog = millis();
      }
      if (audio_rx_ringbuf && length > 0) {
        // Non-blocking push to FreeRTOS RingBuffer for playback task
        xRingbufferSend(audio_rx_ringbuf, payload, length, 0);
      }
      break;
    }
  }
}

inline void loadWifiCredentials() {
  Preferences prefs;
  prefs.begin("wifi", true); // read-only
  String s = prefs.getString("ssid", "");
  String p = prefs.getString("pass", "");
  prefs.end();

  if (s.length() > 0) {
    strncpy(wifi_ssid, s.c_str(), sizeof(wifi_ssid) - 1);
    strncpy(wifi_password, p.c_str(), sizeof(wifi_password) - 1);
    Serial.printf("[wifi] Loaded credentials from NVS: SSID='%s'\n", wifi_ssid);
  } else {
    Serial.printf("[wifi] NVS empty, keeping active SSID='%s'\n", wifi_ssid);
  }
}

inline void saveWifiCredentials(const char *ssid, const char *pass) {
  strncpy(wifi_ssid, ssid, sizeof(wifi_ssid) - 1);
  strncpy(wifi_password, pass, sizeof(wifi_password) - 1);

  Preferences prefs;
  prefs.begin("wifi", false); // read-write
  prefs.putString("ssid", wifi_ssid);
  prefs.putString("pass", wifi_password);
  prefs.end();
  Serial.printf("[wifi] Saved credentials: SSID='%s'\n", wifi_ssid);
}

inline void wifiStartAsync() {
  if (strlen(wifi_ssid) > 0) {
    WiFi.mode(WIFI_STA);
    WiFi.setAutoReconnect(true);
    WiFi.setTxPower(WIFI_POWER_19_5dBm);
    WiFi.setSleep(false); // CRITICAL: Disable Wi-Fi modem sleep to prevent RF dropouts!
    WiFi.begin(wifi_ssid, wifi_password);
    Serial.printf("[wifi] Async connection started for '%s'\n", wifi_ssid);
  } else {
    Serial.println("[wifi] WARNING: No saved SSID in NVS! Connect via UI or BLE.");
  }
}

inline bool wifiConnect(uint32_t timeoutMs = 2000) {
  if (strlen(wifi_ssid) == 0) {
    Serial.println("[wifi] No SSID configured. Connect via touch screen.");
    return false;
  }

  WiFi.mode(WIFI_STA);
  WiFi.setAutoReconnect(true);
  WiFi.setTxPower(WIFI_POWER_19_5dBm);
  WiFi.setSleep(false);
  WiFi.begin(wifi_ssid, wifi_password);
  uint32_t start = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - start < timeoutMs) {
    delay(50);
    Serial.print(".");
  }
  Serial.println();
  if (WiFi.status() == WL_CONNECTED) {
    Serial.printf("[wifi] connected to '%s', IP=%s\n", wifi_ssid, WiFi.localIP().toString().c_str());
    return true;
  }
  Serial.println("[wifi] connect failed / timed out");
  return false;
}

inline bool mqttConnect() {
  if (WiFi.status() != WL_CONNECTED) return false;
  
  wifiClient.setTimeout(1); // Set 1 sec socket timeout to prevent UI lag during connection attempts
  mqtt.setServer(MQTT_HOST, MQTT_PORT);
  String clientId = String(DEVICE_ID) + "-" + String((uint32_t)ESP.getEfuseMac(), HEX);
  String statusTopic = String("device/") + DEVICE_ID + "/status";
  String willPayload = "{\"status\":\"offline\"}";
  
  if (mqtt.connect(clientId.c_str(), statusTopic.c_str(), 1, true, willPayload.c_str())) {
    Serial.println("[mqtt] connected");
    String taskTopic = String("device/") + DEVICE_ID + "/tasks";
    mqtt.subscribe(taskTopic.c_str());
    mqtt.subscribe("config/users");
    mqtt.subscribe("config/inventory");
    Serial.printf("[mqtt] Subscribed to %s, config/users, config/inventory\n", taskTopic.c_str());
    
    // Publish online presence immediately upon connecting
    extern bool is_logged_in;
    extern char logged_in_user[32];
    publishDeviceStatus(is_logged_in, logged_in_user);
    return true;
  }
  Serial.printf("[mqtt] connect failed, state=%d\n", mqtt.state());
  return false;
}

inline void mqttLoop() {
  if (WiFi.status() == WL_CONNECTED) {
    if (!mqtt.connected()) {
      static uint32_t lastMqttAttempt = 0;
      if (millis() - lastMqttAttempt > 10000) { // Retry at most once every 10 sec to prevent UI lag
        lastMqttAttempt = millis();
        mqttConnect();
      }
    } else {
      mqtt.loop();
    }
    // audioWs.loop() is now handled exclusively by audioTask!
  } else {
    // If Wi-Fi is disconnected, retry connecting periodically
    static uint32_t lastWifiAttempt = 0;
    if (millis() - lastWifiAttempt > 5000) {
      lastWifiAttempt = millis();
      if (strlen(wifi_ssid) > 0) {
        Serial.printf("[wifi] Retrying connection to '%s'...\n", wifi_ssid);
        WiFi.begin(wifi_ssid, wifi_password);
      }
    }
  }
}

// Generates a lightweight pseudo-UUID (good enough for dedup on a single
// device; swap for a proper UUID library if you need cross-device guarantees)
inline String makeScanUUID() {
  static uint32_t counter = 0;
  counter++;
  char buf[40];
  snprintf(buf, sizeof(buf), "%08X-%08X-%04X", (uint32_t)millis(), (uint32_t)ESP.getEfuseMac(), counter & 0xFFFF);
  return String(buf);
}

// Publishes device/{DEVICE_ID}/scan with a JSON payload matching the
// server-side scans table (see architecture doc: uuid, device_id, sku, ts)
inline bool publishScan(const String &sku) {
  if (!mqtt.connected()) return false;
  String uuid = makeScanUUID();
  String topic = String("device/") + DEVICE_ID + "/scan";
  String payload = String("{\"uuid\":\"") + uuid +
                    "\",\"device_id\":\"" + DEVICE_ID +
                    "\",\"sku\":\"" + sku +
                    "\",\"ts\":" + String(millis()) + "}";
  bool ok = mqtt.publish(topic.c_str(), payload.c_str());
  Serial.printf("[mqtt] publish %s -> %s : %s\n", topic.c_str(), payload.c_str(), ok ? "OK" : "FAILED");
  return ok;
}

inline bool publishDeviceStatus(bool loggedIn, const char* userId) {
  if (!mqtt.connected()) return false;
  String topic = String("device/") + DEVICE_ID + "/status";
  
  StaticJsonDocument<128> doc;
  doc["status"] = "online";
  if (loggedIn && userId && strlen(userId) > 0 && strcmp(userId, "Unassigned") != 0 && strcmp(userId, "No Login") != 0) {
    doc["user"] = userId;
  } else {
    doc["user"] = "No Login";
  }
  doc["ts"] = millis();
  
  String payload;
  serializeJson(doc, payload);
  
  bool ok = mqtt.publish(topic.c_str(), payload.c_str(), true); // Retained message
  Serial.printf("[mqtt] publish status -> %s : %s\n", payload.c_str(), ok ? "OK" : "FAILED");
  return ok;
}
