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

// =============================================================================
// ENHANCED DRAWING PRIMITIVES (BlackBerry Bold-style UI)
// =============================================================================

/**
 * Draw a filled rounded rectangle
 */
void fillRoundRect(int16_t x, int16_t y, int16_t w, int16_t h, int16_t radius, uint16_t color);

/**
 * Draw a rounded rectangle outline
 */
void drawRoundRect(int16_t x, int16_t y, int16_t w, int16_t h, int16_t radius, uint16_t color);

/**
 * Draw a horizontal line
 */
void drawHLine(int16_t x, int16_t y, int16_t w, uint16_t color);

/**
 * Draw a vertical line
 */
void drawVLine(int16_t x, int16_t y, int16_t h, uint16_t color);

/**
 * Draw a line between two points
 */
void drawLine(int16_t x0, int16_t y0, int16_t x1, int16_t y1, uint16_t color);

/**
 * Draw a filled circle
 */
void fillCircle(int16_t x, int16_t y, int16_t r, uint16_t color);

/**
 * Draw a circle outline
 */
void drawCircle(int16_t x, int16_t y, int16_t r, uint16_t color);

/**
 * Draw a filled triangle
 */
void fillTriangle(int16_t x0, int16_t y0, int16_t x1, int16_t y1, int16_t x2, int16_t y2, uint16_t color);

/**
 * Draw centered text within an area
 * @param x Left edge of area
 * @param y Top of text
 * @param w Width of area
 * @param text Text to center
 * @param color Text color
 * @param size Font size multiplier
 */
void drawTextCentered(int16_t x, int16_t y, int16_t w, const char* text, uint16_t color, uint8_t size = 1);

/**
 * Draw right-aligned text
 * @param x Right edge position
 * @param y Top of text
 * @param text Text to draw
 * @param color Text color
 * @param size Font size multiplier
 */
void drawTextRight(int16_t x, int16_t y, const char* text, uint16_t color, uint8_t size = 1);

/**
 * Draw a monochrome bitmap icon
 * @param x X position
 * @param y Y position
 * @param bitmap Pointer to bitmap data (1 bit per pixel, MSB first)
 * @param w Width in pixels
 * @param h Height in pixels
 * @param color Foreground color
 */
void drawBitmap(int16_t x, int16_t y, const uint8_t* bitmap, int16_t w, int16_t h, uint16_t color);

/**
 * Draw a monochrome bitmap with background
 */
void drawBitmapBg(int16_t x, int16_t y, const uint8_t* bitmap, int16_t w, int16_t h, uint16_t fgColor, uint16_t bgColor);

/**
 * Draw an RGB565 image from PROGMEM
 * @param x X position
 * @param y Y position
 * @param bitmap Pointer to RGB565 pixel data in PROGMEM
 * @param w Width in pixels
 * @param h Height in pixels
 */
void drawRGB565(int16_t x, int16_t y, const uint16_t* bitmap, int16_t w, int16_t h);

// =============================================================================
// EMOJI-AWARE TEXT RENDERING
// =============================================================================

/**
 * Draw text with embedded emoji support
 * Parses UTF-8 text and renders emoji as bitmaps inline with text.
 * Unknown Unicode characters are rendered as [?]
 *
 * @param x X position
 * @param y Y position
 * @param text UTF-8 encoded text (may contain emoji)
 * @param color Text color (emoji use their own colors)
 * @param size Font size multiplier (1 or 2)
 */
void drawTextWithEmoji(int16_t x, int16_t y, const char* text, uint16_t color, uint8_t size = 1);

/**
 * Draw centered text with emoji support
 * @param x Left edge of area
 * @param y Top of text
 * @param w Width of area
 * @param text UTF-8 text to center
 * @param color Text color
 * @param size Font size multiplier
 */
void drawTextWithEmojiCentered(int16_t x, int16_t y, int16_t w, const char* text, uint16_t color, uint8_t size = 1);

/**
 * Get underlying display pointer for advanced operations
 */
void* getDisplayPtr();

} // namespace Display

#endif // MESHBERRY_DISPLAY_H
