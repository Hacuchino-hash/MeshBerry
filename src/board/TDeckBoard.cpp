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

    // Configure trackball/user button
    pinMode(PIN_TRACKBALL_CLICK, INPUT);

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

    Serial.println("[BOARD] T-Deck board initialized");
}

uint16_t TDeckBoard::getBattMilliVolts() {
    analogReadResolution(12);

    uint32_t raw = 0;
    for (int i = 0; i < BATTERY_SAMPLES; i++) {
        raw += analogRead(PIN_BATTERY_ADC);
    }
    raw = raw / BATTERY_SAMPLES;

    return (ADC_MULTIPLIER * raw) / 4096;
}

uint8_t TDeckBoard::getBatteryPercent() {
    uint16_t mv = getBattMilliVolts();

    // Convert mV to percentage (linear approximation)
    // Full charge: 4200mV, Empty: 3000mV
    if (mv >= 4200) return 100;
    if (mv <= 3000) return 0;

    return (uint8_t)(((mv - 3000) * 100) / 1200);
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
    // T-Deck doesn't have true power off, enter deep sleep instead
    enterDeepSleep(0, PIN_TRACKBALL_CLICK);
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

void TDeckBoard::enterDeepSleep(uint32_t secs, int pin_wake_btn) {
    Serial.println("[BOARD] Entering deep sleep...");
    Serial.flush();

    // Configure RTC domain to stay powered
    esp_sleep_pd_config(ESP_PD_DOMAIN_RTC_PERIPH, ESP_PD_OPTION_ON);

    // Configure LoRa DIO1 as wake source (packet received)
    rtc_gpio_set_direction((gpio_num_t)PIN_LORA_DIO1, RTC_GPIO_MODE_INPUT_ONLY);
    rtc_gpio_pulldown_en((gpio_num_t)PIN_LORA_DIO1);

    // Hold LoRa NSS high during sleep
    rtc_gpio_hold_en((gpio_num_t)PIN_LORA_CS);

    // Configure wake sources
    if (pin_wake_btn < 0) {
        // Wake on LoRa packet only
        esp_sleep_enable_ext1_wakeup((1ULL << PIN_LORA_DIO1), ESP_EXT1_WAKEUP_ANY_HIGH);
    } else {
        // Wake on LoRa packet OR button press
        esp_sleep_enable_ext1_wakeup(
            (1ULL << PIN_LORA_DIO1) | (1ULL << pin_wake_btn),
            ESP_EXT1_WAKEUP_ANY_HIGH
        );
    }

    // Configure timer wake if requested
    if (secs > 0) {
        esp_sleep_enable_timer_wakeup(secs * 1000000ULL);
    }

    // Enter deep sleep (never returns)
    esp_deep_sleep_start();
}

void TDeckBoard::setPeripheralPower(bool enabled) {
    digitalWrite(PIN_PERIPHERAL_POWER, enabled ? HIGH : LOW);
    _peripheral_power_on = enabled;

    if (enabled) {
        // Give peripherals time to power up
        // T-Deck needs longer delay for display controller to stabilize
        delay(100);
    }
}
