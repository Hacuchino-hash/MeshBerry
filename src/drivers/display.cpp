/**
 * MeshBerry Display Driver Implementation
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
 * Uses Adafruit ST7789 library (same as MeshCore) for display control
 */

#include "display.h"
#include <SPI.h>
#include <Adafruit_GFX.h>
#include <Adafruit_ST7789.h>
#include "../ui/Emoji.h"
#include "../ui/Theme.h"

// T-Deck pins (from LilyGo official)
#define BOARD_POWERON   10
#define BOARD_TFT_CS    12
#define BOARD_TFT_DC    11
#define BOARD_TFT_SCLK  40
#define BOARD_TFT_MOSI  41
#define BOARD_TFT_BL    42

// Display dimensions
#define DISPLAY_WIDTH  240
#define DISPLAY_HEIGHT 320

// Dynamically allocated to avoid global constructor issues
static SPIClass* displaySPI = nullptr;
static Adafruit_ST7789* display = nullptr;
static bool displayInitialized = false;
static uint8_t currentBrightness = 255;

namespace Display {

bool init() {
    if (displayInitialized) {
        return true;
    }

    Serial.println("[DISPLAY] Initializing ST7789...");

    // Step 1: Ensure peripheral power is ON
    pinMode(BOARD_POWERON, OUTPUT);
    digitalWrite(BOARD_POWERON, HIGH);
    delay(100);
    Serial.println("[DISPLAY] Peripheral power ON");

    // Step 2: Turn on backlight IMMEDIATELY
    pinMode(BOARD_TFT_BL, OUTPUT);
    digitalWrite(BOARD_TFT_BL, HIGH);
    Serial.println("[DISPLAY] Backlight ON");

    // Step 3: Initialize HSPI
    displaySPI = new SPIClass(HSPI);
    displaySPI->begin(BOARD_TFT_SCLK, -1, BOARD_TFT_MOSI, BOARD_TFT_CS);
    Serial.println("[DISPLAY] HSPI initialized");

    // Step 4: Create and initialize display
    display = new Adafruit_ST7789(displaySPI, BOARD_TFT_CS, BOARD_TFT_DC, -1);
    display->init(DISPLAY_WIDTH, DISPLAY_HEIGHT);
    Serial.println("[DISPLAY] ST7789 init complete");

    // Step 5: Configure display
    display->setRotation(3);  // Landscape
    display->setSPISpeed(40000000);

    // Step 6: Clear display (skip color test for faster boot)
    display->fillScreen(ST77XX_BLACK);

    // Step 7: Configure text
    display->setTextColor(ST77XX_WHITE);
    display->setTextSize(2);
    display->cp437(true);

    // Step 8: Set up PWM for backlight dimming
    ledcSetup(TFT_BL_PWM_CHANNEL, TFT_BL_PWM_FREQ, TFT_BL_PWM_RES);
    ledcAttachPin(BOARD_TFT_BL, TFT_BL_PWM_CHANNEL);
    setBacklight(255);

    displayInitialized = true;
    Serial.printf("[DISPLAY] Initialized: %dx%d\n", display->width(), display->height());

    return true;
}

void setBacklight(uint8_t level) {
    currentBrightness = level;
    ledcWrite(TFT_BL_PWM_CHANNEL, level);
}

void backlightOn() {
    setBacklight(255);
}

void backlightOff() {
    setBacklight(0);
}

void clear(uint16_t color) {
    if (!displayInitialized || !display) return;
    display->fillScreen(color);
}

void drawText(int16_t x, int16_t y, const char* text, uint16_t color, uint8_t size) {
    if (!displayInitialized || !display) return;
    display->setTextColor(color);
    display->setTextSize(size);
    display->setCursor(x, y);
    display->print(text);
}

void fillRect(int16_t x, int16_t y, int16_t w, int16_t h, uint16_t color) {
    if (!displayInitialized || !display) return;
    display->fillRect(x, y, w, h, color);
}

void drawRect(int16_t x, int16_t y, int16_t w, int16_t h, uint16_t color) {
    if (!displayInitialized || !display) return;
    display->drawRect(x, y, w, h, color);
}

uint16_t getWidth() {
    return (displayInitialized && display) ? display->width() : DISPLAY_HEIGHT;
}

uint16_t getHeight() {
    return (displayInitialized && display) ? display->height() : DISPLAY_WIDTH;
}

void setRotation(uint8_t rotation) {
    if (!displayInitialized || !display) return;
    display->setRotation(rotation);
}

// =============================================================================
// ENHANCED DRAWING PRIMITIVES
// =============================================================================

void fillRoundRect(int16_t x, int16_t y, int16_t w, int16_t h, int16_t radius, uint16_t color) {
    if (!displayInitialized || !display) return;
    display->fillRoundRect(x, y, w, h, radius, color);
}

void drawRoundRect(int16_t x, int16_t y, int16_t w, int16_t h, int16_t radius, uint16_t color) {
    if (!displayInitialized || !display) return;
    display->drawRoundRect(x, y, w, h, radius, color);
}

void drawHLine(int16_t x, int16_t y, int16_t w, uint16_t color) {
    if (!displayInitialized || !display) return;
    display->drawFastHLine(x, y, w, color);
}

void drawVLine(int16_t x, int16_t y, int16_t h, uint16_t color) {
    if (!displayInitialized || !display) return;
    display->drawFastVLine(x, y, h, color);
}

void drawLine(int16_t x0, int16_t y0, int16_t x1, int16_t y1, uint16_t color) {
    if (!displayInitialized || !display) return;
    display->drawLine(x0, y0, x1, y1, color);
}

void fillCircle(int16_t x, int16_t y, int16_t r, uint16_t color) {
    if (!displayInitialized || !display) return;
    display->fillCircle(x, y, r, color);
}

void drawCircle(int16_t x, int16_t y, int16_t r, uint16_t color) {
    if (!displayInitialized || !display) return;
    display->drawCircle(x, y, r, color);
}

void fillTriangle(int16_t x0, int16_t y0, int16_t x1, int16_t y1, int16_t x2, int16_t y2, uint16_t color) {
    if (!displayInitialized || !display) return;
    display->fillTriangle(x0, y0, x1, y1, x2, y2, color);
}

void drawTextCentered(int16_t x, int16_t y, int16_t w, const char* text, uint16_t color, uint8_t size) {
    if (!displayInitialized || !display || !text) return;

    // Calculate text width (6 pixels per char at size 1)
    int16_t charWidth = 6 * size;
    int16_t textWidth = strlen(text) * charWidth;
    int16_t centeredX = x + (w - textWidth) / 2;

    display->setTextColor(color);
    display->setTextSize(size);
    display->setCursor(centeredX, y);
    display->print(text);
}

void drawTextRight(int16_t x, int16_t y, const char* text, uint16_t color, uint8_t size) {
    if (!displayInitialized || !display || !text) return;

    // Calculate text width
    int16_t charWidth = 6 * size;
    int16_t textWidth = strlen(text) * charWidth;
    int16_t rightX = x - textWidth;

    display->setTextColor(color);
    display->setTextSize(size);
    display->setCursor(rightX, y);
    display->print(text);
}

void drawBitmap(int16_t x, int16_t y, const uint8_t* bitmap, int16_t w, int16_t h, uint16_t color) {
    if (!displayInitialized || !display || !bitmap) return;
    display->drawBitmap(x, y, bitmap, w, h, color);
}

void drawBitmapBg(int16_t x, int16_t y, const uint8_t* bitmap, int16_t w, int16_t h, uint16_t fgColor, uint16_t bgColor) {
    if (!displayInitialized || !display || !bitmap) return;
    display->drawBitmap(x, y, bitmap, w, h, fgColor, bgColor);
}

void drawRGB565(int16_t x, int16_t y, const uint16_t* bitmap, int16_t w, int16_t h) {
    if (!displayInitialized || !display || !bitmap) return;

    int pixels = w * h;

    if (pixels <= 144) {
        // Small image (emoji size) - use stack buffer for efficiency
        uint16_t buffer[144];
        memcpy_P(buffer, bitmap, pixels * sizeof(uint16_t));
        display->drawRGBBitmap(x, y, buffer, w, h);
    } else {
        // Large image - draw row by row to avoid stack overflow
        // Use a row buffer (max screen width is 320)
        uint16_t rowBuffer[320];
        int rowPixels = (w < 320) ? w : 320;

        for (int row = 0; row < h; row++) {
            memcpy_P(rowBuffer, bitmap + (row * w), rowPixels * sizeof(uint16_t));
            display->drawRGBBitmap(x, y + row, rowBuffer, rowPixels, 1);
        }
    }
}

// =============================================================================
// EMOJI-AWARE TEXT RENDERING
// =============================================================================

void drawTextWithEmoji(int16_t x, int16_t y, const char* text, uint16_t color, uint8_t size) {
    if (!displayInitialized || !display || !text) return;

    int16_t cursorX = x;
    int16_t charWidth = 6 * size;
    int16_t charHeight = 8 * size;
    const char* p = text;

    while (*p) {
        uint32_t codepoint;
        int bytes = Emoji::decodeUTF8(p, &codepoint);

        if (bytes == 0) {
            // Invalid byte, skip it
            p++;
            continue;
        }

        if (bytes == 1) {
            // ASCII character - render normally
            char c[2] = { (char)codepoint, '\0' };
            display->setTextColor(color);
            display->setTextSize(size);
            display->setCursor(cursorX, y);
            display->print(c);
            cursorX += charWidth;
            p++;
        } else {
            // Multi-byte UTF-8 character - check if it's an emoji
            const EmojiEntry* emoji = Emoji::findByCodepoint(codepoint);
            if (emoji && emoji->bitmap) {
                // Draw emoji bitmap (12x12 RGB565)
                // Center vertically relative to text
                int16_t emojiY = y;
                if (charHeight > EMOJI_HEIGHT) {
                    emojiY += (charHeight - EMOJI_HEIGHT) / 2;
                }
                // Use our PROGMEM-safe function
                drawRGB565(cursorX, emojiY, emoji->bitmap, EMOJI_WIDTH, EMOJI_HEIGHT);
                cursorX += EMOJI_WIDTH;
            } else {
                // Unknown Unicode character - render placeholder [?]
                display->setTextColor(color);
                display->setTextSize(size);
                display->setCursor(cursorX, y);
                display->print("[?]");
                cursorX += 3 * charWidth;
            }
            p += bytes;
        }
    }
}

void drawTextWithEmojiCentered(int16_t x, int16_t y, int16_t w, const char* text, uint16_t color, uint8_t size) {
    if (!displayInitialized || !display || !text) return;

    // Calculate width using emoji-aware function
    int16_t textWidth = Emoji::textWidth(text, size);
    int16_t centeredX = x + (w - textWidth) / 2;

    drawTextWithEmoji(centeredX, y, text, color, size);
}

void* getDisplayPtr() {
    return display;
}

// =============================================================================
// MODERN UI HELPER FUNCTIONS IMPLEMENTATION
// =============================================================================

void drawCard(int16_t x, int16_t y, int16_t w, int16_t h,
              uint16_t bgColor, int16_t radius, bool shadow) {
    if (!displayInitialized || !display) return;

    // Draw shadow if requested
    if (shadow) {
        drawShadow(x, y, w, h, 4, 40);
    }

    // Draw card background
    display->fillRoundRect(x, y, w, h, radius, bgColor);
}

void fillGradient(int16_t x, int16_t y, int16_t w, int16_t h,
                  uint16_t colorTop, uint16_t colorBottom) {
    if (!displayInitialized || !display) return;

    // Simple dithered gradient - draw horizontal lines with interpolated colors
    for (int16_t row = 0; row < h; row++) {
        // Interpolate color (simple linear blend)
        float ratio = (float)row / (float)h;

        // Extract RGB components (RGB565 format)
        uint8_t r1 = (colorTop >> 11) & 0x1F;
        uint8_t g1 = (colorTop >> 5) & 0x3F;
        uint8_t b1 = colorTop & 0x1F;

        uint8_t r2 = (colorBottom >> 11) & 0x1F;
        uint8_t g2 = (colorBottom >> 5) & 0x3F;
        uint8_t b2 = colorBottom & 0x1F;

        // Blend
        uint8_t r = r1 + (r2 - r1) * ratio;
        uint8_t g = g1 + (g2 - g1) * ratio;
        uint8_t b = b1 + (b2 - b1) * ratio;

        // Reconstruct RGB565
        uint16_t color = (r << 11) | (g << 5) | b;

        // Draw line
        display->drawFastHLine(x, y + row, w, color);
    }
}

void drawShadow(int16_t x, int16_t y, int16_t w, int16_t h,
                int16_t offset, uint8_t opacity) {
    if (!displayInitialized || !display) return;

    // Simple shadow - multiple offset rectangles with decreasing opacity
    // For RGB565, we'll use progressively lighter gray

    // Calculate shadow gray levels based on opacity
    uint8_t baseGray = (opacity * 255) / 100 / 5;  // Divided by 5 for 5 layers

    for (int i = offset; i > 0; i--) {
        uint8_t grayLevel = baseGray * i;
        uint16_t shadowColor = (grayLevel >> 3) << 11 | (grayLevel >> 2) << 5 | (grayLevel >> 3);

        // Draw shadow rect offset
        display->drawRect(x + offset - i + 1, y + offset - i + 1,
                         w + i, h + i, shadowColor);
    }
}

void drawButton(int16_t x, int16_t y, int16_t w, int16_t h,
                const char* label, ButtonState state, bool isPrimary) {
    if (!displayInitialized || !display) return;

    uint16_t bgColor, borderColor, textColor;

    if (state == BTN_DISABLED) {
        bgColor = Theme::COLOR_BG_CARD;
        borderColor = Theme::COLOR_DIVIDER;
        textColor = Theme::COLOR_TEXT_DISABLED;
    } else if (isPrimary) {
        if (state == BTN_PRESSED) {
            bgColor = Theme::COLOR_PRIMARY_DARK;
        } else if (state == BTN_HOVER) {
            bgColor = Theme::COLOR_PRIMARY_LIGHT;
        } else {
            bgColor = Theme::COLOR_PRIMARY;
        }
        borderColor = bgColor;
        textColor = Theme::WHITE;
    } else {
        // Secondary button (outline)
        bgColor = Theme::COLOR_BG_CARD;
        if (state == BTN_PRESSED) {
            borderColor = Theme::COLOR_PRIMARY_DARK;
            textColor = Theme::COLOR_PRIMARY_DARK;
        } else if (state == BTN_HOVER) {
            borderColor = Theme::COLOR_PRIMARY_LIGHT;
            textColor = Theme::COLOR_PRIMARY_LIGHT;
        } else {
            borderColor = Theme::COLOR_PRIMARY;
            textColor = Theme::COLOR_PRIMARY;
        }
    }

    // Draw button background
    display->fillRoundRect(x, y, w, h, 8, bgColor);

    // Draw border for secondary buttons
    if (!isPrimary && state != BTN_DISABLED) {
        display->drawRoundRect(x, y, w, h, 8, borderColor);
    }

    // Draw centered label
    drawTextCentered(x, y + (h - 8) / 2, w, label, textColor, 1);
}

void drawToggle(int16_t x, int16_t y, bool isOn, bool isEnabled) {
    if (!displayInitialized || !display) return;

    // Toggle dimensions: 48x28 (standard iOS-like toggle)
    const int16_t toggleW = 48;
    const int16_t toggleH = 28;
    const int16_t knobRadius = 12;
    const int16_t knobY = y + toggleH / 2;

    // Colors
    uint16_t trackColor, knobColor;
    if (!isEnabled) {
        trackColor = Theme::GRAY_DARK;
        knobColor = Theme::GRAY_MID;
    } else if (isOn) {
        trackColor = Theme::COLOR_PRIMARY;
        knobColor = Theme::WHITE;
    } else {
        trackColor = Theme::GRAY_MID;
        knobColor = Theme::WHITE;
    }

    // Draw track (rounded rect)
    display->fillRoundRect(x, y, toggleW, toggleH, toggleH / 2, trackColor);

    // Draw knob (circle)
    int16_t knobX = isOn ? (x + toggleW - knobRadius - 2) : (x + knobRadius + 2);
    display->fillCircle(knobX, knobY, knobRadius, knobColor);
}

void drawProgressBar(int16_t x, int16_t y, int16_t w, int16_t h,
                     float progress, uint16_t color) {
    if (!displayInitialized || !display) return;

    // Clamp progress
    if (progress < 0.0f) progress = 0.0f;
    if (progress > 1.0f) progress = 1.0f;

    // Draw background
    display->fillRoundRect(x, y, w, h, h / 2, Theme::COLOR_BG_INPUT);

    // Draw progress fill
    int16_t fillWidth = (int16_t)(w * progress);
    if (fillWidth > 0) {
        display->fillRoundRect(x, y, fillWidth, h, h / 2, color);
    }
}

void drawBadge(int16_t x, int16_t y, int count, uint16_t bgColor) {
    if (!displayInitialized || !display || count <= 0) return;

    // Badge is a small circle with white text
    const int16_t radius = 10;

    // Draw circle
    display->fillCircle(x, y, radius, bgColor);

    // Draw count text
    char countStr[4];
    if (count > 99) {
        strcpy(countStr, "99+");
    } else {
        snprintf(countStr, 4, "%d", count);
    }

    // Center text in circle
    int16_t textW = strlen(countStr) * 6;  // 6px per char at size 1
    int16_t textX = x - textW / 2;
    int16_t textY = y - 4;  // 8px font height / 2

    display->setTextSize(1);
    display->setTextColor(Theme::WHITE);
    display->setCursor(textX, textY);
    display->print(countStr);
}

void drawIconButton(int16_t x, int16_t y, int16_t radius,
                    const uint8_t* icon, uint16_t bgColor, uint16_t iconColor) {
    if (!displayInitialized || !display || !icon) return;

    // Draw circle background
    display->fillCircle(x, y, radius, bgColor);

    // Draw icon centered (assuming 16x16 icon)
    drawBitmap(x - 8, y - 8, icon, 16, 16, iconColor);
}

} // namespace Display
