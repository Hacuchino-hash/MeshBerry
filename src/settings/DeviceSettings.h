/**
 * MeshBerry Device Settings
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2026 NodakMesh (nodakmesh.org)
 *
 * This file is part of MeshBerry.
 *
 * Device-level settings (GPS, display, backlight, etc.)
 */

#ifndef MESHBERRY_DEVICE_SETTINGS_H
#define MESHBERRY_DEVICE_SETTINGS_H

#include <Arduino.h>

struct DeviceSettings {
    static constexpr uint32_t DEVICE_MAGIC = 0x4D424456;  // "MBDV"

    uint32_t magic = DEVICE_MAGIC;

    // GPS settings
    bool gpsEnabled = false;         // User-toggled GPS always-on
    bool gpsRtcSyncEnabled = true;   // Auto-sync RTC from GPS (default: on)

    // Display/backlight settings
    uint8_t backlightTimeout = 30;   // Seconds before keyboard backlight dims

    // Power saving / deep sleep settings
    bool powerSavingEnabled = false;    // Enable deep sleep mode
    uint16_t sleepTimeoutSecs = 120;    // Seconds of inactivity before sleep (2 min default)
    uint16_t sleepDurationSecs = 1800;  // Sleep duration before timer wake (30 min default)
    bool wakeOnLoRa = true;             // Wake when LoRa packet received
    bool wakeOnButton = true;           // Wake on trackball click

    uint8_t reserved[8] = {0};          // Future expansion

    void setDefaults() {
        magic = DEVICE_MAGIC;
        gpsEnabled = false;
        gpsRtcSyncEnabled = true;
        backlightTimeout = 30;
        powerSavingEnabled = false;
        sleepTimeoutSecs = 120;
        sleepDurationSecs = 1800;
        wakeOnLoRa = true;
        wakeOnButton = true;
        memset(reserved, 0, sizeof(reserved));
    }

    bool isValid() const {
        return magic == DEVICE_MAGIC;
    }
};

#endif // MESHBERRY_DEVICE_SETTINGS_H
