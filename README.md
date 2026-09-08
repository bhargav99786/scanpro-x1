# ScanPro X1 - Smart Barcode Scanner System

ScanPro X1 is a production-grade, real-time warehouse barcode scanning and management system built on the **ESP32-S3** microcontroller. It combines a rich LVGL-based UI on the handheld device with a powerful Node.js web dashboard — synchronized seamlessly over MQTT and WebSockets.

---

## 🌟 Key Features

### 📡 Connectivity & Sync
- **Real-Time Synchronization:** Instant bi-directional updates for inventory, user roles, active tasks, and scanned items between the dashboard and all connected scanners using WebSockets + MQTT.
- **Smart User Management:** Devices strictly validate user logins against centralized user profiles broadcast from the server.
- **Offline Resilience:** Firmware caches credentials and inventory data (up to 50 items) locally — operations continue seamlessly through intermittent connectivity.

### 📦 Warehouse Operations
- **Task Assignment:** Managers push priority-based picking/packing tasks directly to specific devices via the dashboard.
- **Live Activity Tracking:** See who is logged into which scanner and watch incoming scans in real-time.
- **Dynamic Inventory:** Add, modify, or delete inventory items from the dashboard; changes instantly reflect on all connected scanners.
- **Strict Picking Workflow:** Validates scanned barcodes against assigned task items and auto-deducts from inventory on completion.

### 🎙️ Voice Intercom (PTT)
- **Push-To-Talk (PTT):** A floating "TAP TO TALK" button enables live two-way voice communication between the handheld scanner and the server/dashboard over WebSockets.
- **Voice Activity Detection (VAD):** Automatic silence gating prevents unnecessary audio transmission.
- **Audio Codec:** Integrated ES8311 I2S audio codec for high-quality capture and playback.
- **Bidirectional:** Scanner → Server and Server → Scanner audio streaming both supported.

### 🔋 Power Management
- **AXP2101 PMIC Integration:** Battery level reporting, charging status monitoring.
- **Power Off Button:** Dedicated hardware power-off action from Settings and Setup screens — sends graceful shutdown signal via AXP2101.
- **Wrist Detection:** Optional wrist-detect toggle for auto screen-on when raised.

### 📲 OTA Firmware Updates
- **Wireless OTA:** Upload new firmware directly from the web dashboard — no cables needed.
- **Resumable Uploads:** OTA upload state is tracked; the process recovers gracefully from WiFi interruptions without restarting from 0%.
- **Dual OTA Partitions:** Uses ESP-IDF's native OTA partition scheme (`ota_0`/`ota_1`) for safe rollback.
- **Version Display:** Current firmware version shown on device status bar and reported to the server.

### 🖥️ Responsive LVGL UI
- **Portrait & Landscape:** Full responsive layout — all screens automatically reflow when the device is rotated.
- **Portrait-Safe Bottom Bar:** In portrait mode, Logout (`OUT`), Power Off (`OFF`), and PTT (`TALK`) buttons are each 74×42 px side-by-side with zero overlap.
- **Screens:** Home, Inventory, Tasks, Setup/Connections, Settings/Diagnostics, Login.
- **Glassmorphism-inspired design** with saffron PTT accent, danger-red action buttons, and dark navy theme.
- **Status Bar:** Always-visible battery %, WiFi status, firmware version, and active user.

---

## 🏗️ Architecture Stack

### Firmware (ESP32-S3) — ESP-IDF v5.4

| Component | Technology |
|---|---|
| **Core** | ESP-IDF v5.4 (C++ / FreeRTOS) |
| **UI Graphics** | LVGL 8.x via `esp_lvgl_port` |
| **Display** | 3.5" 480×320 LCD (ILI9488 / custom port) |
| **Touch** | FT6336 capacitive touch controller |
| **Connectivity** | WiFi (esp_wifi), MQTT (esp-mqtt), WebSockets |
| **Audio** | ES8311 codec via I2S + `esp_codec_dev` |
| **Power Management** | AXP2101 PMIC via I2C |
| **BLE** | BLE scanner for auxiliary barcode devices |
| **OTA** | ESP-IDF `esp_ota_ops` with HTTP chunked upload |
| **Data** | cJSON / custom parsing |

### Server & Dashboard

| Component | Technology |
|---|---|
| **Backend** | Node.js, Express.js |
| **MQTT Broker** | Mosquitto (local), `mqtt.js` client |
| **Real-Time** | WebSockets (`ws` library) |
| **OTA Endpoint** | Chunked binary upload → streams to device |
| **Frontend** | Vanilla HTML/CSS/JS — custom dark glassmorphism UI |
| **TLS** | Optional HTTPS/WSS on port 3031 |

---

## 📁 Project Structure

```
smart_barcode_scanner/
├── esp_idf_firmware/          # ESP-IDF v5.4 production firmware
│   ├── main/
│   │   ├── main.cpp           # App entry point, FreeRTOS tasks
│   │   ├── ui_screens.h       # All LVGL screens & responsive layout
│   │   ├── network_mqtt.cpp   # WiFi, MQTT, WebSocket, OTA client
│   │   ├── ble_scanner.cpp    # BLE barcode scanner integration
│   │   ├── display_port.cpp   # Display init, rotation, backlight
│   │   ├── gm65_scanner.cpp   # GM65 UART barcode scanner driver
│   │   └── ota_task.cpp       # OTA firmware update task
│   └── components/
│       └── esp_port/          # Board-specific port layer (AXP2101, LCD)
├── server/
│   ├── server.js              # Node.js backend (MQTT + WebSocket + OTA)
│   ├── data.json              # Persistent inventory, users, tasks
│   └── public/
│       ├── index.html         # Dashboard UI
│       ├── app.js             # Dashboard logic (WebSocket, OTA upload)
│       ├── style.css          # Custom dark UI styles
│       ├── firmware.bin       # Latest firmware (served for OTA)
│       └── firmware_ota_update.bin
├── smart_barcode_scanner.ino  # Legacy Arduino sketch (reference only)
├── ui_screens.h               # Legacy Arduino UI (reference only)
└── README.md
```

---

## 🚀 Getting Started

### Prerequisites
- Node.js ≥ 18.x
- Mosquitto MQTT broker (`sudo apt install mosquitto mosquitto-clients`)
- ESP-IDF v5.4.1 (for firmware compilation)

### 1. Server Setup
```bash
cd server/
npm install
node server.js
```
- **Dashboard:** http://localhost:3030
- **Secure (HTTPS/WSS):** https://localhost:3031
- **MQTT Broker:** localhost:1883

### 2. Firmware — Build & Flash (ESP-IDF)
```bash
# Source ESP-IDF environment
source ~/esp/v5.4.1/esp-idf/export.sh

cd esp_idf_firmware/
idf.py build
idf.py -p /dev/ttyACM0 flash
```

### 3. OTA Wireless Update
1. Build the firmware: `idf.py build`
2. Copy `build/smart_barcode_scanner_idf.bin` → `server/public/firmware_ota_update.bin`
3. Open the dashboard at http://localhost:3030
4. Click **OTA Update** and select the `.bin` file — the upload streams directly to the device over WiFi.

### 4. Device Configuration
Update these values in `esp_idf_firmware/main/network_mqtt.cpp` or `network.h`:
```cpp
#define WIFI_SSID     "YourNetwork"
#define WIFI_PASS     "YourPassword"
#define MQTT_HOST     "192.168.x.x"   // IP of your server
#define MQTT_PORT     1883
```

---

## 📱 UI Screens

| Screen | Description |
|---|---|
| **Login** | PIN/password entry with numpad, user validation via MQTT |
| **Home** | Scan arc, scan count, quick-action buttons |
| **Inventory** | Live item list with search, synced from server |
| **Tasks** | Priority task queue assigned by manager |
| **Setup** | WiFi status, BLE, MQTT connection cards + Logout/Power |
| **Settings** | Diagnostics, display orientation, wrist detection + Logout/Power |

---

## 🔘 Portrait Mode Bottom Bar

In portrait mode (`width < 400px`), the bottom action bar has 3 buttons perfectly side-by-side with no overlap:

```
┌─────────────────────────────────┐
│                                 │
│        [ Screen Content ]       │
│                                 │
├─────────────────────────────────┤
│ [⏻ OUT] [⏻ OFF] [🔊 TALK]     │
│  74px    74px    74px (PTT)     │
└─────────────────────────────────┘
```

---

## 🤝 Contributing
Contributions, issues, and feature requests are welcome! Feel free to open a PR or file an issue.

---

*Built for modern warehouse logistics. Powered by ESP32-S3 + ESP-IDF v5.4.*
