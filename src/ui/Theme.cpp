/**
 * MeshBerry UI Theme Implementation
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2026 NodakMesh (nodakmesh.org)
 */

#include "Theme.h"
#include <string.h>

namespace Theme {

// Static buffer for truncated text
static char truncateBuffer[64];

uint16_t blendColors(uint16_t c1, uint16_t c2) {
    // Extract RGB components from RGB565
    uint8_t r1 = (c1 >> 11) & 0x1F;
    uint8_t g1 = (c1 >> 5) & 0x3F;
    uint8_t b1 = c1 & 0x1F;

    uint8_t r2 = (c2 >> 11) & 0x1F;
    uint8_t g2 = (c2 >> 5) & 0x3F;
    uint8_t b2 = c2 & 0x1F;

    // Average (50/50 blend)
    uint8_t r = (r1 + r2) >> 1;
    uint8_t g = (g1 + g2) >> 1;
    uint8_t b = (b1 + b2) >> 1;

    return (r << 11) | (g << 5) | b;
}

uint16_t darkenColor(uint16_t color, uint8_t factor) {
    // Extract RGB components
    uint8_t r = (color >> 11) & 0x1F;
    uint8_t g = (color >> 5) & 0x3F;
    uint8_t b = color & 0x1F;

    // Scale by factor (0=black, 255=original)
    r = (r * factor) >> 8;
    g = (g * factor) >> 8;
    b = (b * factor) >> 8;

    return (r << 11) | (g << 5) | b;
}

uint16_t lightenColor(uint16_t color, uint8_t factor) {
    // Extract RGB components
    uint8_t r = (color >> 11) & 0x1F;
    uint8_t g = (color >> 5) & 0x3F;
    uint8_t b = color & 0x1F;

    // Scale towards white (31 for R/B, 63 for G in RGB565)
    r = r + (((31 - r) * factor) >> 8);
    g = g + (((63 - g) * factor) >> 8);
    b = b + (((31 - b) * factor) >> 8);

    return (r << 11) | (g << 5) | b;
}

int16_t getTextWidth(const char* text, uint8_t size) {
    if (!text) return 0;
    int16_t charWidth = (size >= 2) ? CHAR_WIDTH_L : CHAR_WIDTH_S;
    return strlen(text) * charWidth;
}

int16_t getCenteredX(const char* text, int16_t areaWidth, uint8_t size) {
    int16_t textWidth = getTextWidth(text, size);
    return (areaWidth - textWidth) / 2;
}

const char* truncateText(const char* text, int16_t maxWidth, uint8_t size) {
    if (!text) return "";

    int16_t charWidth = (size >= 2) ? CHAR_WIDTH_L : CHAR_WIDTH_S;
    int16_t textWidth = getTextWidth(text, size);

    if (textWidth <= maxWidth) {
        return text;  // No truncation needed
    }

    // Calculate max characters that fit (minus 3 for "...")
    int maxChars = (maxWidth / charWidth) - 3;
    if (maxChars < 1) maxChars = 1;
    if (maxChars > 60) maxChars = 60;  // Buffer safety

    strncpy(truncateBuffer, text, maxChars);
    truncateBuffer[maxChars] = '\0';
    strcat(truncateBuffer, "...");

    return truncateBuffer;
}

} // namespace Theme
