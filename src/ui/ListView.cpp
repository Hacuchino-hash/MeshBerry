/**
 * MeshBerry ListView Widget Implementation
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2026 NodakMesh (nodakmesh.org)
 */

#include "ListView.h"
#include "../drivers/display.h"

ListView::ListView() {
    _x = 0;
    _y = Theme::CONTENT_Y;
    _width = Theme::SCREEN_WIDTH;
    _height = Theme::CONTENT_HEIGHT;
}

void ListView::setBounds(int16_t x, int16_t y, int16_t width, int16_t height) {
    _x = x;
    _y = y;
    _width = width;
    _height = height;
    _needsRedraw = true;
}

void ListView::setItems(const ListItem* items, int count) {
    _items = items;
    _itemCount = count;
    _selectedIndex = 0;
    _scrollOffset = 0;
    _needsRedraw = true;
}

void ListView::setSelectedIndex(int index) {
    if (index < 0) index = 0;
    if (index >= _itemCount) index = _itemCount - 1;
    if (index != _selectedIndex) {
        _selectedIndex = index;
        ensureVisible();
        _needsRedraw = true;
    }
}

void ListView::calcVisibleRange(int& first, int& last) {
    int visibleCount = _height / _itemHeight;
    first = _scrollOffset;
    last = _scrollOffset + visibleCount;
    if (last > _itemCount) last = _itemCount;
}

void ListView::ensureVisible() {
    int visibleCount = _height / _itemHeight;

    // Scroll up if needed
    if (_selectedIndex < _scrollOffset) {
        _scrollOffset = _selectedIndex;
    }
    // Scroll down if needed
    else if (_selectedIndex >= _scrollOffset + visibleCount) {
        _scrollOffset = _selectedIndex - visibleCount + 1;
    }
}

void ListView::draw(bool fullRedraw) {
    if (!_items || _itemCount == 0) {
        if (fullRedraw) {
            Display::fillRect(_x, _y, _width, _height, Theme::BG_PRIMARY);
            Display::drawTextCentered(_x, _y + _height / 2 - 8,
                                      _width, "No items", Theme::TEXT_SECONDARY, 1);
        }
        return;
    }

    if (fullRedraw) {
        Display::fillRect(_x, _y, _width, _height, Theme::BG_PRIMARY);
    }

    // Draw visible items
    int first, last;
    calcVisibleRange(first, last);

    for (int i = first; i < last; i++) {
        drawItem(i, i == _selectedIndex);
    }

    // Draw scroll indicators
    if (_showScrollIndicators) {
        int16_t indicatorX = _x + _width - 8;

        // Up arrow if can scroll up
        if (_scrollOffset > 0) {
            Display::drawText(indicatorX, _y + 2, "^", Theme::ACCENT, 1);
        }

        // Down arrow if can scroll down
        int visibleCount = _height / _itemHeight;
        if (_scrollOffset + visibleCount < _itemCount) {
            Display::drawText(indicatorX, _y + _height - 12, "v", Theme::ACCENT, 1);
        }
    }

    _needsRedraw = false;
}

void ListView::drawItem(int index, bool selected) {
    if (index < 0 || index >= _itemCount || !_items) return;

    const ListItem& item = _items[index];
    int16_t itemY = _y + (index - _scrollOffset) * _itemHeight;

    // Check if item is visible
    if (itemY < _y || itemY + _itemHeight > _y + _height) return;

    // Modern background styling
    if (selected) {
        // Selected item - softer teal highlight with single rounded border
        Display::fillRoundRect(_x + 4, itemY + 2,
                               _width - 8, _itemHeight - 4,
                               Theme::RADIUS_MEDIUM, Theme::ACCENT_PRIMARY);
    } else {
        // Unselected - clean background
        Display::fillRect(_x, itemY, _width, _itemHeight, Theme::BG_PRIMARY);
    }

    int16_t textX = _x + _itemPadding + 4;  // Slightly more padding
    int16_t iconSize = 16;

    // Draw indicator dot if present
    if (item.hasIndicator) {
        Display::fillCircle(_x + 8, itemY + _itemHeight / 2, 3, item.indicatorColor);
        textX += 10;
    }

    // Draw icon if present
    if (item.icon) {
        int16_t iconY = itemY + (_itemHeight - iconSize) / 2;
        uint16_t iconColor = selected ? Theme::WHITE : item.iconColor;
        Display::drawBitmap(textX, iconY, item.icon, iconSize, iconSize, iconColor);
        textX += iconSize + 8;  // More spacing after icon
    }

    // Calculate max text width (account for padding and scroll indicators)
    int16_t maxTextWidth = _x + _width - textX - (_showScrollIndicators ? 14 : 6);

    // Draw primary text
    int16_t primaryY = itemY + 8;
    if (item.secondary) {
        // Two-line layout
        primaryY = itemY + 8;
    } else {
        // Single line - center vertically
        primaryY = itemY + (_itemHeight - 8) / 2;
    }

    uint16_t primaryColor = selected ? Theme::WHITE : Theme::TEXT_PRIMARY;
    const char* primaryText = Theme::truncateText(item.primary, maxTextWidth, 1);
    Display::drawText(textX, primaryY, primaryText, primaryColor, 1);

    // Draw secondary text if present (truncate to fit)
    if (item.secondary) {
        int16_t secondaryY = itemY + 24;
        uint16_t secondaryColor = selected ? Theme::GRAY_LIGHTER : Theme::TEXT_SECONDARY;
        const char* secondaryText = Theme::truncateText(item.secondary, maxTextWidth, 1);
        Display::drawText(textX, secondaryY, secondaryText, secondaryColor, 1);
    }

    // No divider lines - modern clean look uses spacing instead
}

bool ListView::handleTrackball(bool up, bool down, bool left, bool right, bool click) {
    if (_itemCount == 0) return false;

    int prevSelected = _selectedIndex;

    if (up) {
        if (_selectedIndex > 0) {
            _selectedIndex--;
            ensureVisible();
        }
    } else if (down) {
        if (_selectedIndex < _itemCount - 1) {
            _selectedIndex++;
            ensureVisible();
        }
    } else if (click) {
        // Trigger selection callback
        if (_selectCallback && _selectedIndex >= 0 && _selectedIndex < _itemCount) {
            _selectCallback(_selectedIndex, &_items[_selectedIndex]);
        }
        return true;
    } else if (left || right) {
        // Left/right not used by ListView - let parent handle
        return false;
    }

    if (_selectedIndex != prevSelected) {
        _needsRedraw = true;
        return true;
    }

    return false;
}

bool ListView::handleTouchDrag(int16_t deltaY) {
    if (_itemCount == 0) return false;

    int visibleCount = _height / _itemHeight;
    int maxOffset = _itemCount - visibleCount;
    if (maxOffset < 0) maxOffset = 0;

    // Scroll opposite to drag direction (drag down = scroll up = show earlier items)
    int pixelThreshold = _itemHeight / 2;  // Half item height triggers scroll
    if (abs(deltaY) >= pixelThreshold) {
        int itemDelta = deltaY > 0 ? -1 : 1;  // Drag down = scroll up
        int newOffset = _scrollOffset + itemDelta;

        if (newOffset < 0) newOffset = 0;
        if (newOffset > maxOffset) newOffset = maxOffset;

        if (newOffset != _scrollOffset) {
            _scrollOffset = newOffset;
            // Keep selection visible
            if (_selectedIndex < _scrollOffset) {
                _selectedIndex = _scrollOffset;
            } else if (_selectedIndex >= _scrollOffset + visibleCount) {
                _selectedIndex = _scrollOffset + visibleCount - 1;
            }
            _needsRedraw = true;
            return true;
        }
    }
    return false;
}
