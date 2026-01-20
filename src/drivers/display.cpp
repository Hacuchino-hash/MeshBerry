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
 * Uses Adafruit ST7789 library (same as MeshCore) for display control
 */

#include "display.h"
#include <SPI.h>
#include <Adafruit_GFX.h>
#include <Adafruit_ST7789.h>

// T-Deck pins (from LilyGo official)
#define BOARD_POWERON   10
#define BOARD_TFT_CS    12
#define BOARD_TFT_DC    11
#define BOARD_TFT_SCLK  40
#define BOARD_TFT_MOSI  41
#define BOARD_TFT_BL    42

// Display dimensions
#define DISPLAY_WIDTH  240
#define DISPLAY_HEIGHT 320

// Dynamically allocated to avoid global constructor issues
static SPIClass* displaySPI = nullptr;
static Adafruit_ST7789* display = nullptr;
static bool displayInitialized = false;
static uint8_t currentBrightness = 255;

namespace Display {

bool init() {
    if (displayInitialized) {
        return true;
    }

    Serial.println("[DISPLAY] Initializing ST7789...");

    // Step 1: Ensure peripheral power is ON
    pinMode(BOARD_POWERON, OUTPUT);
    digitalWrite(BOARD_POWERON, HIGH);
    delay(100);
    Serial.println("[DISPLAY] Peripheral power ON");

    // Step 2: Turn on backlight IMMEDIATELY
    pinMode(BOARD_TFT_BL, OUTPUT);
    digitalWrite(BOARD_TFT_BL, HIGH);
    Serial.println("[DISPLAY] Backlight ON");

    // Step 3: Initialize HSPI
    displaySPI = new SPIClass(HSPI);
    displaySPI->begin(BOARD_TFT_SCLK, -1, BOARD_TFT_MOSI, BOARD_TFT_CS);
    Serial.println("[DISPLAY] HSPI initialized");

    // Step 4: Create and initialize display
    display = new Adafruit_ST7789(displaySPI, BOARD_TFT_CS, BOARD_TFT_DC, -1);
    display->init(DISPLAY_WIDTH, DISPLAY_HEIGHT);
    Serial.println("[DISPLAY] ST7789 init complete");

    // Step 5: Configure display
    display->setRotation(3);  // Landscape
    display->setSPISpeed(40000000);

    // Step 6: Clear display (skip color test for faster boot)
    display->fillScreen(ST77XX_BLACK);

    // Step 7: Configure text
    display->setTextColor(ST77XX_WHITE);
    display->setTextSize(2);
    display->cp437(true);

    // Step 8: Set up PWM for backlight dimming
    ledcSetup(TFT_BL_PWM_CHANNEL, TFT_BL_PWM_FREQ, TFT_BL_PWM_RES);
    ledcAttachPin(BOARD_TFT_BL, TFT_BL_PWM_CHANNEL);
    setBacklight(255);

    displayInitialized = true;
    Serial.printf("[DISPLAY] Initialized: %dx%d\n", display->width(), display->height());

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
    if (!displayInitialized || !display) return;
    display->fillScreen(color);
}

void drawText(int16_t x, int16_t y, const char* text, uint16_t color, uint8_t size) {
    if (!displayInitialized || !display) return;
    display->setTextColor(color);
    display->setTextSize(size);
    display->setCursor(x, y);
    display->print(text);
}

void fillRect(int16_t x, int16_t y, int16_t w, int16_t h, uint16_t color) {
    if (!displayInitialized || !display) return;
    display->fillRect(x, y, w, h, color);
}

void drawRect(int16_t x, int16_t y, int16_t w, int16_t h, uint16_t color) {
    if (!displayInitialized || !display) return;
    display->drawRect(x, y, w, h, color);
}

uint16_t getWidth() {
    return (displayInitialized && display) ? display->width() : DISPLAY_HEIGHT;
}

uint16_t getHeight() {
    return (displayInitialized && display) ? display->height() : DISPLAY_WIDTH;
}

void setRotation(uint8_t rotation) {
    if (!displayInitialized || !display) return;
    display->setRotation(rotation);
}

// =============================================================================
// ENHANCED DRAWING PRIMITIVES
// =============================================================================

void fillRoundRect(int16_t x, int16_t y, int16_t w, int16_t h, int16_t radius, uint16_t color) {
    if (!displayInitialized || !display) return;
    display->fillRoundRect(x, y, w, h, radius, color);
}

void drawRoundRect(int16_t x, int16_t y, int16_t w, int16_t h, int16_t radius, uint16_t color) {
    if (!displayInitialized || !display) return;
    display->drawRoundRect(x, y, w, h, radius, color);
}

void drawHLine(int16_t x, int16_t y, int16_t w, uint16_t color) {
    if (!displayInitialized || !display) return;
    display->drawFastHLine(x, y, w, color);
}

void drawVLine(int16_t x, int16_t y, int16_t h, uint16_t color) {
    if (!displayInitialized || !display) return;
    display->drawFastVLine(x, y, h, color);
}

void drawLine(int16_t x0, int16_t y0, int16_t x1, int16_t y1, uint16_t color) {
    if (!displayInitialized || !display) return;
    display->drawLine(x0, y0, x1, y1, color);
}

void fillCircle(int16_t x, int16_t y, int16_t r, uint16_t color) {
    if (!displayInitialized || !display) return;
    display->fillCircle(x, y, r, color);
}

void drawCircle(int16_t x, int16_t y, int16_t r, uint16_t color) {
    if (!displayInitialized || !display) return;
    display->drawCircle(x, y, r, color);
}

void fillTriangle(int16_t x0, int16_t y0, int16_t x1, int16_t y1, int16_t x2, int16_t y2, uint16_t color) {
    if (!displayInitialized || !display) return;
    display->fillTriangle(x0, y0, x1, y1, x2, y2, color);
}

void drawTextCentered(int16_t x, int16_t y, int16_t w, const char* text, uint16_t color, uint8_t size) {
    if (!displayInitialized || !display || !text) return;

    // Calculate text width (6 pixels per char at size 1)
    int16_t charWidth = 6 * size;
    int16_t textWidth = strlen(text) * charWidth;
    int16_t centeredX = x + (w - textWidth) / 2;

    display->setTextColor(color);
    display->setTextSize(size);
    display->setCursor(centeredX, y);
    display->print(text);
}

void drawTextRight(int16_t x, int16_t y, const char* text, uint16_t color, uint8_t size) {
    if (!displayInitialized || !display || !text) return;

    // Calculate text width
    int16_t charWidth = 6 * size;
    int16_t textWidth = strlen(text) * charWidth;
    int16_t rightX = x - textWidth;

    display->setTextColor(color);
    display->setTextSize(size);
    display->setCursor(rightX, y);
    display->print(text);
}

void drawBitmap(int16_t x, int16_t y, const uint8_t* bitmap, int16_t w, int16_t h, uint16_t color) {
    if (!displayInitialized || !display || !bitmap) return;
    display->drawBitmap(x, y, bitmap, w, h, color);
}

void drawBitmapBg(int16_t x, int16_t y, const uint8_t* bitmap, int16_t w, int16_t h, uint16_t fgColor, uint16_t bgColor) {
    if (!displayInitialized || !display || !bitmap) return;
    display->drawBitmap(x, y, bitmap, w, h, fgColor, bgColor);
}

void drawRGB565(int16_t x, int16_t y, const uint16_t* bitmap, int16_t w, int16_t h) {
    if (!displayInitialized || !display || !bitmap) return;
    display->drawRGBBitmap(x, y, bitmap, w, h);
}

void* getDisplayPtr() {
    return display;
}

} // namespace Display
