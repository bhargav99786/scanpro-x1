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

### 📲 Multi-Layer Resilient OTA Updates
- **Zero-Touch Wireless OTA:** Upload new firmware directly from the web dashboard — broadcast to all scanners or targeted devices via MQTT.
- **Deep Server-Side Binary Inspection (Layer 2):** Every uploaded firmware binary undergoes cryptographic and structural integrity analysis before being broadcast:
  - **Magic Byte Check:** Validates ESP image signature (`0xE9` at offset 0).
  - **ESP32 Chip ID Validation:** Checks hardware architecture (ESP32-S3 `0x0009`, ESP32, ESP32-C3, etc.) preventing cross-architecture flashing.
  - **Segment Boundary Traversal:** Parses image header and walks all segments to catch truncated or malformed images.
  - **Appended SHA-256 Verification:** Validates the binary's trailing cryptographic hash against computed payload hash.
  - **Immediate Corrupt File Rejection:** Corrupted, truncated, bitflipped, or invalid architecture binaries are immediately rejected with actionable dashboard alerts.
- **Resumable HTTP Uploads:** OTA upload state is tracked; recovers gracefully from WiFi drops without restarting from 0%.
- **Dual Partition Safe Rollback (Layer 3):** Uses ESP-IDF's native dual-boot scheme (`ota_0`/`ota_1`) with automatic fallback to previous operational partition if the new image fails boot self-tests.
- **Real-Time Version Display:** Current firmware version reported in status bar and synced via MQTT.

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
├── ScanPro_X1_General_Testing_Scenarios.xlsx # Comprehensive system test matrix (38+ scenarios)
├── ScanPro_X1_General_Testing_Scenarios.csv  # CSV export of general test scenarios
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

## 🧪 Test Documentation & Verification Matrix

The repository includes a comprehensive QA testing workbook and CSV export designed for hardware-in-the-loop (HIL) and system validation:

### General System Test Matrix (`ScanPro_X1_General_Testing_Scenarios.xlsx`)
Comprehensive 38-scenario test suite covering all operational aspects of the device and server ecosystem:
- **Core Scanning (GM65 UART + BLE):** 1D/2D symbology decoding, rapid batch scanning, damaged/low-contrast code handling.
- **Real-Time Sync:** Bi-directional MQTT/WebSocket state synchronization under normal and high network loads.
- **Warehouse Task Picking Workflow:** Strict SKU barcode matching, quantity deductions, and error handling for unexpected scans.
- **Two-Way Voice Intercom (PTT):** ES8311 I2S codec initialization, VAD gating, packet streaming, and bi-directional audio clarity.
- **Power Management & AXP2101:** Battery ADC accuracy, charge status telemetry, power off button deep sleep/shutdown.
- **Responsive UI:** Dynamic rotation between portrait (320×480) and landscape (480×320), non-overlapping bottom action buttons.
- **Offline Resilience:** NVS cached login credentials and offline buffer behavior.

---

## 🤝 Contributing
Contributions, issues, and feature requests are welcome! Feel free to open a PR or file an issue.

---

*Built for modern warehouse logistics. Powered by ESP32-S3 + ESP-IDF v5.4.*
