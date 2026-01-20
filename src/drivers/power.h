/**
 * MeshBerry Power Management Driver
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2026 NodakMesh (nodakmesh.org)
 *
 * ESP32 deep sleep management with multiple wake sources:
 * - LoRa packet received (DIO1 interrupt)
 * - Trackball click (GPIO 0)
 * - Timer wake
 *
 * Based on MeshCore T-Deck implementation patterns.
 */

#ifndef MESHBERRY_POWER_H
#define MESHBERRY_POWER_H

#include <Arduino.h>
#include "../config.h"

namespace Power {

/**
 * Reason for device wake/reset
 */
enum WakeReason {
    WAKE_UNKNOWN,       // Unknown wake source
    WAKE_TIMER,         // Timer expired
    WAKE_BUTTON,        // Trackball click (GPIO 0)
    WAKE_LORA_PACKET,   // LoRa DIO1 triggered
    WAKE_POWER_ON       // Normal power-on or reset
};

/**
 * Initialize power management
 * Call early in setup() to detect wake reason
 */
void init();

/**
 * Get the reason for last wake/reset
 * @return WakeReason enum value
 */
WakeReason getWakeReason();

/**
 * Get human-readable wake reason string
 */
const char* getWakeReasonString();

/**
 * Check if we woke from deep sleep
 */
bool wokeFromSleep();

/**
 * Enter deep sleep mode
 *
 * @param timerSecs Timer wake in seconds (0 = no timer wake)
 * @param wakeOnButton Enable wake from trackball click
 * @param wakeOnLoRa Enable wake from LoRa packet
 *
 * Note: This function does not return. Device will reset on wake.
 */
void enterDeepSleep(uint32_t timerSecs = 0, bool wakeOnButton = true, bool wakeOnLoRa = true);

/**
 * Prepare LoRa radio for sleep
 * Configures DIO1 as RTC GPIO for wake interrupt
 * Call before enterDeepSleep()
 */
void prepareLoRaForSleep();

/**
 * Check if it's safe to enter sleep
 * Returns false if:
 * - Currently transmitting
 * - In the middle of receiving a packet
 * - Other pending operations
 */
bool safeToSleep();

/**
 * Release RTC GPIO holds after wake
 * Called automatically by init() but can be called manually
 */
void releaseRtcGpioHolds();

} // namespace Power

#endif // MESHBERRY_POWER_H
