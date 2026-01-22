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

// =============================================================================
// MODERN UI HELPER FUNCTIONS
// =============================================================================

/**
 * Button rendering states
 */
enum ButtonState {
    BTN_NORMAL = 0,
    BTN_HOVER,
    BTN_PRESSED,
    BTN_DISABLED
};

/**
 * Draw a card with optional shadow (modern card-based UI)
 * @param x X position
 * @param y Y position
 * @param w Width
 * @param h Height
 * @param bgColor Background color (default: Theme::COLOR_BG_CARD)
 * @param radius Corner radius (default: Theme::CARD_RADIUS)
 * @param shadow Draw shadow underneath (default: false)
 */
void drawCard(int16_t x, int16_t y, int16_t w, int16_t h,
              uint16_t bgColor, int16_t radius = 12, bool shadow = false);

/**
 * Draw a vertical gradient fill (dithered for RGB565)
 * @param x X position
 * @param y Y position
 * @param w Width
 * @param h Height
 * @param colorTop Top color
 * @param colorBottom Bottom color
 */
void fillGradient(int16_t x, int16_t y, int16_t w, int16_t h,
                  uint16_t colorTop, uint16_t colorBottom);

/**
 * Draw a drop shadow (soft blur simulation)
 * @param x X position of element
 * @param y Y position of element
 * @param w Width of element
 * @param h Height of element
 * @param offset Shadow offset in pixels (default: 4)
 * @param opacity Shadow opacity 0-100 (default: 40)
 */
void drawShadow(int16_t x, int16_t y, int16_t w, int16_t h,
                int16_t offset = 4, uint8_t opacity = 40);

/**
 * Draw a styled button with state-based appearance
 * @param x X position
 * @param y Y position
 * @param w Width
 * @param h Height
 * @param label Button label text
 * @param state Button state (normal, hover, pressed, disabled)
 * @param isPrimary Primary button style (filled) vs secondary (outline)
 */
void drawButton(int16_t x, int16_t y, int16_t w, int16_t h,
                const char* label, ButtonState state = BTN_NORMAL,
                bool isPrimary = false);

/**
 * Draw a toggle switch (ON/OFF indicator)
 * @param x X position (left edge)
 * @param y Y position (top edge)
 * @param isOn Toggle state (on = true, off = false)
 * @param isEnabled Enabled state (default: true)
 */
void drawToggle(int16_t x, int16_t y, bool isOn, bool isEnabled = true);

/**
 * Draw a progress bar
 * @param x X position
 * @param y Y position
 * @param w Width
 * @param h Height
 * @param progress Progress value 0.0-1.0
 * @param color Progress bar color (default: Theme::COLOR_PRIMARY)
 */
void drawProgressBar(int16_t x, int16_t y, int16_t w, int16_t h,
                     float progress, uint16_t color);

/**
 * Draw a notification badge (circular count indicator)
 * @param x X position (center)
 * @param y Y position (center)
 * @param count Number to display (1-99, 99+ shows "99+")
 * @param bgColor Badge background color (default: Theme::COLOR_ERROR)
 */
void drawBadge(int16_t x, int16_t y, int count, uint16_t bgColor);

/**
 * Draw an icon button (icon centered in circular background)
 * @param x X position (center)
 * @param y Y position (center)
 * @param radius Circle radius
 * @param icon Pointer to icon bitmap (16x16)
 * @param bgColor Background color
 * @param iconColor Icon foreground color
 */
void drawIconButton(int16_t x, int16_t y, int16_t radius,
                    const uint8_t* icon, uint16_t bgColor, uint16_t iconColor);

} // namespace Display

#endif // MESHBERRY_DISPLAY_H
