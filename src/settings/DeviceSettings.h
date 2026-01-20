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
    bool gpsEnabled = false;         // User-toggled GPS always-on
    bool gpsRtcSyncEnabled = true;   // Auto-sync RTC from GPS (default: on)
    uint8_t backlightTimeout = 30;   // Seconds (for future use)
    uint8_t reserved[16] = {0};      // Future expansion

    void setDefaults() {
        magic = DEVICE_MAGIC;
        gpsEnabled = false;
        gpsRtcSyncEnabled = true;
        backlightTimeout = 30;
        memset(reserved, 0, sizeof(reserved));
    }

    bool isValid() const {
        return magic == DEVICE_MAGIC;
    }
};

#endif // MESHBERRY_DEVICE_SETTINGS_H
