/**
 * MeshBerry T-Deck Board Abstraction
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2026 NodakMesh (nodakmesh.org)
 *
 * This file is part of MeshBerry.
 *
 * MeshBerry is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * MeshBerry is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with MeshBerry. If not, see <https://www.gnu.org/licenses/>.
 *
 * Hardware: LILYGO T-Deck / T-Deck Plus
 * Based on MeshCore ESP32Board abstraction (MIT licensed)
 */

#ifndef MESHBERRY_TDECK_BOARD_H
#define MESHBERRY_TDECK_BOARD_H

#include <Arduino.h>
#include <Wire.h>
#include <driver/rtc_io.h>
#include <MeshCore.h>
#include "../config.h"

// Battery reading configuration
#define BATTERY_SAMPLES         8
#define ADC_MULTIPLIER          (2.0f * 3.3f * 1000)  // mV

/**
 * T-Deck board abstraction implementing MeshCore's MainBoard interface
 */
class TDeckBoard : public mesh::MainBoard {
public:
    TDeckBoard();

    /**
     * Initialize the board hardware
     */
    void begin();

    /**
     * Get battery voltage in millivolts
     */
    uint16_t getBattMilliVolts() override;

    /**
     * Get battery percentage (0-100)
     */
    uint8_t getBatteryPercent();

    /**
     * Get manufacturer/board name
     */
    const char* getManufacturerName() const override;

    /**
     * Reboot the device
     */
    void reboot() override;

    /**
     * Power off the device (if supported)
     */
    void powerOff() override;

    /**
     * Get startup reason
     */
    uint8_t getStartupReason() const override;

    /**
     * Hook called before radio transmit
     */
    void onBeforeTransmit() override;

    /**
     * Hook called after radio transmit
     */
    void onAfterTransmit() override;

    // NOTE: Deep sleep removed - it corrupts GPIO 0 (trackball click) due to
    // rtc_gpio_init() on strapping pins, and GPIO 45 (LoRa DIO1) is not RTC-capable
    // on ESP32-S3. Use Power::enterLightSleep() instead.

    /**
     * Enable/disable peripheral power
     */
    void setPeripheralPower(bool enabled);

    /**
     * Check if peripheral power is enabled
     */
    bool isPeripheralPowerEnabled() const { return _peripheral_power_on; }

private:
    uint8_t _startup_reason;
    bool _peripheral_power_on;

    // Battery filtering for stable readings
    static constexpr int BATT_FILTER_SIZE = 8;  // Increased from 4 for more stable readings
    uint16_t _battFilter[BATT_FILTER_SIZE] = {0};
    int _battFilterIdx = 0;
    bool _battFilterFilled = false;
    uint16_t _lastReportedMV = 0;
};

#endif // MESHBERRY_TDECK_BOARD_H
