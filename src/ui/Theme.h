/**
 * MeshBerry UI Theme
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2026 NodakMesh (nodakmesh.org)
 *
 * BlackBerry Bold-inspired color scheme and UI constants
 */

#ifndef MESHBERRY_THEME_H
#define MESHBERRY_THEME_H

#include <Arduino.h>

namespace Theme {

// =============================================================================
// BLACKBERRY-INSPIRED COLOR PALETTE (RGB565)
// =============================================================================

// Primary colors
constexpr uint16_t BLACK        = 0x0000;  // Main background
constexpr uint16_t WHITE        = 0xFFFF;  // Primary text
constexpr uint16_t BLUE         = 0x2B9F;  // BlackBerry Blue accent (#1475DC)
constexpr uint16_t BLUE_LIGHT   = 0x5D7F;  // Lighter blue for highlights
constexpr uint16_t BLUE_DARK    = 0x1A5F;  // Darker blue for pressed states

// Grayscale
constexpr uint16_t GRAY_DARKEST = 0x1082;  // Card backgrounds, panels
constexpr uint16_t GRAY_DARK    = 0x2104;  // Secondary backgrounds
constexpr uint16_t GRAY_MID     = 0x4208;  // Borders, dividers
constexpr uint16_t GRAY_LIGHT   = 0x8410;  // Inactive text, disabled
constexpr uint16_t GRAY_LIGHTER = 0xC618;  // Subtle highlights

// Status colors
constexpr uint16_t GREEN        = 0x07E0;  // Online, success, connected
constexpr uint16_t GREEN_DARK   = 0x03E0;  // Darker green
constexpr uint16_t RED          = 0xF800;  // Error, offline, alert
constexpr uint16_t RED_DARK     = 0xA000;  // Darker red
constexpr uint16_t YELLOW       = 0xFE60;  // Warning, notification
constexpr uint16_t ORANGE       = 0xFC00;  // Caution

// Semantic aliases
constexpr uint16_t BG_PRIMARY   = BLACK;
constexpr uint16_t BG_SECONDARY = GRAY_DARKEST;
constexpr uint16_t BG_CARD      = GRAY_DARK;
constexpr uint16_t TEXT_PRIMARY = WHITE;
constexpr uint16_t TEXT_SECONDARY = GRAY_LIGHT;
constexpr uint16_t ACCENT       = BLUE;
constexpr uint16_t FOCUS        = BLUE_LIGHT;
constexpr uint16_t DIVIDER      = GRAY_MID;
constexpr uint16_t SUCCESS      = GREEN;
constexpr uint16_t ERROR        = RED;
constexpr uint16_t WARNING      = YELLOW;

// =============================================================================
// LAYOUT CONSTANTS
// =============================================================================

// Screen dimensions (landscape orientation)
constexpr int16_t SCREEN_WIDTH  = 320;
constexpr int16_t SCREEN_HEIGHT = 240;

// Status bar (top)
constexpr int16_t STATUS_BAR_HEIGHT = 20;
constexpr int16_t STATUS_BAR_Y      = 0;

// Content area
constexpr int16_t CONTENT_Y      = STATUS_BAR_HEIGHT;
constexpr int16_t CONTENT_HEIGHT = SCREEN_HEIGHT - STATUS_BAR_HEIGHT - 30; // 190px

// Soft key bar (bottom)
constexpr int16_t SOFTKEY_BAR_HEIGHT = 30;
constexpr int16_t SOFTKEY_BAR_Y      = SCREEN_HEIGHT - SOFTKEY_BAR_HEIGHT;

// Margins and padding
constexpr int16_t MARGIN_SMALL  = 4;
constexpr int16_t MARGIN_MEDIUM = 8;
constexpr int16_t MARGIN_LARGE  = 12;
constexpr int16_t PADDING       = 8;

// List item dimensions
constexpr int16_t LIST_ITEM_HEIGHT      = 44;  // Touch-friendly minimum
constexpr int16_t LIST_ITEM_PADDING     = 8;
constexpr int16_t LIST_ICON_SIZE        = 24;
constexpr int16_t LIST_DIVIDER_HEIGHT   = 1;

// Home screen grid
constexpr int16_t HOME_GRID_COLS    = 4;
constexpr int16_t HOME_GRID_ROWS    = 2;
constexpr int16_t HOME_TILE_WIDTH   = 76;
constexpr int16_t HOME_TILE_HEIGHT  = 85;
constexpr int16_t HOME_TILE_MARGIN  = 4;
constexpr int16_t HOME_ICON_SIZE    = 32;

// Dialog dimensions
constexpr int16_t DIALOG_WIDTH      = 280;
constexpr int16_t DIALOG_MIN_HEIGHT = 120;
constexpr int16_t DIALOG_PADDING    = 16;
constexpr int16_t DIALOG_RADIUS     = 8;

// Button dimensions
constexpr int16_t BUTTON_HEIGHT     = 32;
constexpr int16_t BUTTON_MIN_WIDTH  = 80;
constexpr int16_t BUTTON_RADIUS     = 4;

// Text input
constexpr int16_t INPUT_HEIGHT      = 28;
constexpr int16_t INPUT_RADIUS      = 4;

// Corner radius for rounded elements
constexpr int16_t RADIUS_SMALL      = 2;
constexpr int16_t RADIUS_MEDIUM     = 4;
constexpr int16_t RADIUS_LARGE      = 8;

// =============================================================================
// FONT SIZES
// =============================================================================

constexpr uint8_t FONT_SMALL   = 1;  // 6x8 pixels
constexpr uint8_t FONT_MEDIUM  = 1;  // We'll use size 1 mostly (6x8)
constexpr uint8_t FONT_LARGE   = 2;  // 12x16 pixels

// Character dimensions (Adafruit GFX default font)
constexpr int16_t CHAR_WIDTH_S  = 6;
constexpr int16_t CHAR_HEIGHT_S = 8;
constexpr int16_t CHAR_WIDTH_L  = 12;
constexpr int16_t CHAR_HEIGHT_L = 16;

// =============================================================================
// ICON INDICES (for icon lookup)
// =============================================================================

enum IconId {
    ICON_NONE = 0,
    ICON_MESSAGE,
    ICON_MESSAGE_UNREAD,
    ICON_CONTACTS,
    ICON_SETTINGS,
    ICON_INFO,
    ICON_CHANNEL,
    ICON_REPEATER,
    ICON_GPS,
    ICON_BATTERY_FULL,
    ICON_BATTERY_MID,
    ICON_BATTERY_LOW,
    ICON_BATTERY_EMPTY,
    ICON_SIGNAL_FULL,
    ICON_SIGNAL_MID,
    ICON_SIGNAL_LOW,
    ICON_SIGNAL_NONE,
    ICON_LORA,
    ICON_LORA_TX,
    ICON_LORA_RX,
    ICON_BACK,
    ICON_MENU,
    ICON_CHECK,
    ICON_CANCEL,
    ICON_EDIT,
    ICON_DELETE,
    ICON_STAR,
    ICON_STAR_FILLED,
    ICON_LOCK,
    ICON_UNLOCK,
    ICON_ARROW_UP,
    ICON_ARROW_DOWN,
    ICON_ARROW_LEFT,
    ICON_ARROW_RIGHT,
    ICON_COUNT
};

// =============================================================================
// ANIMATION / TIMING
// =============================================================================

constexpr uint32_t FOCUS_BLINK_MS       = 500;   // Cursor blink rate
constexpr uint32_t STATUS_UPDATE_MS     = 1000;  // Status bar refresh
constexpr uint32_t NOTIFICATION_MS      = 3000;  // Toast duration
constexpr uint32_t DEBOUNCE_MS          = 50;    // Input debounce

// =============================================================================
// UTILITY FUNCTIONS
// =============================================================================

/**
 * Convert RGB888 to RGB565
 */
constexpr uint16_t rgb565(uint8_t r, uint8_t g, uint8_t b) {
    return ((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3);
}

/**
 * Blend two colors (simple 50/50 blend)
 */
uint16_t blendColors(uint16_t c1, uint16_t c2);

/**
 * Darken a color by a factor (0-255, 0=black, 255=original)
 */
uint16_t darkenColor(uint16_t color, uint8_t factor);

/**
 * Lighten a color by a factor (0-255, 0=original, 255=white)
 */
uint16_t lightenColor(uint16_t color, uint8_t factor);

/**
 * Get text width in pixels
 */
int16_t getTextWidth(const char* text, uint8_t size = FONT_MEDIUM);

/**
 * Get centered X position for text
 */
int16_t getCenteredX(const char* text, int16_t areaWidth, uint8_t size = FONT_MEDIUM);

/**
 * Truncate text to fit width, adding "..." if needed
 * Returns pointer to static buffer
 */
const char* truncateText(const char* text, int16_t maxWidth, uint8_t size = FONT_MEDIUM);

/**
 * Word-wrap text into lines that fit within maxWidth
 * Uses emoji-aware width calculation for accurate wrapping
 * @param text Input text to wrap
 * @param maxWidth Maximum pixel width per line
 * @param size Font size (1 or 2)
 * @param lines Output array of line buffers (each 64 chars)
 * @param maxLines Maximum number of lines to output
 * @return Number of lines produced
 */
int wrapText(const char* text, int16_t maxWidth, uint8_t size,
             char lines[][64], int maxLines);

} // namespace Theme

#endif // MESHBERRY_THEME_H
