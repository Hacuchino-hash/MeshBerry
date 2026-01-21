/**
 * MeshBerry Emoji Support
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2026 NodakMesh (nodakmesh.org)
 *
 * Emoji rendering and UTF-8 parsing for chat messages.
 * Emoji are stored as 12x12 RGB565 bitmaps in PROGMEM.
 */

#ifndef MESHBERRY_EMOJI_H
#define MESHBERRY_EMOJI_H

#include <Arduino.h>

// Emoji bitmap dimensions
#define EMOJI_WIDTH  12
#define EMOJI_HEIGHT 12
#define EMOJI_PIXELS (EMOJI_WIDTH * EMOJI_HEIGHT)  // 144 pixels

// Category IDs for emoji picker
enum class EmojiCategory : uint8_t {
    FACES = 0,
    GESTURES,
    PEOPLE,
    HEARTS,
    ANIMALS,
    FOOD,
    ACTIVITIES,
    TRAVEL,
    OBJECTS,
    SYMBOLS,
    FLAGS,
    CATEGORY_COUNT
};

/**
 * Emoji entry structure
 */
struct EmojiEntry {
    uint32_t codepoint;           // Unicode codepoint (e.g., 0x1F600 for grinning face)
    const char* shortcode;        // Shortcode without colons (e.g., "smile")
    const uint16_t* bitmap;       // 12x12 RGB565 bitmap in PROGMEM
    EmojiCategory category;       // Category for picker organization
};

namespace Emoji {

// =========================================================================
// UTF-8 PARSING
// =========================================================================

/**
 * Decode a UTF-8 character from a string
 * @param str Input string pointer
 * @param codepoint Output Unicode codepoint
 * @return Number of bytes consumed (1-4), or 0 if invalid
 */
int decodeUTF8(const char* str, uint32_t* codepoint);

/**
 * Encode a Unicode codepoint to UTF-8
 * @param codepoint Unicode codepoint to encode
 * @param buf Output buffer (must be at least 5 bytes)
 * @return Number of bytes written (1-4)
 */
int encodeUTF8(uint32_t codepoint, char* buf);

/**
 * Calculate the length of a UTF-8 string in characters (not bytes)
 * @param str UTF-8 string
 * @return Number of Unicode characters
 */
int utf8Length(const char* str);

// =========================================================================
// EMOJI LOOKUP
// =========================================================================

/**
 * Find emoji by Unicode codepoint
 * @param codepoint Unicode codepoint
 * @return Pointer to EmojiEntry, or nullptr if not found
 */
const EmojiEntry* findByCodepoint(uint32_t codepoint);

/**
 * Find emoji by shortcode (without colons)
 * @param shortcode Shortcode string (e.g., "smile")
 * @return Pointer to EmojiEntry, or nullptr if not found
 */
const EmojiEntry* findByShortcode(const char* shortcode);

/**
 * Get emoji by index
 * @param index Index in the emoji table
 * @return Pointer to EmojiEntry, or nullptr if out of range
 */
const EmojiEntry* getByIndex(int index);

/**
 * Get total number of emoji
 */
int getCount();

/**
 * Get first emoji index for a category
 * @param category Category ID
 * @return Index of first emoji in category, or -1 if empty
 */
int getCategoryStart(EmojiCategory category);

/**
 * Get number of emoji in a category
 * @param category Category ID
 * @return Number of emoji in category
 */
int getCategoryCount(EmojiCategory category);

/**
 * Get category name
 * @param category Category ID
 * @return Category name string
 */
const char* getCategoryName(EmojiCategory category);

// =========================================================================
// TEXT PROCESSING
// =========================================================================

/**
 * Calculate pixel width of text with emoji
 * @param text UTF-8 text string
 * @param textSize Text size (1 or 2)
 * @return Width in pixels
 */
int16_t textWidth(const char* text, uint8_t textSize);

/**
 * Convert shortcodes in text to UTF-8 emoji
 * Replaces :shortcode: patterns with actual emoji bytes
 * @param text Text buffer (modified in place)
 * @param maxLen Maximum buffer length
 */
void convertShortcodes(char* text, size_t maxLen);

/**
 * Check if a codepoint is a known emoji
 * @param codepoint Unicode codepoint
 * @return true if this is a supported emoji
 */
bool isEmoji(uint32_t codepoint);

// =========================================================================
// INITIALIZATION
// =========================================================================

/**
 * Initialize emoji subsystem
 * Call once at startup
 */
void init();

} // namespace Emoji

#endif // MESHBERRY_EMOJI_H
