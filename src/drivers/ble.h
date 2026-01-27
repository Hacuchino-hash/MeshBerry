/**
 * MeshBerry BLE Driver
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2026 NodakMesh (nodakmesh.org)
 *
 * Bluetooth Low Energy driver for MeshCore companion app connectivity.
 * Uses Nordic UART Service (NUS) for serial-over-BLE communication.
 */

#ifndef MESHBERRY_BLE_H
#define MESHBERRY_BLE_H

#include <Arduino.h>
#include <helpers/esp32/SerialBLEInterface.h>

namespace BLE {

/**
 * Initialize BLE subsystem
 * @param deviceName BLE device name (shown during scanning)
 * @param pinCode 6-digit PIN for pairing authentication
 */
void init(const char* deviceName, uint32_t pinCode);

/**
 * Enable BLE advertising and allow connections
 */
void enable();

/**
 * Disable BLE and disconnect any active connection
 */
void disable();

/**
 * Check if BLE is currently enabled
 */
bool isEnabled();

/**
 * Check if a companion device is connected
 */
bool isConnected();

/**
 * Get the underlying serial interface for mesh integration
 * Used by MeshBerryMesh::startCompanionInterface()
 */
SerialBLEInterface& getInterface();

/**
 * Process BLE events - call periodically in main loop
 * Handles connection state changes and queued data
 */
void loop();

} // namespace BLE

#endif // MESHBERRY_BLE_H
