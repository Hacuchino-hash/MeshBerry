/**
 * MeshBerry Soft Key Bar
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2026 NodakMesh (nodakmesh.org)
 *
 * BlackBerry-style context-sensitive action bar at bottom of screen
 */

#ifndef MESHBERRY_SOFTKEYBAR_H
#define MESHBERRY_SOFTKEYBAR_H

#include <Arduino.h>
#include "Theme.h"

namespace SoftKeyBar {

/**
 * Soft key positions
 */
enum SoftKey {
    KEY_LEFT = 0,    // Left soft key (typically Menu)
    KEY_CENTER = 1,  // Center/Select key (trackball click)
    KEY_RIGHT = 2    // Right soft key (typically Back)
};

/**
 * Initialize the soft key bar
 */
void init();

/**
 * Draw the soft key bar
 */
void draw();

/**
 * Force full redraw
 */
void redraw();

/**
 * Set label for a soft key
 * @param key Which key to set
 * @param label Text label (nullptr to hide)
 */
void setLabel(SoftKey key, const char* label);

/**
 * Set all three labels at once
 * @param left Left key label
 * @param center Center key label
 * @param right Right key label
 */
void setLabels(const char* left, const char* center, const char* right);

/**
 * Set highlight state for a key (pressed visual)
 * @param key Which key
 * @param highlighted True to highlight
 */
void setHighlight(SoftKey key, bool highlighted);

/**
 * Clear all highlights
 */
void clearHighlights();

/**
 * Get soft key bar height
 */
constexpr int16_t getHeight() { return Theme::SOFTKEY_BAR_HEIGHT; }

/**
 * Get Y position of soft key bar
 */
constexpr int16_t getY() { return Theme::SOFTKEY_BAR_Y; }

/**
 * Common label presets for quick setup
 */
void setNavigationMode();    // Menu / Select / Back
void setConfirmMode();       // Cancel / OK / (empty)
void setListMode();          // Menu / Open / Back
void setEditMode();          // Clear / Save / Cancel
void setDialogMode();        // No / Yes / (empty)

} // namespace SoftKeyBar

#endif // MESHBERRY_SOFTKEYBAR_H
