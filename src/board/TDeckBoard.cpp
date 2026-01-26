/**
 * MeshBerry T-Deck Board Implementation
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
 */

#include "TDeckBoard.h"

TDeckBoard::TDeckBoard()
    : _startup_reason(BD_STARTUP_NORMAL)
    , _peripheral_power_on(false)
{
}

void TDeckBoard::begin() {
    // Enable peripheral power first (required for LoRa and other peripherals)
    pinMode(PIN_PERIPHERAL_POWER, OUTPUT);
    setPeripheralPower(true);

    // NOTE: GPIO 0 (trackball click) is configured in main.cpp with ESP-IDF API
    // for interrupt-based detection. Don't configure it here to avoid conflicts.

    // Configure LoRa pins
    pinMode(PIN_LORA_MISO, INPUT_PULLUP);

    // Check if we woke from deep sleep
    esp_reset_reason_t reason = esp_reset_reason();
    if (reason == ESP_RST_DEEPSLEEP) {
        uint64_t wakeup_source = esp_sleep_get_ext1_wakeup_status();
        if (wakeup_source & (1ULL << PIN_LORA_DIO1)) {
            _startup_reason = BD_STARTUP_RX_PACKET;
        }

        // Release LoRa NSS hold from deep sleep
        rtc_gpio_hold_dis((gpio_num_t)PIN_LORA_CS);
        rtc_gpio_deinit((gpio_num_t)PIN_LORA_DIO1);
    }

    // Initialize battery filter with current reading for stable startup
    // This prevents erratic readings on first boot
    analogReadResolution(12);
    analogSetAttenuation(ADC_11db);
    analogRead(PIN_BATTERY_ADC);  // Discard first
    delay(10);

    uint32_t raw = 0;
    for (int i = 0; i < BATTERY_SAMPLES; i++) {
        raw += analogRead(PIN_BATTERY_ADC);
        delay(1);
    }
    raw = raw / BATTERY_SAMPLES;
    uint16_t initialMV = (ADC_MULTIPLIER * raw) / 4096;

    // Clamp initial reading too (in case booting while on charger)
    if (initialMV > 4250) initialMV = 4200;

    // Pre-fill the filter
    for (int i = 0; i < BATT_FILTER_SIZE; i++) {
        _battFilter[i] = initialMV;
    }
    _battFilterFilled = true;
    _lastReportedMV = initialMV;

    Serial.printf("[BOARD] Battery filter initialized: %dmV\n", initialMV);
    Serial.println("[BOARD] T-Deck board initialized");
}

uint16_t TDeckBoard::getBattMilliVolts() {
    // Re-configure ADC (required after deep sleep wake)
    analogReadResolution(12);
    analogSetAttenuation(ADC_11db);  // Full range 0-3.3V

    // Discard first reading to let ADC stabilize
    analogRead(PIN_BATTERY_ADC);
    delay(5);  // Longer delay for stability

    // Take multiple samples
    uint32_t raw = 0;
    for (int i = 0; i < BATTERY_SAMPLES; i++) {
        raw += analogRead(PIN_BATTERY_ADC);
        delay(1);  // Small delay between samples
    }
    raw = raw / BATTERY_SAMPLES;

    // Convert to millivolts
    uint16_t mv = (ADC_MULTIPLIER * raw) / 4096;

    // Clamp to reasonable battery range (ignore charger voltage)
    // LiPo max is 4.2V, anything above 4250mV is likely charger connected
    if (mv > 4250) {
        // When charging, use last known battery reading to prevent
        // the 100% -> 30% jump when unplugging
        if (_lastReportedMV > 0 && _lastReportedMV <= 4250) {
            mv = _lastReportedMV;  // Keep last battery reading
        } else {
            mv = 4200;  // Assume full when first plugged in
        }
    }

    // Add to moving average filter
    _battFilter[_battFilterIdx] = mv;
    _battFilterIdx = (_battFilterIdx + 1) % BATT_FILTER_SIZE;
    if (_battFilterIdx == 0) _battFilterFilled = true;

    // Calculate filtered average
    uint32_t sum = 0;
    int count = _battFilterFilled ? BATT_FILTER_SIZE : (_battFilterIdx > 0 ? _battFilterIdx : 1);
    for (int i = 0; i < count; i++) {
        sum += _battFilter[i];
    }
    uint16_t filtered = sum / count;

    // Apply hysteresis: only change reported value if difference > 50mV
    // Increased from 20mV to reduce jumpy readings from ESP32 ADC noise
    if (_lastReportedMV == 0 || abs((int)filtered - (int)_lastReportedMV) > 50) {
        _lastReportedMV = filtered;
    }

    return _lastReportedMV;
}

uint8_t TDeckBoard::getBatteryPercent() {
    uint16_t mv = getBattMilliVolts();

    // LiPo discharge curve approximation (more accurate than linear)
    // Based on typical single-cell LiPo discharge characteristics
    // The curve is non-linear with a plateau in the middle and steep drop at ends
    //
    // Voltage (mV) -> Percentage approximations:
    // 4200+ = 100%, 4100 = 90%, 4000 = 80%, 3900 = 70%, 3800 = 60%
    // 3700 = 40%, 3600 = 25%, 3500 = 15%, 3400 = 8%, 3300 = 4%, 3000 = 0%

    if (mv >= 4200) return 100;
    if (mv >= 4100) return 90 + (mv - 4100) / 10;        // 90-100%
    if (mv >= 4000) return 80 + (mv - 4000) / 10;        // 80-90%
    if (mv >= 3900) return 70 + (mv - 3900) / 10;        // 70-80%
    if (mv >= 3800) return 60 + (mv - 3800) / 10;        // 60-70%
    if (mv >= 3700) return 40 + (mv - 3700) / 5;         // 40-60% (steeper)
    if (mv >= 3600) return 25 + (mv - 3600) / 7;         // 25-40%
    if (mv >= 3500) return 15 + (mv - 3500) / 10;        // 15-25%
    if (mv >= 3400) return 8 + (mv - 3400) / 14;         // 8-15%
    if (mv >= 3300) return 4 + (mv - 3300) / 25;         // 4-8%
    if (mv >= 3000) return (mv - 3000) / 75;             // 0-4%

    return 0;
}

const char* TDeckBoard::getManufacturerName() const {
    return "MeshBerry T-Deck";
}

void TDeckBoard::reboot() {
    Serial.println("[BOARD] Rebooting...");
    Serial.flush();
    delay(100);
    esp_restart();
}

void TDeckBoard::powerOff() {
    // T-Deck doesn't have true power off capability
    // NOTE: Deep sleep removed - it corrupts GPIO 0 (trackball click)
    // Instead, we'll just reboot which is the safest option
    Serial.println("[BOARD] powerOff() called - rebooting instead");
    reboot();
}

uint8_t TDeckBoard::getStartupReason() const {
    return _startup_reason;
}

void TDeckBoard::onBeforeTransmit() {
    // Could toggle TX LED here if available
}

void TDeckBoard::onAfterTransmit() {
    // Could toggle TX LED here if available
}

// NOTE: enterDeepSleep() removed - it corrupts GPIO 0 (trackball click) due to
// rtc_gpio_init() on strapping pins, and GPIO 45 (LoRa DIO1) is not RTC-capable
// on ESP32-S3. Use Power::enterLightSleep() instead for power saving.

void TDeckBoard::setPeripheralPower(bool enabled) {
    digitalWrite(PIN_PERIPHERAL_POWER, enabled ? HIGH : LOW);
    _peripheral_power_on = enabled;

    if (enabled) {
        // Give peripherals time to power up
        // ESP32-C3 keyboard controller needs extra time to boot and init I2C slave
        delay(250);
    }
}
