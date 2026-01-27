/**
 * MeshBerry UI Theme
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2026 NodakMesh (nodakmesh.org)
 *
 * Modern, Apple-inspired color scheme and UI constants
 */

#ifndef MESHBERRY_THEME_H
#define MESHBERRY_THEME_H

#include <Arduino.h>

namespace Theme {

// =============================================================================
// MESHBERRY COLOR PALETTE (RGB565)
// =============================================================================

// Background colors - near-black for high contrast
constexpr uint16_t BG_DARK      = 0x0841;  // #080808 - Near black (primary)
constexpr uint16_t BG_ELEVATED  = 0x1082;  // #101010 - Slightly elevated (cards, tiles)
constexpr uint16_t BG_TERTIARY  = 0x18E3;  // #1C1C1E - Tertiary (incoming bubbles)

// Legacy aliases (for compatibility)
constexpr uint16_t BLACK        = BG_DARK;      // Near black
constexpr uint16_t WHITE        = 0xFFFF;       // Primary text

// Accent colors - BlackBerry Orient Blue
constexpr uint16_t ACCENT_PRIMARY   = 0x0290;   // #005387 - BB Orient Blue
constexpr uint16_t ACCENT_LIGHT     = 0x3B5F;   // #3A7BB0 - Light blue
constexpr uint16_t ACCENT_PRESSED   = 0x0188;   // #003050 - Dark blue (pressed)

// Legacy blue aliases (for compatibility)
constexpr uint16_t BLUE         = ACCENT_PRIMARY;
constexpr uint16_t BLUE_LIGHT   = ACCENT_LIGHT;
constexpr uint16_t BLUE_DARK    = ACCENT_PRESSED;

// Grayscale - refined for high contrast
constexpr uint16_t GRAY_DARKEST = BG_ELEVATED;  // Card backgrounds
constexpr uint16_t GRAY_DARK    = BG_TERTIARY;  // Secondary backgrounds
constexpr uint16_t GRAY_MID     = 0x4A49;       // #4A4A4C - Borders, dividers
constexpr uint16_t GRAY_LIGHT   = 0xB5B6;       // #B0B0B0 - Secondary text (brighter)
constexpr uint16_t GRAY_LIGHTER = 0xDEDB;       // #DDDDDD - Subtle highlights

// Status colors - softer, more refined
constexpr uint16_t GREEN        = 0x2DC9;       // #2ECC71 - Soft green (success)
constexpr uint16_t GREEN_DARK   = 0x24A3;       // #27AE60 - Darker green
constexpr uint16_t RED          = 0xFA12;       // #FF5252 - Soft red (error)
constexpr uint16_t RED_DARK     = 0xC800;       // #C62828 - Darker red
constexpr uint16_t YELLOW       = 0xFD20;       // #F5A623 - Amber (warning)
constexpr uint16_t ORANGE       = 0xFC60;       // #FF9800 - Orange (caution)

// Semantic aliases
constexpr uint16_t BG_PRIMARY   = BG_DARK;
constexpr uint16_t BG_SECONDARY = BG_ELEVATED;
constexpr uint16_t BG_CARD      = BG_ELEVATED;
constexpr uint16_t TEXT_PRIMARY = WHITE;
constexpr uint16_t TEXT_SECONDARY = GRAY_LIGHT;
constexpr uint16_t TEXT_DISABLED = 0x7BCF;      // #7A7A7A - Disabled (brighter)
constexpr uint16_t ACCENT       = ACCENT_PRIMARY;
constexpr uint16_t FOCUS        = ACCENT_LIGHT;
constexpr uint16_t DIVIDER      = 0x2104;       // #202020 - Subtle divider
constexpr uint16_t SUCCESS      = GREEN;
constexpr uint16_t ERROR        = RED;
constexpr uint16_t WARNING      = YELLOW;

// Chat bubble colors
constexpr uint16_t BUBBLE_OUTGOING = ACCENT_PRIMARY;  // BB Blue for sent messages
constexpr uint16_t BUBBLE_INCOMING = BG_TERTIARY;     // Gray for received messages

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

// Home screen grid - larger tiles with more breathing room
constexpr int16_t HOME_GRID_COLS    = 3;
constexpr int16_t HOME_GRID_ROWS    = 2;
constexpr int16_t HOME_TILE_WIDTH   = 96;   // Larger tiles
constexpr int16_t HOME_TILE_HEIGHT  = 80;
constexpr int16_t HOME_TILE_MARGIN  = 8;    // More spacing
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

// Corner radius for rounded elements - more rounded for modern look
constexpr int16_t RADIUS_SMALL      = 4;
constexpr int16_t RADIUS_MEDIUM     = 8;
constexpr int16_t RADIUS_LARGE      = 12;
constexpr int16_t RADIUS_BUBBLE     = 12;  // For chat bubbles

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
