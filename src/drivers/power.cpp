/**
 * MeshBerry Power Management Driver Implementation
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2026 NodakMesh (nodakmesh.org)
 *
 * Meshtastic-style power state machine implementation.
 * Uses light sleep with 30-second intervals and proper wake handling.
 */

#include "power.h"
#include "display.h"
#include "keyboard.h"
#include <driver/rtc_io.h>
#include <esp_sleep.h>

// Forward declaration for mesh pending work check
class MeshBerryMesh;
extern MeshBerryMesh* theMesh;

namespace Power {

// Forward declarations
static void runSleepLoop(uint32_t sleepIntervalSecs, uint32_t minWakeSecs, bool wakeOnLoRa);

// =============================================================================
// STATE VARIABLES
// =============================================================================

// Exported state
SleepState currentState = STATE_AWAKE;
uint32_t lastActivityTime = 0;
uint32_t screenOffTime = 0;
uint32_t totalSleptSecs = 0;

// Internal state
static WakeReason wakeReason = WAKE_UNKNOWN;
static bool initialized = false;
static bool fromDeepSleep = false;

// =============================================================================
// INITIALIZATION
// =============================================================================

void init() {
    if (initialized) return;

    // Initialize state
    currentState = STATE_AWAKE;
    lastActivityTime = millis();
    screenOffTime = 0;
    totalSleptSecs = 0;

    // Determine reset/wake reason
    esp_reset_reason_t resetReason = esp_reset_reason();

    if (resetReason == ESP_RST_DEEPSLEEP) {
        fromDeepSleep = true;

        // Determine which source woke us
        esp_sleep_wakeup_cause_t wakeupCause = esp_sleep_get_wakeup_cause();

        switch (wakeupCause) {
            case ESP_SLEEP_WAKEUP_TIMER:
                wakeReason = WAKE_TIMER;
                Serial.println("[POWER] Wake reason: Timer");
                break;

            case ESP_SLEEP_WAKEUP_EXT0:
                wakeReason = WAKE_BUTTON;
                Serial.println("[POWER] Wake reason: Button press (EXT0)");
                break;

            case ESP_SLEEP_WAKEUP_EXT1: {
                uint64_t wakeupStatus = esp_sleep_get_ext1_wakeup_status();
                if (wakeupStatus & (1ULL << PIN_LORA_DIO1)) {
                    wakeReason = WAKE_LORA_PACKET;
                    Serial.println("[POWER] Wake reason: LoRa packet (EXT1)");
                } else {
                    wakeReason = WAKE_UNKNOWN;
                    Serial.printf("[POWER] Wake reason: Unknown EXT1 (status=0x%llx)\n", wakeupStatus);
                }
                break;
            }

            default:
                wakeReason = WAKE_UNKNOWN;
                Serial.printf("[POWER] Wake reason: Unknown (%d)\n", wakeupCause);
                break;
        }

        releaseRtcGpioHolds();
    } else {
        fromDeepSleep = false;
        wakeReason = WAKE_POWER_ON;
        Serial.printf("[POWER] Reset reason: %d (not from deep sleep)\n", resetReason);
    }

    initialized = true;
}

WakeReason getWakeReason() {
    return wakeReason;
}

const char* getWakeReasonString() {
    switch (wakeReason) {
        case WAKE_TIMER:       return "Timer";
        case WAKE_BUTTON:      return "Button";
        case WAKE_LORA_PACKET: return "LoRa Packet";
        case WAKE_POWER_ON:    return "Power On";
        default:               return "Unknown";
    }
}

bool wokeFromSleep() {
    return fromDeepSleep;
}

void releaseRtcGpioHolds() {
    if (!fromDeepSleep) return;

    Serial.println("[POWER] Releasing RTC GPIO holds...");
    rtc_gpio_hold_dis((gpio_num_t)PIN_LORA_CS);
    rtc_gpio_deinit((gpio_num_t)PIN_LORA_DIO1);
    rtc_gpio_deinit((gpio_num_t)PIN_LORA_CS);
    Serial.println("[POWER] RTC GPIO holds released");
}

// =============================================================================
// PREFLIGHT CHECKS
// =============================================================================

bool doPreflightSleep() {
    // Check if LoRa DIO1 is HIGH (packet incoming/received)
    if (digitalRead(PIN_LORA_DIO1) == HIGH) {
        Serial.println("[POWER] Preflight: BLOCKED - DIO1 HIGH");
        return false;
    }

    // Check if radio is busy (transmitting)
    if (digitalRead(PIN_LORA_BUSY) == HIGH) {
        Serial.println("[POWER] Preflight: BLOCKED - Radio BUSY");
        return false;
    }

    // Check if mesh has pending work (DM retries, etc.)
    // Note: theMesh is declared extern above
    if (theMesh) {
        // Use hasPendingWork() method if available
        // For now, just check radio state which is the main concern
    }

    return true;
}

// Legacy alias
bool safeToSleep() {
    return doPreflightSleep();
}

// =============================================================================
// STATE QUERIES
// =============================================================================

bool isScreenOff() {
    return currentState == STATE_SCREEN_OFF || currentState == STATE_SLEEPING;
}

bool isSleeping() {
    return currentState == STATE_SLEEPING;
}

// =============================================================================
// USER ACTIVITY HANDLING
// =============================================================================

void onUserActivity() {
    lastActivityTime = millis();

    // Wake from screen-off state
    if (currentState == STATE_SCREEN_OFF) {
        Serial.println("[POWER] User activity - screen on");
        Display::backlightOn();
        Keyboard::setBacklight(true);
        currentState = STATE_AWAKE;
    }
    // Note: sleeping state is handled inside the sleep loop
}

// =============================================================================
// POWER STATE MACHINE
// =============================================================================

void updatePowerState(uint32_t screenTimeoutSecs, uint32_t sleepTimeoutSecs,
                      uint32_t sleepIntervalSecs, uint32_t minWakeSecs,
                      bool wakeOnLoRa) {

    // Skip if sleep is disabled
    if (sleepTimeoutSecs == 0) {
        return;
    }

    uint32_t now = millis();
    uint32_t inactiveMs = now - lastActivityTime;

    switch (currentState) {
        case STATE_AWAKE:
            // Check for screen timeout
            if (screenTimeoutSecs > 0 && inactiveMs > screenTimeoutSecs * 1000UL) {
                Serial.println("[POWER] Screen timeout - turning off backlight");
                Display::backlightOff();
                Keyboard::setBacklight(false);
                screenOffTime = now;
                currentState = STATE_SCREEN_OFF;
            }
            break;

        case STATE_SCREEN_OFF:
            // Check for sleep entry
            if (now - screenOffTime > sleepTimeoutSecs * 1000UL) {
                if (doPreflightSleep()) {
                    Serial.println("[POWER] Entering sleep mode");
                    currentState = STATE_SLEEPING;
                    totalSleptSecs = 0;

                    // Run the sleep loop
                    runSleepLoop(sleepIntervalSecs, minWakeSecs, wakeOnLoRa);

                    // When we exit the loop, we're fully awake
                    currentState = STATE_AWAKE;
                    Display::backlightOn();
                    Keyboard::setBacklight(true);
                    lastActivityTime = millis();
                    Serial.println("[POWER] Fully awake");
                }
            }
            break;

        case STATE_SLEEPING:
            // This shouldn't happen - we stay in runSleepLoop until wake
            currentState = STATE_AWAKE;
            break;
    }
}

// =============================================================================
// SLEEP LOOP (Meshtastic-style)
// =============================================================================

static void runSleepLoop(uint32_t sleepIntervalSecs, uint32_t minWakeSecs, bool wakeOnLoRa) {
    while (currentState == STATE_SLEEPING) {
        // Preflight check before each sleep cycle
        if (!doPreflightSleep()) {
            Serial.println("[POWER] Preflight failed - exiting sleep");
            break;
        }

        Serial.printf("[POWER] Sleep cycle (slept %u secs total)\n", totalSleptSecs);
        Serial.flush();

        // Enter light sleep for one interval
        enterLightSleep(sleepIntervalSecs, wakeOnLoRa);

        // Check what woke us
        esp_sleep_wakeup_cause_t cause = esp_sleep_get_wakeup_cause();

        if (cause == ESP_SLEEP_WAKEUP_GPIO) {
            // LoRa packet received - process it briefly then return to sleep
            Serial.println("[POWER] LoRa wake - processing packet");

            // Process mesh for minWakeSecs seconds
            unsigned long wakeStart = millis();
            while (millis() - wakeStart < minWakeSecs * 1000UL) {
                // Process mesh packets
                if (theMesh) {
                    // Call mesh loop - need to include the header
                    // For now, just delay to let interrupt handlers run
                }
                delay(10);

                // Check if user did something during this time
                // (keyboard read happens in main loop, so check lastActivityTime)
                if (millis() - lastActivityTime < 1000) {
                    Serial.println("[POWER] User activity during LoRa wake");
                    return;  // Exit sleep loop
                }
            }

            // No user activity - continue sleeping
            Serial.println("[POWER] LoRa processed - resuming sleep");
            continue;
        }

        // Timer wake - check for keyboard input
        uint8_t key = Keyboard::read();
        if (key != KEY_NONE) {
            Serial.println("[POWER] Keyboard wake");
            onUserActivity();
            return;  // Exit sleep loop
        }

        // Check trackball (GPIO 0) using direct register read
        // GPIO 0 is a strapping pin so we use register read
        bool trackballClick = (REG_READ(GPIO_IN_REG) & BIT(PIN_TRACKBALL_CLICK)) == 0;  // Active LOW
        if (trackballClick) {
            Serial.println("[POWER] Trackball wake");
            onUserActivity();
            return;  // Exit sleep loop
        }

        // No activity - accumulate sleep time and continue
        totalSleptSecs += sleepIntervalSecs;

        // Optional: max sleep time limit (e.g., 24 hours)
        // For now, keep sleeping until user activity
    }
}

// =============================================================================
// LIGHT SLEEP IMPLEMENTATION
// =============================================================================

void enterLightSleep(uint32_t timerSecs, bool wakeOnLoRa) {
    // Enable GPIO wakeup system
    esp_sleep_enable_gpio_wakeup();

    // LoRa wake - DIO1 goes HIGH when packet received
    // GPIO 45 is NOT a strapping pin, safe to use
    if (wakeOnLoRa) {
        gpio_wakeup_enable((gpio_num_t)PIN_LORA_DIO1, GPIO_INTR_HIGH_LEVEL);
    }

    // Timer wake
    if (timerSecs > 0) {
        esp_sleep_enable_timer_wakeup((uint64_t)timerSecs * 1000000ULL);
    }

    // Enter light sleep
    noInterrupts();
    esp_light_sleep_start();

    // Disable wake sources
    if (wakeOnLoRa) {
        gpio_wakeup_disable((gpio_num_t)PIN_LORA_DIO1);
    }
    interrupts();

    // Get wake cause for logging
    esp_sleep_wakeup_cause_t cause = esp_sleep_get_wakeup_cause();
    if (cause == ESP_SLEEP_WAKEUP_GPIO) {
        // Small delay to let radio/interrupt stabilize
        delay(10);
    }

    esp_sleep_disable_wakeup_source(ESP_SLEEP_WAKEUP_ALL);

    // Reset strapping pin GPIOs after wake (they may get corrupted)
    gpio_reset_pin((gpio_num_t)PIN_TRACKBALL_CLICK);
    gpio_reset_pin((gpio_num_t)PIN_TRACKBALL_LEFT);
    pinMode(PIN_TRACKBALL_CLICK, INPUT_PULLUP);
    pinMode(PIN_TRACKBALL_LEFT, INPUT_PULLUP);
}

} // namespace Power
