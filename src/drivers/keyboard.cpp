/**
 * MeshBerry Keyboard Driver Implementation
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
 * T-Deck keyboard interface via ESP32-C3 I2C slave
 */

#include "keyboard.h"
#include <Wire.h>

// Keyboard state
static bool keyboardInitialized = false;
static bool backlightOn = false;
static bool shiftPressed = false;
static bool altPressed = false;

namespace Keyboard {

bool init() {
    if (keyboardInitialized) {
        return true;
    }

    Serial.println("[KEYBOARD] Initializing...");

    // I2C should already be initialized in main
    // Check if keyboard controller responds
    Wire.beginTransmission(KB_I2C_ADDR);
    uint8_t error = Wire.endTransmission();

    if (error == 0) {
        keyboardInitialized = true;
        Serial.println("[KEYBOARD] Controller detected at 0x55");

        // Turn on backlight by default
        setBacklight(true);
        return true;
    } else {
        Serial.printf("[KEYBOARD] Controller not found (error: %d)\n", error);
        return false;
    }
}

bool available() {
    // T-Deck keyboard: just try to read - returns 0 if no key
    // We always return true and let read() handle the actual check
    return keyboardInitialized;
}

uint8_t read() {
    if (!keyboardInitialized) return KEY_NONE;

    // T-Deck keyboard: simple direct read (no register select needed)
    Wire.requestFrom((uint8_t)KB_I2C_ADDR, (uint8_t)1);
    if (Wire.available()) {
        uint8_t key = Wire.read();

        // Return 0 (KEY_NONE) if no key pressed
        if (key == KEY_NONE) return KEY_NONE;

        // Track modifier state
        if (key == KEY_SHIFT) {
            shiftPressed = !shiftPressed;
            return KEY_SHIFT;
        }
        if (key == KEY_ALT) {
            altPressed = !altPressed;
            return KEY_ALT;
        }

        return key;
    }

    return KEY_NONE;
}

char getChar(uint8_t keyCode) {
    // Handle special keys
    if (keyCode == KEY_NONE) return 0;
    if (keyCode == KEY_BACKSPACE) return '\b';
    if (keyCode == KEY_ENTER) return '\n';
    if (keyCode == KEY_TAB) return '\t';
    if (keyCode == KEY_ESC) return 0x1B;
    if (keyCode == KEY_SPACE) return ' ';

    // Modifier keys return 0
    if (keyCode >= 0x80) return 0;

    // Handle printable characters
    char c = (char)keyCode;

    // Apply shift for uppercase
    if (shiftPressed && c >= 'a' && c <= 'z') {
        c = c - 'a' + 'A';
    }

    // Apply shift for symbols
    if (shiftPressed) {
        switch (c) {
            case '1': c = '!'; break;
            case '2': c = '@'; break;
            case '3': c = '#'; break;
            case '4': c = '$'; break;
            case '5': c = '%'; break;
            case '6': c = '^'; break;
            case '7': c = '&'; break;
            case '8': c = '*'; break;
            case '9': c = '('; break;
            case '0': c = ')'; break;
            case '-': c = '_'; break;
            case '=': c = '+'; break;
            case '[': c = '{'; break;
            case ']': c = '}'; break;
            case '\\': c = '|'; break;
            case ';': c = ':'; break;
            case '\'': c = '"'; break;
            case ',': c = '<'; break;
            case '.': c = '>'; break;
            case '/': c = '?'; break;
            case '`': c = '~'; break;
        }
    }

    // Reset modifiers after character processed
    shiftPressed = false;
    altPressed = false;

    return c;
}

bool isShiftPressed() {
    return shiftPressed;
}

bool isAltPressed() {
    return altPressed;
}

void setBacklight(bool on) {
    if (!keyboardInitialized) return;

    backlightOn = on;

    // Write to backlight control register
    Wire.beginTransmission(KB_I2C_ADDR);
    Wire.write(KB_REG_BL);
    Wire.write(on ? 0x01 : 0x00);
    Wire.endTransmission();
}

void toggleBacklight() {
    setBacklight(!backlightOn);
}

bool getBacklightState() {
    return backlightOn;
}

} // namespace Keyboard
