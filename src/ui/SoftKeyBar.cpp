/**
 * MeshBerry Soft Key Bar Implementation
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2026 NodakMesh (nodakmesh.org)
 */

#include "SoftKeyBar.h"
#include "../drivers/display.h"
#include <string.h>

namespace SoftKeyBar {

// Labels for each position
static char leftLabel[12] = "";
static char centerLabel[12] = "";
static char rightLabel[12] = "";

// Highlight states
static bool leftHighlight = false;
static bool centerHighlight = false;
static bool rightHighlight = false;

// Highlight auto-clear timer (in ms)
static uint32_t highlightClearTime = 0;
static const uint32_t HIGHLIGHT_DURATION_MS = 100;

// Previous state for partial updates
static char prevLeftLabel[12] = "";
static char prevCenterLabel[12] = "";
static char prevRightLabel[12] = "";
static bool prevLeftHighlight = false;
static bool prevCenterHighlight = false;
static bool prevRightHighlight = false;
static bool forceRedraw = true;

// Layout constants
static const int16_t KEY_WIDTH = Theme::SCREEN_WIDTH / 3;  // ~106 pixels each
static const int16_t KEY_PADDING = 4;

void init() {
    leftLabel[0] = '\0';
    centerLabel[0] = '\0';
    rightLabel[0] = '\0';
    leftHighlight = false;
    centerHighlight = false;
    rightHighlight = false;
    forceRedraw = true;
}

static void drawKey(int16_t x, int16_t w, const char* label, bool highlight, bool needsDraw) {
    if (!needsDraw) return;

    int16_t y = Theme::SOFTKEY_BAR_Y;
    int16_t h = Theme::SOFTKEY_BAR_HEIGHT;

    // Background
    uint16_t bgColor = highlight ? Theme::BLUE : Theme::BG_SECONDARY;
    Display::fillRect(x, y, w, h, bgColor);

    // Label text (centered)
    if (label && label[0]) {
        uint16_t textColor = highlight ? Theme::WHITE : Theme::TEXT_PRIMARY;
        int16_t textY = y + (h - 8) / 2;  // Center text vertically
        Display::drawTextCentered(x, textY, w, label, textColor, 1);
    }

    // Subtle dividers between keys
    if (x > 0) {
        Display::drawVLine(x, y + 4, h - 8, Theme::GRAY_MID);
    }
}

void draw() {
    // Auto-clear highlights after duration
    if (highlightClearTime > 0 && millis() >= highlightClearTime) {
        leftHighlight = false;
        centerHighlight = false;
        rightHighlight = false;
        highlightClearTime = 0;
    }

    // Check what changed
    bool leftChanged = forceRedraw ||
                       (strcmp(leftLabel, prevLeftLabel) != 0) ||
                       (leftHighlight != prevLeftHighlight);
    bool centerChanged = forceRedraw ||
                         (strcmp(centerLabel, prevCenterLabel) != 0) ||
                         (centerHighlight != prevCenterHighlight);
    bool rightChanged = forceRedraw ||
                        (strcmp(rightLabel, prevRightLabel) != 0) ||
                        (rightHighlight != prevRightHighlight);

    if (!leftChanged && !centerChanged && !rightChanged) {
        return;
    }

    // Draw top divider line (once on full redraw)
    if (forceRedraw) {
        Display::drawHLine(0, Theme::SOFTKEY_BAR_Y, Theme::SCREEN_WIDTH, Theme::DIVIDER);
    }

    // Draw each key section
    int16_t x = 0;

    // Left key
    drawKey(x, KEY_WIDTH, leftLabel, leftHighlight, leftChanged);
    if (leftChanged) {
        strlcpy(prevLeftLabel, leftLabel, sizeof(prevLeftLabel));
        prevLeftHighlight = leftHighlight;
    }
    x += KEY_WIDTH;

    // Center key
    drawKey(x, KEY_WIDTH, centerLabel, centerHighlight, centerChanged);
    if (centerChanged) {
        strlcpy(prevCenterLabel, centerLabel, sizeof(prevCenterLabel));
        prevCenterHighlight = centerHighlight;
    }
    x += KEY_WIDTH;

    // Right key (may be slightly wider due to rounding)
    int16_t rightWidth = Theme::SCREEN_WIDTH - x;
    drawKey(x, rightWidth, rightLabel, rightHighlight, rightChanged);
    if (rightChanged) {
        strlcpy(prevRightLabel, rightLabel, sizeof(prevRightLabel));
        prevRightHighlight = rightHighlight;
    }

    forceRedraw = false;
}

void redraw() {
    forceRedraw = true;
    draw();
}

void setLabel(SoftKey key, const char* label) {
    char* target;
    switch (key) {
        case KEY_LEFT:   target = leftLabel; break;
        case KEY_CENTER: target = centerLabel; break;
        case KEY_RIGHT:  target = rightLabel; break;
        default: return;
    }

    if (label) {
        strlcpy(target, label, 12);
    } else {
        target[0] = '\0';
    }
}

void setLabels(const char* left, const char* center, const char* right) {
    setLabel(KEY_LEFT, left);
    setLabel(KEY_CENTER, center);
    setLabel(KEY_RIGHT, right);
}

void setHighlight(SoftKey key, bool highlighted) {
    switch (key) {
        case KEY_LEFT:   leftHighlight = highlighted; break;
        case KEY_CENTER: centerHighlight = highlighted; break;
        case KEY_RIGHT:  rightHighlight = highlighted; break;
    }

    // Start auto-clear timer if highlighting
    if (highlighted) {
        highlightClearTime = millis() + HIGHLIGHT_DURATION_MS;
    }
}

void clearHighlights() {
    leftHighlight = false;
    centerHighlight = false;
    rightHighlight = false;
}

// =============================================================================
// PRESET CONFIGURATIONS
// =============================================================================

void setNavigationMode() {
    setLabels("Menu", "Select", "Back");
}

void setConfirmMode() {
    setLabels("Cancel", "OK", nullptr);
}

void setListMode() {
    setLabels("Menu", "Open", "Back");
}

void setEditMode() {
    setLabels("Clear", "Save", "Cancel");
}

void setDialogMode() {
    setLabels("No", "Yes", nullptr);
}

} // namespace SoftKeyBar
