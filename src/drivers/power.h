/**
 * MeshBerry Power Management Driver
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2026 NodakMesh (nodakmesh.org)
 *
 * Meshtastic-style power state machine with:
 * - Screen timeout (dim/off while awake)
 * - Sleep timeout (enter light sleep)
 * - LoRa wake (brief wake to process packets)
 * - Keyboard wake (full wake)
 *
 * Based on Meshtastic PowerFSM patterns.
 */

#ifndef MESHBERRY_POWER_H
#define MESHBERRY_POWER_H

#include <Arduino.h>
#include "../config.h"

namespace Power {

// =============================================================================
// POWER STATES (Meshtastic-style FSM)
// =============================================================================

/**
 * Power state machine states
 */
enum SleepState {
    STATE_AWAKE,          // Normal operation, screen on
    STATE_SCREEN_OFF,     // Screen off, device awake, waiting for sleep timeout
    STATE_SLEEPING,       // In light sleep cycles
};

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

// =============================================================================
// STATE TRACKING
// =============================================================================

// Current power state
extern SleepState currentState;

// Activity tracking
extern uint32_t lastActivityTime;    // Last user input (millis)
extern uint32_t screenOffTime;       // When screen turned off (millis)
extern uint32_t totalSleptSecs;      // Accumulated sleep time in current sleep session

// =============================================================================
// INITIALIZATION
// =============================================================================

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

// =============================================================================
// POWER STATE MACHINE (Call from main loop)
// =============================================================================

/**
 * Update power state machine
 * Call this once per loop() iteration. Handles:
 * - Screen timeout transitions
 * - Sleep entry/exit
 * - Wake cause handling
 *
 * @param screenTimeoutSecs Seconds of inactivity before screen off (0 = disabled)
 * @param sleepTimeoutSecs Seconds after screen off before sleep (0 = disabled)
 * @param sleepIntervalSecs Duration of each sleep cycle (default 30)
 * @param minWakeSecs Minimum wake time after LoRa packet (default 10)
 * @param wakeOnLoRa Enable LoRa wake source
 */
void updatePowerState(uint32_t screenTimeoutSecs, uint32_t sleepTimeoutSecs,
                      uint32_t sleepIntervalSecs = 30, uint32_t minWakeSecs = 10,
                      bool wakeOnLoRa = true);

/**
 * Notify power system of user activity
 * Call when keyboard, touch, or trackball input detected.
 * Resets activity timer and wakes from screen-off state.
 */
void onUserActivity();

/**
 * Check if screen is currently off (for UI updates)
 */
bool isScreenOff();

/**
 * Check if currently in sleep mode
 */
bool isSleeping();

// =============================================================================
// LOW-LEVEL SLEEP FUNCTIONS
// =============================================================================

/**
 * Preflight check before entering sleep
 * Returns false if sleep should be blocked (LoRa busy, pending work, etc.)
 */
bool doPreflightSleep();

/**
 * Enter light sleep mode for specified duration
 * @param timerSecs Timer wake in seconds (0 = no timer wake)
 * @param wakeOnLoRa Enable wake from LoRa packet (DIO1 HIGH)
 */
void enterLightSleep(uint32_t timerSecs = 0, bool wakeOnLoRa = true);

/**
 * Release RTC GPIO holds after wake
 * Called automatically by init() but can be called manually
 */
void releaseRtcGpioHolds();

/**
 * Legacy function - use doPreflightSleep() instead
 */
bool safeToSleep();

} // namespace Power

#endif // MESHBERRY_POWER_H
