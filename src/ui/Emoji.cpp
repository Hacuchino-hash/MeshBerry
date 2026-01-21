/**
 * MeshBerry Emoji Support Implementation
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2026 NodakMesh (nodakmesh.org)
 */

#include "Emoji.h"
#include <string.h>

// Include the generated emoji bitmap data (353 emoji)
#include "EmojiData.h"

// Category names
static const char* CATEGORY_NAMES[] = {
    "Faces",
    "Gestures",
    "People",
    "Hearts",
    "Animals",
    "Food",
    "Activities",
    "Travel",
    "Objects",
    "Symbols",
    "Flags"
};

// =========================================================================
// UTF-8 PARSING IMPLEMENTATION
// =========================================================================

namespace Emoji {

int decodeUTF8(const char* str, uint32_t* codepoint) {
    if (!str || !*str) {
        if (codepoint) *codepoint = 0;
        return 0;
    }

    uint8_t b0 = (uint8_t)str[0];

    // Single byte (ASCII)
    if ((b0 & 0x80) == 0) {
        if (codepoint) *codepoint = b0;
        return 1;
    }

    // Two bytes
    if ((b0 & 0xE0) == 0xC0) {
        if (!str[1]) return 0;
        uint8_t b1 = (uint8_t)str[1];
        if ((b1 & 0xC0) != 0x80) return 0;
        if (codepoint) *codepoint = ((b0 & 0x1F) << 6) | (b1 & 0x3F);
        return 2;
    }

    // Three bytes
    if ((b0 & 0xF0) == 0xE0) {
        if (!str[1] || !str[2]) return 0;
        uint8_t b1 = (uint8_t)str[1];
        uint8_t b2 = (uint8_t)str[2];
        if ((b1 & 0xC0) != 0x80 || (b2 & 0xC0) != 0x80) return 0;
        if (codepoint) *codepoint = ((b0 & 0x0F) << 12) | ((b1 & 0x3F) << 6) | (b2 & 0x3F);
        return 3;
    }

    // Four bytes
    if ((b0 & 0xF8) == 0xF0) {
        if (!str[1] || !str[2] || !str[3]) return 0;
        uint8_t b1 = (uint8_t)str[1];
        uint8_t b2 = (uint8_t)str[2];
        uint8_t b3 = (uint8_t)str[3];
        if ((b1 & 0xC0) != 0x80 || (b2 & 0xC0) != 0x80 || (b3 & 0xC0) != 0x80) return 0;
        if (codepoint) *codepoint = ((b0 & 0x07) << 18) | ((b1 & 0x3F) << 12) |
                                    ((b2 & 0x3F) << 6) | (b3 & 0x3F);
        return 4;
    }

    return 0;  // Invalid
}

int encodeUTF8(uint32_t codepoint, char* buf) {
    if (codepoint < 0x80) {
        buf[0] = (char)codepoint;
        buf[1] = '\0';
        return 1;
    } else if (codepoint < 0x800) {
        buf[0] = (char)(0xC0 | (codepoint >> 6));
        buf[1] = (char)(0x80 | (codepoint & 0x3F));
        buf[2] = '\0';
        return 2;
    } else if (codepoint < 0x10000) {
        buf[0] = (char)(0xE0 | (codepoint >> 12));
        buf[1] = (char)(0x80 | ((codepoint >> 6) & 0x3F));
        buf[2] = (char)(0x80 | (codepoint & 0x3F));
        buf[3] = '\0';
        return 3;
    } else if (codepoint < 0x110000) {
        buf[0] = (char)(0xF0 | (codepoint >> 18));
        buf[1] = (char)(0x80 | ((codepoint >> 12) & 0x3F));
        buf[2] = (char)(0x80 | ((codepoint >> 6) & 0x3F));
        buf[3] = (char)(0x80 | (codepoint & 0x3F));
        buf[4] = '\0';
        return 4;
    }
    buf[0] = '\0';
    return 0;
}

int utf8Length(const char* str) {
    if (!str) return 0;

    int count = 0;
    const char* p = str;
    while (*p) {
        uint32_t cp;
        int bytes = decodeUTF8(p, &cp);
        if (bytes == 0) {
            p++;  // Skip invalid byte
        } else {
            p += bytes;
            count++;
        }
    }
    return count;
}

// =========================================================================
// EMOJI LOOKUP IMPLEMENTATION
// =========================================================================

const EmojiEntry* findByCodepoint(uint32_t codepoint) {
    for (int i = 0; i < EMOJI_COUNT; i++) {
        if (EMOJI_TABLE[i].codepoint == codepoint) {
            return &EMOJI_TABLE[i];
        }
    }
    return nullptr;
}

const EmojiEntry* findByShortcode(const char* shortcode) {
    if (!shortcode) return nullptr;

    for (int i = 0; i < EMOJI_COUNT; i++) {
        if (strcmp(EMOJI_TABLE[i].shortcode, shortcode) == 0) {
            return &EMOJI_TABLE[i];
        }
    }
    return nullptr;
}

const EmojiEntry* getByIndex(int index) {
    if (index < 0 || index >= EMOJI_COUNT) return nullptr;
    return &EMOJI_TABLE[index];
}

int getCount() {
    return EMOJI_COUNT;
}

int getCategoryStart(EmojiCategory category) {
    for (int i = 0; i < EMOJI_COUNT; i++) {
        if (EMOJI_TABLE[i].category == category) {
            return i;
        }
    }
    return -1;
}

int getCategoryCount(EmojiCategory category) {
    int count = 0;
    for (int i = 0; i < EMOJI_COUNT; i++) {
        if (EMOJI_TABLE[i].category == category) {
            count++;
        }
    }
    return count;
}

const char* getCategoryName(EmojiCategory category) {
    int idx = (int)category;
    if (idx >= 0 && idx < (int)EmojiCategory::CATEGORY_COUNT) {
        return CATEGORY_NAMES[idx];
    }
    return "Unknown";
}

// =========================================================================
// TEXT PROCESSING IMPLEMENTATION
// =========================================================================

int16_t textWidth(const char* text, uint8_t textSize) {
    if (!text) return 0;

    int16_t width = 0;
    int16_t charWidth = 6 * textSize;  // Default font width
    const char* p = text;

    while (*p) {
        uint32_t cp;
        int bytes = decodeUTF8(p, &cp);

        if (bytes == 0) {
            // Invalid byte, skip
            p++;
            width += charWidth;
        } else if (bytes == 1) {
            // ASCII character
            width += charWidth;
            p++;
        } else {
            // Multi-byte character
            if (findByCodepoint(cp)) {
                // Known emoji
                width += EMOJI_WIDTH;
            } else {
                // Unknown Unicode, render as [?]
                width += 3 * charWidth;
            }
            p += bytes;
        }
    }

    return width;
}

void convertShortcodes(char* text, size_t maxLen) {
    if (!text || maxLen < 2) return;

    char result[256];
    size_t resultPos = 0;
    const char* p = text;

    while (*p && resultPos < sizeof(result) - 5) {
        if (*p == ':') {
            // Look for closing colon
            const char* end = strchr(p + 1, ':');
            if (end && (end - p) < 32) {
                // Extract shortcode
                char shortcode[32];
                size_t len = end - p - 1;
                strncpy(shortcode, p + 1, len);
                shortcode[len] = '\0';

                // Look up emoji
                const EmojiEntry* emoji = findByShortcode(shortcode);
                if (emoji) {
                    // Encode codepoint as UTF-8
                    char utf8[5];
                    int bytes = encodeUTF8(emoji->codepoint, utf8);
                    if (bytes > 0 && resultPos + bytes < sizeof(result) - 1) {
                        memcpy(result + resultPos, utf8, bytes);
                        resultPos += bytes;
                    }
                    p = end + 1;
                    continue;
                }
            }
        }

        // Copy character as-is
        result[resultPos++] = *p++;
    }

    result[resultPos] = '\0';

    // Copy back to original buffer
    strncpy(text, result, maxLen - 1);
    text[maxLen - 1] = '\0';
}

bool isEmoji(uint32_t codepoint) {
    return findByCodepoint(codepoint) != nullptr;
}

void init() {
    // Nothing to initialize currently
    Serial.printf("[EMOJI] Initialized with %d emoji\n", EMOJI_COUNT);
}

} // namespace Emoji
