#pragma once
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Initialise the NimBLE host stack.
 *        Call once at startup BEFORE bleStartAdvertising().
 */
void ble_scanner_init(void);

/**
 * @brief Start BLE advertising with the device name "ScanPro-X1".
 *        After calling this the device is visible to mobile phones.
 */
void bleStartAdvertising(void);

/**
 * @brief Stop BLE advertising (device becomes invisible).
 */
void bleStopAdvertising(void);

/**
 * @brief Returns true if BLE advertising is currently active.
 */
bool bleIsAdvertising(void);

/**
 * @brief Returns the dynamically generated BLE device name (includes last 4 MAC digits).
 */
const char* bleGetDeviceName(void);

#ifdef __cplusplus
}
#endif
