/*
  ble_conn.h — BLE server for ScanPro-X1
  Advertises as "ScanPro-X1". Connected phone apps receive scan
  notifications on BLE_CHAR_SCAN_UUID.

  API:
    bleInit()              — call once in setup()
    bleStartAdvertising()  — start BLE advertising
    bleStopAdvertising()   — stop advertising
    bleIsConnected()       — returns true when a client is connected
    bleIsAdvertising()     — returns true while advertising
    bleNotifyScan(sku)     — push scan result to connected client
*/
#pragma once
#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLE2902.h>

#define BLE_DEVICE_NAME    "ScanPro-X1"
#define BLE_SERVICE_UUID   "4fafc201-1fb5-459e-8fcc-c5c9c331914b"
#define BLE_SCAN_CHAR_UUID "beb5483e-36e1-4688-b7f5-ea07361b26a8"

static BLEServer*         _bleServer   = nullptr;
static BLECharacteristic* _bleScanChar = nullptr;
static bool               _bleConn     = false;
static bool               _bleAdv      = false;

class _BleCb : public BLEServerCallbacks {
    void onConnect(BLEServer*)    override { _bleConn = true;  }
    void onDisconnect(BLEServer*) override {
        _bleConn = false;
        if (_bleAdv) BLEDevice::startAdvertising(); // auto-restart for next client
    }
};

inline void bleInit() {
    BLEDevice::init(BLE_DEVICE_NAME);
    _bleServer = BLEDevice::createServer();
    _bleServer->setCallbacks(new _BleCb());

    BLEService* svc = _bleServer->createService(BLE_SERVICE_UUID);
    _bleScanChar = svc->createCharacteristic(
        BLE_SCAN_CHAR_UUID,
        BLECharacteristic::PROPERTY_READ | BLECharacteristic::PROPERTY_NOTIFY);
    _bleScanChar->addDescriptor(new BLE2902());
    svc->start();
    Serial.println("[BLE] Initialized as '" BLE_DEVICE_NAME "' (not advertising yet)");
}

inline void bleStartAdvertising() {
    if (_bleAdv) return;
    BLEAdvertising* adv = BLEDevice::getAdvertising();
    adv->addServiceUUID(BLE_SERVICE_UUID);
    adv->setScanResponse(true);
    BLEDevice::startAdvertising();
    _bleAdv = true;
    Serial.println("[BLE] Advertising started");
}

inline void bleStopAdvertising() {
    if (!_bleAdv) return;
    BLEDevice::stopAdvertising();
    _bleAdv = false;
    Serial.println("[BLE] Advertising stopped");
}

inline bool bleIsConnected()   { return _bleConn; }
inline bool bleIsAdvertising() { return _bleAdv;  }

inline void bleNotifyScan(const String& sku) {
    if (!_bleConn || !_bleScanChar) return;
    _bleScanChar->setValue(sku.c_str());
    _bleScanChar->notify();
    Serial.printf("[BLE] Notified: %s\n", sku.c_str());
}
