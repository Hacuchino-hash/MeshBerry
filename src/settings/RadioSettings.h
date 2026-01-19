/**
 * MeshBerry Radio Settings
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2026 NodakMesh (nodakmesh.org)
 *
 * This file is part of MeshBerry.
 *
 * Runtime-configurable LoRa radio settings with region presets.
 * Compatible with MeshCore radio parameters.
 */

#ifndef MESHBERRY_RADIO_SETTINGS_H
#define MESHBERRY_RADIO_SETTINGS_H

#include <Arduino.h>

// =============================================================================
// REGION DEFINITIONS
// =============================================================================

enum LoRaRegion : uint8_t {
    REGION_US = 0,      // US/Canada 910.525 MHz narrow
    REGION_UK = 1,      // UK 869.525 MHz
    REGION_EU = 2,      // EU (same as UK)
    REGION_AU = 3,      // Australia/NZ 916.8 MHz
    REGION_CUSTOM = 4   // User-defined
};

// =============================================================================
// RADIO SETTINGS STRUCTURE
// =============================================================================

struct RadioSettings {
    // Region preset
    LoRaRegion region;

    // Radio parameters (MeshCore compatible)
    float frequency;        // MHz (e.g., 915.0, 869.525)
    float bandwidth;        // kHz (125, 250, 500)
    uint8_t spreadingFactor; // 5-12
    uint8_t codingRate;     // 5-8 (maps to 4/5 - 4/8)
    uint8_t txPower;        // 1-22 dBm

    // Magic number for validation
    uint32_t magic;

    // Initialize with defaults (US narrow preset - MeshCore Oct 2025)
    void setDefaults() {
        region = REGION_US;
        frequency = 910.525f;   // US narrow preset
        bandwidth = 62.5f;      // Narrow bandwidth
        spreadingFactor = 7;    // SF7 for narrow
        codingRate = 5;         // CR 4/5
        txPower = 22;
        magic = SETTINGS_MAGIC;
    }

    // Apply region preset (MeshCore narrow presets - Oct 2025)
    void setRegionPreset(LoRaRegion newRegion) {
        region = newRegion;

        switch (newRegion) {
            case REGION_US:
                // US/Canada narrow: 910.525 MHz, SF7, BW62.5, CR5
                frequency = 910.525f;
                bandwidth = 62.5f;
                spreadingFactor = 7;
                codingRate = 5;
                txPower = 22;
                break;

            case REGION_UK:
            case REGION_EU:
                // UK/EU narrow: 869.525 MHz, SF8, BW62.5, CR5
                frequency = 869.525f;
                bandwidth = 62.5f;
                spreadingFactor = 8;
                codingRate = 5;
                txPower = 22;
                break;

            case REGION_AU:
                // Australia/NZ: 916.8 MHz, SF9, BW125, CR5
                frequency = 916.8f;
                bandwidth = 125.0f;
                spreadingFactor = 9;
                codingRate = 5;
                txPower = 22;
                break;

            case REGION_CUSTOM:
                // Keep current values
                break;
        }
    }

    // Validate settings are within acceptable ranges
    bool isValid() const {
        if (magic != SETTINGS_MAGIC) return false;
        if (frequency < 137.0f || frequency > 1020.0f) return false;
        // Accept 62.5, 125, 250, 500 kHz bandwidths
        if (bandwidth != 62.5f && bandwidth != 125.0f && bandwidth != 250.0f && bandwidth != 500.0f) return false;
        if (spreadingFactor < 5 || spreadingFactor > 12) return false;
        if (codingRate < 5 || codingRate > 8) return false;
        if (txPower < 1 || txPower > 22) return false;
        return true;
    }

    // Get region name string
    const char* getRegionName() const {
        switch (region) {
            case REGION_US: return "US/CA";
            case REGION_UK: return "UK";
            case REGION_EU: return "EU";
            case REGION_AU: return "AU/NZ";
            case REGION_CUSTOM: return "Custom";
            default: return "Unknown";
        }
    }

    // Get bandwidth as string
    const char* getBandwidthStr() const {
        if (bandwidth == 62.5f) return "62.5";
        if (bandwidth == 125.0f) return "125";
        if (bandwidth == 250.0f) return "250";
        if (bandwidth == 500.0f) return "500";
        return "???";
    }

    static constexpr uint32_t SETTINGS_MAGIC = 0x4D425253;  // "MBRS"
};

// =============================================================================
// BANDWIDTH OPTIONS
// =============================================================================

constexpr float BW_62_5 = 62.5f;   // Narrow (MeshCore default)
constexpr float BW_125 = 125.0f;
constexpr float BW_250 = 250.0f;
constexpr float BW_500 = 500.0f;

// =============================================================================
// TX POWER LIMITS
// =============================================================================

constexpr uint8_t TX_POWER_MIN = 1;
constexpr uint8_t TX_POWER_MAX = 22;

// =============================================================================
// SPREADING FACTOR LIMITS
// =============================================================================

constexpr uint8_t SF_MIN = 5;
constexpr uint8_t SF_MAX = 12;

#endif // MESHBERRY_RADIO_SETTINGS_H
