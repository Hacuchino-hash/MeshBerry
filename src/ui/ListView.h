/**
 * MeshBerry ListView Widget
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2026 NodakMesh (nodakmesh.org)
 *
 * Scrollable list widget with BlackBerry-style selection
 */

#ifndef MESHBERRY_LISTVIEW_H
#define MESHBERRY_LISTVIEW_H

#include <stdint.h>
#include "Theme.h"

/**
 * List item structure
 */
struct ListItem {
    const char* primary;      // Primary text (required)
    const char* secondary;    // Secondary text (optional)
    const uint8_t* icon;      // 16x16 icon (optional)
    uint16_t iconColor;       // Icon color
    bool hasIndicator;        // Show indicator dot
    uint16_t indicatorColor;  // Indicator color
    void* userData;           // User data pointer
};

/**
 * ListView callback type
 */
typedef void (*ListViewCallback)(int index, const ListItem* item);

/**
 * ListView Widget - Scrollable list with selection
 */
class ListView {
public:
    ListView();
    ~ListView() = default;

    /**
     * Set the list bounds within content area
     */
    void setBounds(int16_t x, int16_t y, int16_t width, int16_t height);

    /**
     * Set items (pointer to external array)
     * @param items Array of ListItem
     * @param count Number of items
     */
    void setItems(const ListItem* items, int count);

    /**
     * Get currently selected index
     */
    int getSelectedIndex() const { return _selectedIndex; }

    /**
     * Get item count
     */
    int getItemCount() const { return _itemCount; }

    /**
     * Set selected index
     */
    void setSelectedIndex(int index);

    /**
     * Set selection callback
     */
    void setSelectCallback(ListViewCallback cb) { _selectCallback = cb; }

    /**
     * Draw the list
     * @param fullRedraw Clear and redraw everything
     */
    void draw(bool fullRedraw);

    /**
     * Handle trackball input
     * @return true if input was consumed
     */
    bool handleTrackball(bool up, bool down, bool left, bool right, bool click);

    /**
     * Check if list needs redraw
     */
    bool needsRedraw() const { return _needsRedraw; }

    /**
     * Clear redraw flag
     */
    void clearRedrawFlag() { _needsRedraw = false; }

    /**
     * Request redraw
     */
    void requestRedraw() { _needsRedraw = true; }

    /**
     * Get item height
     */
    int16_t getItemHeight() const { return _itemHeight; }

    /**
     * Set item height
     */
    void setItemHeight(int16_t height) { _itemHeight = height; }

    /**
     * Show/hide scroll indicators
     */
    void setShowScrollIndicators(bool show) { _showScrollIndicators = show; }

private:
    // Draw a single item
    void drawItem(int index, bool selected);

    // Ensure selected item is visible
    void ensureVisible();

    // Calculate visible range
    void calcVisibleRange(int& first, int& last);

    // Bounds
    int16_t _x = 0;
    int16_t _y = Theme::CONTENT_Y;
    int16_t _width = Theme::SCREEN_WIDTH;
    int16_t _height = Theme::CONTENT_HEIGHT;

    // Items
    const ListItem* _items = nullptr;
    int _itemCount = 0;

    // State
    int _selectedIndex = 0;
    int _scrollOffset = 0;  // First visible item index
    bool _needsRedraw = true;

    // Layout
    int16_t _itemHeight = 44;  // Default item height (touch-friendly)
    int16_t _itemPadding = 8;
    bool _showScrollIndicators = true;

    // Callback
    ListViewCallback _selectCallback = nullptr;
};

#endif // MESHBERRY_LISTVIEW_H
