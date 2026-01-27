/**
 * MeshBerry BLE Driver Implementation
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2026 NodakMesh (nodakmesh.org)
 */

#include "ble.h"

namespace BLE {

// Global BLE interface instance (from MeshCore library)
static SerialBLEInterface bleInterface;
static bool initialized = false;
static bool enabled = false;

void init(const char* deviceName, uint32_t pinCode) {
    if (initialized) return;

    // Initialize with device name and PIN code
    // SerialBLEInterface::begin() sets up:
    // - BLE device with given name
    // - Nordic UART Service (NUS) for serial communication
    // - Security with PIN code authentication
    bleInterface.begin(deviceName, pinCode);

    initialized = true;
    Serial.printf("BLE: Initialized as '%s' with PIN %06lu\n", deviceName, pinCode);
}

void enable() {
    if (!initialized) return;
    if (enabled) return;

    bleInterface.enable();
    enabled = true;
    Serial.println("BLE: Advertising enabled");
}

void disable() {
    if (!initialized) return;
    if (!enabled) return;

    bleInterface.disable();
    enabled = false;
    Serial.println("BLE: Disabled");
}

bool isEnabled() {
    return initialized && bleInterface.isEnabled();
}

bool isConnected() {
    return initialized && bleInterface.isConnected();
}

SerialBLEInterface& getInterface() {
    return bleInterface;
}

void loop() {
    // SerialBLEInterface handles its own event loop internally
    // through BLE callbacks, but we can add state monitoring here
    // if needed for UI updates

    static bool wasConnected = false;
    bool nowConnected = isConnected();

    if (nowConnected != wasConnected) {
        if (nowConnected) {
            Serial.println("BLE: Companion connected");
        } else {
            Serial.println("BLE: Companion disconnected");
        }
        wasConnected = nowConnected;
    }
}

} // namespace BLE
