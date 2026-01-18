/**
 * MeshBerry Display Driver
 *
 * Interface for ST7789 2.8" IPS LCD (320x240)
 *
 * Hardware: LILYGO T-Deck
 * Controller: ST7789
 * Interface: SPI
 *
 * See: dev-docs/REQUIREMENTS.md for specifications
 */

#ifndef MESHBERRY_DISPLAY_H
#define MESHBERRY_DISPLAY_H

#include <Arduino.h>
#include "../config.h"

// =============================================================================
// DISPLAY DRIVER INTERFACE
// =============================================================================

namespace Display {

/**
 * Initialize the display hardware
 * @return true if successful
 */
bool init();

/**
 * Set display backlight level
 * @param level 0-255 (0 = off, 255 = full brightness)
 */
void setBacklight(uint8_t level);

/**
 * Turn backlight on/off
 */
void backlightOn();
void backlightOff();

/**
 * Clear the entire screen
 * @param color RGB565 color value
 */
void clear(uint16_t color = 0x0000);

/**
 * Draw text at position
 * @param x X coordinate
 * @param y Y coordinate
 * @param text Text to display
 * @param color Text color (RGB565)
 * @param size Font size multiplier
 */
void drawText(int16_t x, int16_t y, const char* text, uint16_t color, uint8_t size = 1);

/**
 * Draw a filled rectangle
 */
void fillRect(int16_t x, int16_t y, int16_t w, int16_t h, uint16_t color);

/**
 * Draw a rectangle outline
 */
void drawRect(int16_t x, int16_t y, int16_t w, int16_t h, uint16_t color);

/**
 * Get display dimensions
 */
uint16_t getWidth();
uint16_t getHeight();

/**
 * Set display rotation
 * @param rotation 0-3 (0 = normal, 1 = 90°, 2 = 180°, 3 = 270°)
 */
void setRotation(uint8_t rotation);

} // namespace Display

#endif // MESHBERRY_DISPLAY_H
