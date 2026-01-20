/**
 * MeshBerry Power Management Driver Implementation
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
 * ESP32 deep sleep implementation based on MeshCore patterns.
 */

#include "power.h"
#include <driver/rtc_io.h>
#include <esp_sleep.h>

// Internal state
static Power::WakeReason wakeReason = Power::WAKE_UNKNOWN;
static bool initialized = false;
static bool fromDeepSleep = false;

namespace Power {

void init() {
    if (initialized) return;

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

            case ESP_SLEEP_WAKEUP_EXT1: {
                // Check which GPIO triggered the wake
                uint64_t wakeupStatus = esp_sleep_get_ext1_wakeup_status();

                if (wakeupStatus & (1ULL << PIN_LORA_DIO1)) {
                    wakeReason = WAKE_LORA_PACKET;
                    Serial.println("[POWER] Wake reason: LoRa packet");
                } else if (wakeupStatus & (1ULL << PIN_TRACKBALL_CLICK)) {
                    wakeReason = WAKE_BUTTON;
                    Serial.println("[POWER] Wake reason: Button press");
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

        // Release RTC GPIO holds from sleep
        releaseRtcGpioHolds();

    } else {
        // Normal power-on or reset
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
        case WAKE_TIMER:      return "Timer";
        case WAKE_BUTTON:     return "Button";
        case WAKE_LORA_PACKET: return "LoRa Packet";
        case WAKE_POWER_ON:   return "Power On";
        default:              return "Unknown";
    }
}

bool wokeFromSleep() {
    return fromDeepSleep;
}

void prepareLoRaForSleep() {
    Serial.println("[POWER] Preparing LoRa for sleep...");

    // Configure LoRa DIO1 as RTC GPIO for wake interrupt
    // DIO1 goes HIGH when a packet is received
    rtc_gpio_init((gpio_num_t)PIN_LORA_DIO1);
    rtc_gpio_set_direction((gpio_num_t)PIN_LORA_DIO1, RTC_GPIO_MODE_INPUT_ONLY);
    rtc_gpio_pulldown_en((gpio_num_t)PIN_LORA_DIO1);
    rtc_gpio_pullup_dis((gpio_num_t)PIN_LORA_DIO1);

    // Hold LoRa CS pin state during sleep to maintain SPI state
    rtc_gpio_init((gpio_num_t)PIN_LORA_CS);
    rtc_gpio_hold_en((gpio_num_t)PIN_LORA_CS);

    Serial.println("[POWER] LoRa prepared for sleep");
}

void releaseRtcGpioHolds() {
    Serial.println("[POWER] Releasing RTC GPIO holds...");

    // Release LoRa CS hold
    rtc_gpio_hold_dis((gpio_num_t)PIN_LORA_CS);

    // Deinitialize RTC GPIOs to return to normal mode
    rtc_gpio_deinit((gpio_num_t)PIN_LORA_DIO1);
    rtc_gpio_deinit((gpio_num_t)PIN_LORA_CS);

    Serial.println("[POWER] RTC GPIO holds released");
}

bool safeToSleep() {
    // TODO: Add checks for:
    // - Active LoRa transmission
    // - Pending packet reception
    // - Active mesh forwarding
    // For now, always return true
    return true;
}

void enterDeepSleep(uint32_t timerSecs, bool wakeOnButton, bool wakeOnLoRa) {
    Serial.println("[POWER] Entering deep sleep...");
    Serial.printf("[POWER] Config: timer=%us, button=%d, lora=%d\n",
                  timerSecs, wakeOnButton, wakeOnLoRa);

    // Keep RTC peripherals powered for wake functionality
    esp_sleep_pd_config(ESP_PD_DOMAIN_RTC_PERIPH, ESP_PD_OPTION_ON);

    // Build wake source mask
    uint64_t wakeMask = 0;

    if (wakeOnLoRa) {
        wakeMask |= (1ULL << PIN_LORA_DIO1);
        Serial.println("[POWER] LoRa wake enabled (DIO1)");
    }

    if (wakeOnButton) {
        // Configure trackball click as RTC GPIO
        rtc_gpio_init((gpio_num_t)PIN_TRACKBALL_CLICK);
        rtc_gpio_set_direction((gpio_num_t)PIN_TRACKBALL_CLICK, RTC_GPIO_MODE_INPUT_ONLY);
        // Button is active LOW, so we need pullup and wake on LOW
        // But ext1 only supports ANY_HIGH, so we may need ext0 for button
        // For now, try with ANY_HIGH (button might have external pulldown)
        wakeMask |= (1ULL << PIN_TRACKBALL_CLICK);
        Serial.println("[POWER] Button wake enabled (GPIO 0)");
    }

    // Enable EXT1 wake (multiple GPIO sources)
    if (wakeMask != 0) {
        // Note: ESP_EXT1_WAKEUP_ANY_HIGH means wake when ANY of the pins go HIGH
        // The trackball click on T-Deck may need different configuration
        esp_sleep_enable_ext1_wakeup(wakeMask, ESP_EXT1_WAKEUP_ANY_HIGH);
    }

    // Enable timer wake if requested
    if (timerSecs > 0) {
        esp_sleep_enable_timer_wakeup((uint64_t)timerSecs * 1000000ULL);
        Serial.printf("[POWER] Timer wake enabled: %u seconds\n", timerSecs);
    }

    Serial.println("[POWER] Goodbye! Entering deep sleep now...");
    Serial.flush();

    // Small delay to ensure serial output is complete
    delay(100);

    // Enter deep sleep - this does not return
    esp_deep_sleep_start();

    // Never reached
}

} // namespace Power
