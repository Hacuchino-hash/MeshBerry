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

/**
 * Alert tone options
 * TONE_NONE = silent (no sound for this alert)
 * Other values select which tone pattern to play
 */
enum AlertTone : uint8_t {
    TONE_NONE = 0,          // Silent - no sound
    TONE_BBM_PING,          // Classic BBM-style ping (default for messages)
    TONE_CHIRP,             // Quick single chirp
    TONE_DOUBLE_BEEP,       // Two quick beeps
    TONE_ASCENDING,         // Ascending three-tone
    TONE_DESCENDING,        // Descending two-tone
    TONE_WARNING,           // Warning/alert tone
    TONE_MARIO_COIN,        // Mario coin sound
    TONE_COUNT              // Number of tone options
};

// Helper to get tone name for UI
inline const char* getAlertToneName(AlertTone tone) {
    switch (tone) {
        case TONE_NONE:       return "None";
        case TONE_BBM_PING:   return "BBM Ping";
        case TONE_CHIRP:      return "Chirp";
        case TONE_DOUBLE_BEEP: return "Double Beep";
        case TONE_ASCENDING:  return "Ascending";
        case TONE_DESCENDING: return "Descending";
        case TONE_WARNING:    return "Warning";
        case TONE_MARIO_COIN: return "Mario Coin";
        default:              return "Unknown";
    }
}

struct DeviceSettings {
    static constexpr uint32_t DEVICE_MAGIC = 0x4D424456;  // "MBDV"

    uint32_t magic = DEVICE_MAGIC;

    // GPS settings
    bool gpsEnabled = false;         // User-toggled GPS always-on
    bool gpsRtcSyncEnabled = true;   // Auto-sync RTC from GPS (default: on)

    // Display/backlight settings
    uint8_t backlightTimeout = 30;   // Seconds before keyboard backlight dims

    // Sleep settings (sleep is always enabled for power savings)
    bool useDeepSleep = false;          // false = light sleep (default), true = deep sleep (lower power but reboots on wake)
    uint16_t sleepTimeoutSecs = 120;    // Seconds of inactivity before sleep (2 min default)
    uint16_t sleepDurationSecs = 1800;  // Sleep duration before timer wake (30 min default)
    bool wakeOnLoRa = true;             // Wake when LoRa packet received (light sleep only)
    bool wakeOnButton = true;           // Wake on trackball click

    // Audio settings
    uint8_t audioVolume = 80;           // Master volume 0-100
    bool audioMuted = false;            // Global mute

    // Per-alert tone selection (TONE_NONE = silent)
    AlertTone toneMessage = TONE_BBM_PING;      // Incoming message notification
    AlertTone toneNodeConnect = TONE_ASCENDING; // New node discovered
    AlertTone toneLowBattery = TONE_WARNING;    // Low battery warning
    AlertTone toneSent = TONE_CHIRP;            // Message sent confirmation
    AlertTone toneError = TONE_DESCENDING;      // Error occurred

    uint8_t reserved[4] = {0};          // Future expansion (reduced from 8)

    void setDefaults() {
        magic = DEVICE_MAGIC;
        gpsEnabled = false;
        gpsRtcSyncEnabled = true;
        backlightTimeout = 30;
        useDeepSleep = false;
        sleepTimeoutSecs = 120;
        sleepDurationSecs = 1800;
        wakeOnLoRa = true;
        wakeOnButton = true;

        // Audio defaults
        audioVolume = 80;
        audioMuted = false;
        toneMessage = TONE_BBM_PING;
        toneNodeConnect = TONE_ASCENDING;
        toneLowBattery = TONE_WARNING;
        toneSent = TONE_CHIRP;
        toneError = TONE_DESCENDING;

        memset(reserved, 0, sizeof(reserved));
    }

    bool isValid() const {
        return magic == DEVICE_MAGIC;
    }
};

#endif // MESHBERRY_DEVICE_SETTINGS_H
