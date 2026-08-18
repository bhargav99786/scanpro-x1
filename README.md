# ScanPro X1 - Smart Barcode Scanner System

ScanPro X1 is a comprehensive, real-time warehouse barcode scanning and management system built around the ESP32-S3 microcontroller. It features a modern, responsive LVGL-based UI on the handheld scanner, synchronized seamlessly via MQTT to a robust Node.js web dashboard.

## 🌟 Key Features

* **Real-Time Synchronization:** Instant updates for inventory, user roles, active tasks, and scanned items between the dashboard and all active hardware scanners using WebSockets and MQTT.
* **Smart User Management:** Devices strictly validate user logins against centralized user profiles broadcasted from the server.
* **Offline Resilience:** The firmware caches user credentials and inventory data (up to 50 items) locally, ensuring operations can continue smoothly in intermittent network conditions.
* **Task Assignment:** Managers can push priority-based picking and packing tasks directly to specific devices via the dashboard.
* **Live Activity Tracking:** See exactly who is logged into which scanner, and watch incoming scans in real-time.
* **Dynamic Inventory:** Easily add, modify, or delete inventory items from the dashboard, with changes instantly reflecting on all connected scanners.

## 🏗️ Architecture Stack

### Firmware (ESP32-S3)
* **Core:** C++ / Arduino Framework (FreeRTOS enabled)
* **UI Graphics:** LVGL (Light and Versatile Graphics Library)
* **Connectivity:** WiFi, MQTT (PubSubClient), WebSockets
* **Data Parsing:** ArduinoJson

### Server & Dashboard
* **Backend:** Node.js, Express.js
* **Messaging:** Local MQTT Broker (Mosquitto), `mqtt.js`
* **Real-Time Data:** `ws` (WebSockets)
* **Frontend:** Vanilla HTML/CSS/JS, Custom Modern UI Design

## 🚀 Getting Started

### 1. Server Setup
1. Ensure you have Node.js and an MQTT broker (like Mosquitto) installed.
2. Navigate to the `server/` directory.
3. Install dependencies:
   ```bash
   npm install
   ```
4. Start the server:
   ```bash
   node server.js
   ```
5. Open your browser and navigate to `http://localhost:3030` to access the Command Center Dashboard.

### 2. Firmware Flashing
1. Open the project in the Arduino IDE.
2. Update the `MQTT_HOST` and `WIFI_SSID`/`WIFI_PASS` variables in `network.h` to match your local network configuration.
3. Select your ESP32-S3 board and upload the code.
4. The scanner will boot up, connect to WiFi, synchronize users/inventory, and await login.

## 🤝 Contributing
Contributions, issues, and feature requests are welcome!

---
*Built for modern warehouse logistics.*
