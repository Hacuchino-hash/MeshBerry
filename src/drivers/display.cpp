/**
 * MeshBerry Display Driver Implementation
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
 * Uses TFT_eSPI library (MIT) for ST7789 display control
 */

#include "display.h"
#include <TFT_eSPI.h>

// TFT_eSPI instance (configured via build flags in platformio.ini)
static TFT_eSPI tft = TFT_eSPI();
static bool displayInitialized = false;
static uint8_t currentBrightness = 255;

namespace Display {

bool init() {
    if (displayInitialized) {
        return true;
    }

    Serial.println("[DISPLAY] Initializing ST7789...");

    // Initialize backlight pin
    pinMode(PIN_TFT_BL, OUTPUT);

    // Set up PWM for backlight dimming
    ledcSetup(TFT_BL_PWM_CHANNEL, TFT_BL_PWM_FREQ, TFT_BL_PWM_RES);
    ledcAttachPin(PIN_TFT_BL, TFT_BL_PWM_CHANNEL);

    // Turn on backlight
    setBacklight(255);

    // Initialize TFT
    tft.init();
    tft.setRotation(1);  // Landscape mode for T-Deck
    tft.fillScreen(TFT_BLACK);

    displayInitialized = true;
    Serial.printf("[DISPLAY] Initialized: %dx%d\n", tft.width(), tft.height());

    return true;
}

void setBacklight(uint8_t level) {
    currentBrightness = level;
    ledcWrite(TFT_BL_PWM_CHANNEL, level);
}

void backlightOn() {
    setBacklight(255);
}

void backlightOff() {
    setBacklight(0);
}

void clear(uint16_t color) {
    if (!displayInitialized) return;
    tft.fillScreen(color);
}

void drawText(int16_t x, int16_t y, const char* text, uint16_t color, uint8_t size) {
    if (!displayInitialized) return;

    tft.setTextColor(color);
    tft.setTextSize(size);
    tft.setCursor(x, y);
    tft.print(text);
}

void fillRect(int16_t x, int16_t y, int16_t w, int16_t h, uint16_t color) {
    if (!displayInitialized) return;
    tft.fillRect(x, y, w, h, color);
}

void drawRect(int16_t x, int16_t y, int16_t w, int16_t h, uint16_t color) {
    if (!displayInitialized) return;
    tft.drawRect(x, y, w, h, color);
}

uint16_t getWidth() {
    return displayInitialized ? tft.width() : TFT_HEIGHT;  // After rotation
}

uint16_t getHeight() {
    return displayInitialized ? tft.height() : TFT_WIDTH;  // After rotation
}

void setRotation(uint8_t rotation) {
    if (!displayInitialized) return;
    tft.setRotation(rotation);
}

} // namespace Display

// Provide access to TFT for advanced operations
TFT_eSPI& getTFT() {
    return tft;
}
