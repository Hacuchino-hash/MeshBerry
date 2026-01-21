/**
 * MeshBerry Touch Screen Driver Implementation
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2026 NodakMesh (nodakmesh.org)
 *
 * This file is part of MeshBerry.
 *
 * GT911 capacitive touch controller driver for LILYGO T-Deck.
 * The GT911 is a 5-point capacitive touch controller.
 */

#include "touch.h"
#include "../config.h"
#include <Wire.h>

// GT911 Register addresses
#define GT911_PRODUCT_ID_REG    0x8140  // Product ID (4 bytes: "911\0")
#define GT911_COORD_REG         0x814E  // Touch status register
#define GT911_POINT1_REG        0x814F  // First touch point data (6 bytes)
#define GT911_CONFIG_REG        0x8047  // Configuration data start

// Internal state
static bool initialized = false;
static bool touchDetected = false;
static volatile bool intTriggered = false;
static int16_t lastX = 0;
static int16_t lastY = 0;
static uint8_t touchCount = 0;
static uint8_t gt911Addr = TOUCH_I2C_ADDR;  // Working I2C address (0x5D or 0x14)

// Interrupt handler
static void IRAM_ATTR touchISR() {
    intTriggered = true;
}

// Write to GT911 register (16-bit address) using dynamic address
static bool gt911WriteReg(uint16_t reg, uint8_t value) {
    Wire.beginTransmission(gt911Addr);
    Wire.write(reg >> 8);       // High byte of register address
    Wire.write(reg & 0xFF);     // Low byte of register address
    Wire.write(value);
    return Wire.endTransmission() == 0;
}

// Read from GT911 register with specific I2C address (for init probing)
static bool gt911ReadRegAddr(uint8_t addr, uint16_t reg, uint8_t* data, uint8_t len) {
    Wire.beginTransmission(addr);
    Wire.write(reg >> 8);
    Wire.write(reg & 0xFF);
    if (Wire.endTransmission(false) != 0) {
        return false;
    }

    Wire.requestFrom(addr, len);
    for (uint8_t i = 0; i < len && Wire.available(); i++) {
        data[i] = Wire.read();
    }
    return true;
}

// Read from GT911 register using stored address
static bool gt911ReadReg(uint16_t reg, uint8_t* data, uint8_t len) {
    return gt911ReadRegAddr(gt911Addr, reg, data, len);
}

namespace Touch {

bool init() {
    if (initialized) return true;

    Serial.println("[TOUCH] Initializing GT911...");

    // First, scan I2C to see what's actually present
    Serial.println("[TOUCH] Scanning I2C for touch controller...");
    for (uint8_t addr = 0x10; addr < 0x80; addr++) {
        Wire.beginTransmission(addr);
        if (Wire.endTransmission() == 0) {
            Serial.printf("[TOUCH] Found I2C device at 0x%02X\n", addr);
        }
    }

    // GT911 requires a specific init sequence to set I2C address:
    // 1. INT pin LOW during power-up/reset sets address
    // 2. INT HIGH = address 0x5D, INT LOW = address 0x14
    // Since we can't control reset, we toggle INT to try to reset the address

    // Step 1: Configure INT as OUTPUT and pull LOW
    pinMode(PIN_TOUCH_INT, OUTPUT);
    digitalWrite(PIN_TOUCH_INT, LOW);
    delay(20);

    // Step 2: Set INT HIGH to select address 0x5D
    digitalWrite(PIN_TOUCH_INT, HIGH);
    delay(10);

    // Step 3: Switch INT back to INPUT for interrupt detection
    pinMode(PIN_TOUCH_INT, INPUT);
    delay(100);  // Wait for GT911 to stabilize

    // Re-scan after address selection sequence
    Serial.println("[TOUCH] Scanning I2C after INT toggle...");
    for (uint8_t addr = 0x10; addr < 0x80; addr++) {
        Wire.beginTransmission(addr);
        if (Wire.endTransmission() == 0) {
            Serial.printf("[TOUCH] Found I2C device at 0x%02X\n", addr);
        }
    }

    // Step 4: Try to read product ID at primary address (0x5D)
    uint8_t productId[4] = {0};
    gt911Addr = TOUCH_I2C_ADDR;  // 0x5D

    Serial.printf("[TOUCH] Trying to read product ID at 0x%02X...\n", gt911Addr);
    if (!gt911ReadRegAddr(gt911Addr, GT911_PRODUCT_ID_REG, productId, 4)) {
        // Try alternate address (0x14)
        Serial.println("[TOUCH] 0x5D failed, trying 0x14...");
        gt911Addr = 0x14;

        if (!gt911ReadRegAddr(gt911Addr, GT911_PRODUCT_ID_REG, productId, 4)) {
            Serial.println("[TOUCH] GT911 not detected at 0x5D or 0x14");
            Serial.println("[TOUCH] Note: T-Deck may not have touch panel installed");
            return false;
        }
    }

    Serial.printf("[TOUCH] Found GT911 at 0x%02X\n", gt911Addr);

    // Check product ID (should be "911" or "9147")
    Serial.printf("[TOUCH] Product ID: %c%c%c%c\n",
                  productId[0], productId[1], productId[2], productId[3]);

    if (productId[0] != '9' || productId[1] != '1' || productId[2] != '1') {
        // Also accept "9147" which is another GT911 variant
        if (!(productId[0] == '9' && productId[1] == '1' && productId[2] == '4')) {
            Serial.println("[TOUCH] Unexpected product ID");
            return false;
        }
    }

    // Attach interrupt for touch events (falling edge when touch starts)
    attachInterrupt(digitalPinToInterrupt(PIN_TOUCH_INT), touchISR, FALLING);

    initialized = true;
    Serial.println("[TOUCH] GT911 initialized successfully");
    return true;
}

bool available() {
    if (!initialized) return false;

    // Check interrupt flag
    if (intTriggered) {
        intTriggered = false;
        return true;
    }

    // Also poll the touch status register periodically
    // Some GT911 variants don't pulse the interrupt properly
    uint8_t status = 0;
    if (gt911ReadReg(GT911_COORD_REG, &status, 1)) {
        if (status & 0x80) {  // Buffer status bit
            return true;
        }
    }

    return false;
}

bool read(int16_t* x, int16_t* y) {
    if (!initialized) return false;

    // Time-based debounce to prevent false "release" events.
    // The GT911 status register gets cleared after each read, which can cause
    // the touch state to flicker between true/false rapidly. This triggers
    // multiple "release" events from the state machine. We require 80ms of
    // no valid touch data before reporting a release.
    static uint32_t lastValidTouchTime = 0;
    const uint32_t TOUCH_DEBOUNCE_MS = 80;

    // Read touch status
    uint8_t status = 0;
    if (!gt911ReadReg(GT911_COORD_REG, &status, 1)) {
        // I2C error - maintain previous state if within debounce window
        if (millis() - lastValidTouchTime < TOUCH_DEBOUNCE_MS) {
            if (x) *x = lastX;
            if (y) *y = lastY;
            return true;
        }
        return false;
    }

    // Check if data is valid (bit 7) and how many touches (bits 0-3)
    bool hasValidTouch = (status & 0x80) && ((status & 0x0F) > 0) && ((status & 0x0F) <= 5);

    if (hasValidTouch) {
        touchCount = status & 0x0F;

        // Read first touch point (6 bytes: track_id, x_low, x_high, y_low, y_high, size)
        uint8_t pointData[6];
        if (!gt911ReadReg(GT911_POINT1_REG, pointData, 6)) {
            gt911WriteReg(GT911_COORD_REG, 0);
            return false;
        }

        // Parse coordinates (little endian)
        uint16_t rawX = pointData[1] | (pointData[2] << 8);
        uint16_t rawY = pointData[3] | (pointData[4] << 8);

        // For T-Deck landscape mode (320 wide, 240 tall):
        // GT911 reports in portrait orientation (240x320), we need landscape (320x240)
        // Transform: swap X/Y and invert Y to match display coordinates
        lastX = rawY;                    // Portrait Y becomes landscape X
        lastY = TFT_WIDTH - 1 - rawX;    // Portrait X becomes landscape Y (inverted)

        // Clamp to screen dimensions
        if (lastX < 0) lastX = 0;
        if (lastX >= TFT_HEIGHT) lastX = TFT_HEIGHT - 1;  // 320
        if (lastY < 0) lastY = 0;
        if (lastY >= TFT_WIDTH) lastY = TFT_WIDTH - 1;    // 240

        if (x) *x = lastX;
        if (y) *y = lastY;

        touchDetected = true;
        lastValidTouchTime = millis();

        // Clear the status register to acknowledge
        gt911WriteReg(GT911_COORD_REG, 0);

        return true;
    }

    // No valid touch - clear register
    gt911WriteReg(GT911_COORD_REG, 0);

    // Debounce: don't report release until debounce time passed
    if (millis() - lastValidTouchTime < TOUCH_DEBOUNCE_MS) {
        // Still within debounce window, pretend we're still touching
        if (x) *x = lastX;
        if (y) *y = lastY;
        return true;
    }

    touchDetected = false;
    touchCount = 0;
    return false;
}

bool isTouched() {
    return touchDetected;
}

uint8_t getTouchCount() {
    return touchCount;
}

} // namespace Touch
