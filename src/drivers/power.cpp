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
#include "config.h"  // For PIN_KB_SDA, PIN_KB_SCL, KB_I2C_FREQ
#include <Wire.h>
#include <driver/rtc_io.h>
#include <esp_sleep.h>
#include <esp_task_wdt.h>

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
    const uint32_t MAX_SLEEP_DURATION = 24 * 3600;  // 24 hours maximum

    while (currentState == STATE_SLEEPING) {
        // Check if we've exceeded maximum sleep duration
        if (totalSleptSecs >= MAX_SLEEP_DURATION) {
            Serial.printf("[POWER] Max sleep duration reached (%u hours) - forcing wake\n",
                          MAX_SLEEP_DURATION / 3600);
            currentState = STATE_AWAKE;
            break;
        }

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
    }
}

// =============================================================================
// LIGHT SLEEP IMPLEMENTATION
// =============================================================================

void enterLightSleep(uint32_t timerSecs, bool wakeOnLoRa) {
    // DIAGNOSTIC: Step-by-step logging to identify crash location
    Serial.println("[SLEEP] Step 1: Disabling watchdog");
    Serial.flush();

    // Disable watchdog before sleep to prevent timeout during sleep cycles
    esp_task_wdt_delete(NULL);

    Serial.println("[SLEEP] Step 2: Shutting down keyboard backlight");
    Serial.flush();

    // Shutdown peripherals before sleep to save power (~20-50mA → ~2-5mA)
    Keyboard::setBacklight(false);   // Turn off keyboard backlight (~5mA)

    Serial.println("[SLEEP] Step 3: Shutting down display backlight");
    Serial.flush();

    Display::backlightOff();         // Turn off display backlight (~10-15mA)

    Serial.println("[SLEEP] Step 4: Ending I2C bus");
    Serial.flush();

    // Suspend I2C bus (keyboard, touchscreen) - saves ~5-10mA
    Wire.end();

    Serial.println("[SLEEP] Step 5: Waiting for peripherals to settle");
    Serial.flush();
    delay(10);  // Give peripherals time to settle

    Serial.println("[SLEEP] Step 6: Resetting strapping pin GPIO 0");
    Serial.flush();

    // CRITICAL: Reset strapping pins BEFORE sleep to prevent corruption
    // GPIO 0 (trackball click) and GPIO 1 (trackball left) are ESP32-S3 strapping pins
    // If they're held LOW during reset, device enters DOWNLOAD mode → boot loop
    gpio_reset_pin((gpio_num_t)PIN_TRACKBALL_CLICK);

    Serial.println("[SLEEP] Step 7: Resetting strapping pin GPIO 1");
    Serial.flush();

    gpio_reset_pin((gpio_num_t)PIN_TRACKBALL_LEFT);

    Serial.println("[SLEEP] Step 8: Reconfiguring GPIO 0 as INPUT_PULLUP");
    Serial.flush();

    pinMode(PIN_TRACKBALL_CLICK, INPUT_PULLUP);

    Serial.println("[SLEEP] Step 9: Reconfiguring GPIO 1 as INPUT_PULLUP");
    Serial.flush();

    pinMode(PIN_TRACKBALL_LEFT, INPUT_PULLUP);

    Serial.println("[SLEEP] Step 10: Waiting for GPIO stabilization");
    Serial.flush();
    delay(10);  // Stabilization time for GPIO state

    Serial.println("[SLEEP] Step 11: Enabling GPIO wakeup system");
    Serial.flush();

    // Enable GPIO wakeup system
    esp_sleep_enable_gpio_wakeup();

    Serial.println("[SLEEP] Step 12: Configuring LoRa wakeup (if enabled)");
    Serial.flush();

    // LoRa wake - DIO1 goes HIGH when packet received
    // GPIO 45 is NOT a strapping pin, safe to use
    if (wakeOnLoRa) {
        gpio_wakeup_enable((gpio_num_t)PIN_LORA_DIO1, GPIO_INTR_HIGH_LEVEL);
    }

    Serial.println("[SLEEP] Step 13: Enabling timer wakeup");
    Serial.flush();

    // Timer wake
    if (timerSecs > 0) {
        esp_sleep_enable_timer_wakeup((uint64_t)timerSecs * 1000000ULL);
    }

    Serial.println("[SLEEP] Step 14: Resetting watchdog before sleep");
    Serial.flush();
    esp_task_wdt_reset();

    Serial.println("[SLEEP] Step 15: Entering light sleep NOW (ESP-IDF handles interrupts)");
    Serial.flush();
    delay(50);  // Ensure Serial buffer fully flushed

    // Enter light sleep - let ESP-IDF handle interrupt management
    // FIXED: Removed noInterrupts()/interrupts() calls which conflicted with LoRa radio
    esp_light_sleep_start();

    Serial.println("[SLEEP] Step 16: WOKE FROM SLEEP");
    Serial.flush();

    // Disable wake sources
    if (wakeOnLoRa) {
        gpio_wakeup_disable((gpio_num_t)PIN_LORA_DIO1);
    }

    Serial.println("[SLEEP] Step 17: Checking wake cause");
    Serial.flush();

    // Get wake cause for logging
    esp_sleep_wakeup_cause_t cause = esp_sleep_get_wakeup_cause();
    Serial.printf("[SLEEP] Wake cause: %d\n", cause);
    Serial.flush();

    if (cause == ESP_SLEEP_WAKEUP_GPIO) {
        // Longer delay to let radio/interrupt stabilize after GPIO wake
        delay(50);
    } else {
        delay(10);  // Brief delay for other wake sources
    }

    Serial.println("[SLEEP] Step 18: Disabling wake sources");
    Serial.flush();

    esp_sleep_disable_wakeup_source(ESP_SLEEP_WAKEUP_ALL);

    Serial.println("[SLEEP] Step 19: Resetting strapping pins after wake");
    Serial.flush();

    // Reset strapping pin GPIOs after wake (defensive redundancy)
    // Already reset before sleep, but reset again after wake for extra safety
    gpio_reset_pin((gpio_num_t)PIN_TRACKBALL_CLICK);
    gpio_reset_pin((gpio_num_t)PIN_TRACKBALL_LEFT);
    pinMode(PIN_TRACKBALL_CLICK, INPUT_PULLUP);
    pinMode(PIN_TRACKBALL_LEFT, INPUT_PULLUP);

    Serial.println("[SLEEP] Step 20: Restoring I2C bus");
    Serial.flush();

    // Restore peripherals after wake
    delay(50);  // Stabilization time for peripherals
    Wire.begin(PIN_KB_SDA, PIN_KB_SCL, KB_I2C_FREQ);  // Restart I2C bus

    Serial.println("[SLEEP] Step 21: Re-initializing keyboard");
    Serial.flush();

    Keyboard::init();                    // Re-initialize keyboard

    Serial.println("[SLEEP] Step 22: Turning on display backlight");
    Serial.flush();

    Display::backlightOn();              // Turn on display backlight

    Serial.println("[SLEEP] Step 23: Turning on keyboard backlight");
    Serial.flush();

    Keyboard::setBacklight(true);        // Turn on keyboard backlight

    Serial.println("[SLEEP] Step 24: Re-adding watchdog and resetting");
    Serial.flush();

    // Re-add task to watchdog and reset it after wake
    esp_task_wdt_add(NULL);
    esp_task_wdt_reset();

    Serial.println("[SLEEP] Step 25: SLEEP CYCLE COMPLETE - FIX APPLIED");
    Serial.flush();
}

} // namespace Power
