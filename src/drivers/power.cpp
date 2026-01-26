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

// MeshCore radio access (the ACTUAL hardware radio)
#include <helpers/radiolib/CustomSX1262.h>
#include "mesh/MeshBerrySX1262Wrapper.h"
extern CustomSX1262* radio;  // Global from main.cpp - this is MeshCore's radio
extern MeshBerrySX1262Wrapper* radioWrapper;  // For ISR re-attachment after sleep wake

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
        // DIO1 stuck HIGH - try to clear the IRQ flags
        // This can happen if MeshCore processed a packet but didn't clear the interrupt
        if (radio) {
            Serial.println("[POWER] Preflight: DIO1 HIGH - attempting IRQ clear");
            radio->clearIrqFlags(0x03FF);  // Clear all IRQ flags
            radio->startReceive();  // Re-arm the receiver
            delay(10);  // Give it time to settle

            // Re-check after clearing
            if (digitalRead(PIN_LORA_DIO1) == HIGH) {
                Serial.println("[POWER] Preflight: BLOCKED - DIO1 still HIGH after clear");
                return false;
            }
            Serial.println("[POWER] Preflight: DIO1 cleared successfully");
        } else {
            Serial.println("[POWER] Preflight: BLOCKED - DIO1 HIGH (no radio)");
            return false;
        }
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
            // GPIO wake - could be trackball or LoRa
            // Check trackball click first (GPIO 0 active LOW)
            // Use register read since GPIO 0 is a strapping pin
            bool trackballPressed = (REG_READ(GPIO_IN_REG) & BIT(PIN_TRACKBALL_CLICK)) == 0;
            if (trackballPressed) {
                Serial.println("[POWER] Trackball GPIO wake");
                onUserActivity();
                return;  // Exit sleep loop - instant wake!
            }

            // Check if LoRa DIO1 is HIGH (packet received)
            if (digitalRead(PIN_LORA_DIO1) == HIGH) {
                Serial.println("[POWER] LoRa wake - processing packet");

                // Process mesh for minWakeSecs seconds
                unsigned long wakeStart = millis();
                while (millis() - wakeStart < minWakeSecs * 1000UL) {
                    if (theMesh) {
                        // Let interrupt handlers run
                    }
                    delay(10);

                    if (millis() - lastActivityTime < 1000) {
                        Serial.println("[POWER] User activity during LoRa wake");
                        return;
                    }
                }

                Serial.println("[POWER] LoRa processed - resuming sleep");
                continue;
            }

            // Unknown GPIO wake - treat as user activity
            Serial.println("[POWER] Unknown GPIO wake");
            onUserActivity();
            return;
        }

        // Timer wake - trackball GPIO wake should have caught trackball clicks
        // so if we get here on timer, just accumulate sleep time and continue
        // (Keyboard is disabled during sleep - Meshtastic approach)

        // No activity - accumulate sleep time and continue
        totalSleptSecs += sleepIntervalSecs;
    }
}

// =============================================================================
// LIGHT SLEEP IMPLEMENTATION
// =============================================================================

void enterLightSleep(uint32_t timerSecs, bool wakeOnLoRa) {
    Serial.println("[SLEEP] Entering sleep");
    Serial.flush();

    // Disable watchdog before sleep to prevent timeout during sleep cycles
    esp_task_wdt_delete(NULL);

    // Shutdown peripherals before sleep to save power (~20-50mA → ~2-5mA)
    Keyboard::setBacklight(false);   // Turn off keyboard backlight (~5mA)
    Display::backlightOff();         // Turn off display backlight (~10-15mA)

    // Suspend I2C bus (keyboard, touchscreen) - saves ~5-10mA
    Wire.end();

    // CRITICAL: Meshtastic sets I2C pins to ANALOG mode after Wire.end()
    // This prevents pullup current draw during sleep
    pinMode(PIN_KB_SDA, ANALOG);
    pinMode(PIN_KB_SCL, ANALOG);

    // CRITICAL: Keep RTC peripherals powered during sleep (Meshtastic pattern)
    esp_sleep_pd_config(ESP_PD_DOMAIN_RTC_PERIPH, ESP_PD_OPTION_ON);

    // STEP 1: Configure radio and detach ISR FIRST (before gpio_wakeup_enable)
    // Per ESP32 forum: https://www.esp32.com/viewtopic.php?t=39011
    // detachInterrupt() must be called BEFORE gpio_wakeup_enable() or it interferes
    if (radio) {
        radio->clearPacketReceivedAction();  // Detach RadioLib ISR

        if (wakeOnLoRa) {
            // Keep radio in RX mode so it can receive packets and trigger DIO1
            radio->startReceive();
            Serial.println("[SLEEP] Radio staying in RX mode for LoRa wake");
        } else {
            // No LoRa wake needed - put radio to sleep to save power
            radio->sleep(false);
        }
    }
    detachInterrupt(PIN_LORA_DIO1);  // Detach Arduino ISR BEFORE gpio_wakeup_enable

    // STEP 2: Enable GPIO wakeup system (after ISR is detached)
    esp_sleep_enable_gpio_wakeup();

    // STEP 3: Configure GPIO wake sources
    // LoRa wake - DIO1 goes HIGH when packet received
    if (wakeOnLoRa) {
        // Keep CS high (deselected) during sleep
        pinMode(PIN_LORA_CS, OUTPUT);
        digitalWrite(PIN_LORA_CS, HIGH);
        gpio_hold_en((gpio_num_t)PIN_LORA_CS);

        // Configure DIO1 as INPUT for wake detection
        // SX1262 DIO1 is push-pull output, no internal pulldown needed
        pinMode(PIN_LORA_DIO1, INPUT);

        // Enable GPIO wake on HIGH level (packet received)
        gpio_wakeup_enable((gpio_num_t)PIN_LORA_DIO1, GPIO_INTR_HIGH_LEVEL);
        Serial.printf("[SLEEP] DIO1 configured for wake (current state: %d)\n", digitalRead(PIN_LORA_DIO1));
    }

    // Trackball click wake - GPIO 0 goes LOW when pressed
    gpio_wakeup_enable((gpio_num_t)PIN_TRACKBALL_CLICK, GPIO_INTR_LOW_LEVEL);

    // Timer wake
    if (timerSecs > 0) {
        esp_sleep_enable_timer_wakeup((uint64_t)timerSecs * 1000000ULL);
    }

    // Wait for peripherals to settle
    delay(200);  // CRITICAL: Longer delay to ensure everything settled (Meshtastic uses 50-100ms)

    // Enter light sleep - let ESP-IDF handle interrupt management
    esp_err_t sleepResult = esp_light_sleep_start();

    // Check if sleep succeeded
    if (sleepResult != ESP_OK) {
        Serial.printf("[SLEEP] ERROR: Sleep failed (code %d)\n", sleepResult);
        Serial.flush();

        // Re-add watchdog before returning
        esp_task_wdt_add(NULL);
        esp_task_wdt_reset();

        // Restore peripherals before returning
        delay(50);
        Wire.begin(PIN_KB_SDA, PIN_KB_SCL, KB_I2C_FREQ);
        delay(100);
        Keyboard::init();
        Display::backlightOn();
        Keyboard::setBacklight(true);

        return;  // Exit without completing sleep
    }

    // === WOKE FROM SLEEP ===

    // Release GPIO holds and disable wake sources
    if (wakeOnLoRa) {
        gpio_hold_dis((gpio_num_t)PIN_LORA_CS);  // Release CS hold
        gpio_wakeup_disable((gpio_num_t)PIN_LORA_DIO1);
    }
    gpio_wakeup_disable((gpio_num_t)PIN_TRACKBALL_CLICK);  // Disable trackball wake

    // Wake MeshCore's radio (the actual hardware)
    if (radio) {
        radio->standby();  // Exit sleep mode
        delay(5);  // Allow radio to settle
        radio->clearIrqFlags(0x03FF);  // Clear all interrupt flags
    }

    // CRITICAL: Re-attach ISR that was detached before sleep
    // radioWrapper->begin() calls setPacketReceivedAction(setFlag) to restore interrupt handling
    // Without this, packets arrive but MeshCore's state flag never gets set, dropping all packets
    if (radioWrapper) {
        radioWrapper->begin();
        Serial.println("[SLEEP] Radio ISR re-attached");
    }

    // Get wake cause for timing decisions
    esp_sleep_wakeup_cause_t cause = esp_sleep_get_wakeup_cause();

    if (cause == ESP_SLEEP_WAKEUP_GPIO) {
        // Longer delay to let radio/interrupt stabilize after GPIO wake
        delay(50);
    } else {
        delay(10);  // Brief delay for other wake sources
    }

    esp_sleep_disable_wakeup_source(ESP_SLEEP_WAKEUP_ALL);

    // Restore peripherals after wake
    delay(50);  // Stabilization time for peripherals
    Wire.begin(PIN_KB_SDA, PIN_KB_SCL, KB_I2C_FREQ);  // Restart I2C bus
    Keyboard::init();                    // Re-initialize keyboard
    Display::backlightOn();              // Turn on display backlight
    Keyboard::setBacklight(true);        // Turn on keyboard backlight

    // Re-add task to watchdog and reset it after wake
    esp_task_wdt_add(NULL);
    esp_task_wdt_reset();

    Serial.println("[SLEEP] Woke from sleep");
    Serial.flush();
}

} // namespace Power
