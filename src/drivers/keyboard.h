/**
 * MeshBerry Keyboard Driver
 *
 * Interface for T-Deck mini QWERTY keyboard via ESP32-C3 I2C
 *
 * Hardware: LILYGO T-Deck keyboard module
 * Controller: ESP32-C3 (I2C slave @ 0x55)
 * Interface: I2C
 *
 * Protocol:
 * - Read from register 0x00 to get next key code
 * - Write to register 0x01 to control backlight
 *
 * See: dev-docs/REQUIREMENTS.md for specifications
 */

#ifndef MESHBERRY_KEYBOARD_H
#define MESHBERRY_KEYBOARD_H

#include <Arduino.h>
#include "../config.h"

// =============================================================================
// KEY CODES
// =============================================================================

// Special keys (based on LilyGO keyboard firmware)
#define KEY_NONE        0x00
#define KEY_BACKSPACE   0x08
#define KEY_TAB         0x09
#define KEY_ENTER       0x0D
#define KEY_ESC         0x1B
#define KEY_SPACE       0x20
#define KEY_ALT         0x80
#define KEY_SYM         0x81
#define KEY_SHIFT       0x82
#define KEY_MIC         0x83
#define KEY_SPEAKER     0x84

// =============================================================================
// KEYBOARD DRIVER INTERFACE
// =============================================================================

namespace Keyboard {

/**
 * Initialize the keyboard hardware
 * @return true if keyboard controller is detected
 */
bool init();

/**
 * Check if a key is available
 * @return true if key data is waiting
 */
bool available();

/**
 * Read the next key code
 * @return Key code or KEY_NONE if no key
 */
uint8_t read();

/**
 * Get the character for a key code
 * Handles shift/alt modifiers
 * @param keyCode Raw key code
 * @return ASCII character or 0 for special keys
 */
char getChar(uint8_t keyCode);

/**
 * Check if shift modifier is active
 */
bool isShiftPressed();

/**
 * Check if alt/fn modifier is active
 */
bool isAltPressed();

/**
 * Set keyboard backlight
 * @param on true to turn on, false to turn off
 */
void setBacklight(bool on);

/**
 * Toggle keyboard backlight
 */
void toggleBacklight();

/**
 * Get current backlight state
 */
bool getBacklightState();

} // namespace Keyboard

#endif // MESHBERRY_KEYBOARD_H
