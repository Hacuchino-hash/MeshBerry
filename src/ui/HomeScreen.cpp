/**
 * MeshBerry Home Screen Implementation
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2026 NodakMesh (nodakmesh.org)
 */

#include "HomeScreen.h"
#include "SoftKeyBar.h"
#include "Icons.h"
#include "../drivers/display.h"
#include "../drivers/keyboard.h"

// Menu item configuration
struct MenuItemConfig {
    const char* label;
    const uint8_t* icon;
    ScreenId targetScreen;
};

// Note: Channels moved to Messages screen (channels are group chats)
static const MenuItemConfig menuItems[HOME_ITEM_COUNT] = {
    { "Messages",  Icons::MSG_ICON,       ScreenId::MESSAGES },
    { "Contacts",  Icons::CONTACTS_ICON,  ScreenId::CONTACTS },
    { "Settings",  Icons::SETTINGS_ICON,  ScreenId::SETTINGS },
    { "Status",    Icons::INFO_ICON,      ScreenId::STATUS },
    { "Repeaters", Icons::REPEATER_ICON,  ScreenId::CONTACTS },  // Filter to repeaters
    { "GPS",       Icons::GPS_ICON,       ScreenId::GPS },
    { "About",     Icons::INFO_ICON,      ScreenId::ABOUT }
};

void HomeScreen::onEnter() {
    requestRedraw();
}

void HomeScreen::onExit() {
    // Nothing to clean up
}

void HomeScreen::configureSoftKeys() {
    SoftKeyBar::setLabels("Menu", "Open", nullptr);
}

void HomeScreen::draw(bool fullRedraw) {
    if (fullRedraw) {
        // Clear content area
        Display::fillRect(0, Theme::CONTENT_Y,
                          Theme::SCREEN_WIDTH, Theme::CONTENT_HEIGHT,
                          Theme::BG_PRIMARY);

        // Draw all tiles
        for (int i = 0; i < HOME_ITEM_COUNT; i++) {
            drawTile(i, i == _selectedItem);
        }
    } else {
        // Partial update - only redraw changed tiles
        // For simplicity, redraw all tiles when selection changes
        for (int i = 0; i < HOME_ITEM_COUNT; i++) {
            drawTile(i, i == _selectedItem);
        }
    }
}

void HomeScreen::drawTile(int index, bool selected) {
    if (index < 0 || index >= HOME_ITEM_COUNT) return;

    const MenuItemConfig& item = menuItems[index];

    // Calculate position
    int row = getRow(index);
    int col = getCol(index);
    int16_t x = GRID_START_X + col * (TILE_WIDTH + TILE_MARGIN);
    int16_t y = GRID_START_Y + row * (TILE_HEIGHT + TILE_MARGIN);

    // Background with rounded corners
    uint16_t bgColor = selected ? Theme::BLUE_DARK : Theme::BG_SECONDARY;
    Display::fillRoundRect(x, y, TILE_WIDTH, TILE_HEIGHT, Theme::RADIUS_MEDIUM, bgColor);

    // Selection border (BB-style blue highlight)
    if (selected) {
        Display::drawRoundRect(x, y, TILE_WIDTH, TILE_HEIGHT, Theme::RADIUS_MEDIUM, Theme::BLUE);
        Display::drawRoundRect(x + 1, y + 1, TILE_WIDTH - 2, TILE_HEIGHT - 2,
                               Theme::RADIUS_MEDIUM - 1, Theme::BLUE_LIGHT);
    }

    // Icon (centered horizontally, near top)
    int16_t iconX = x + (TILE_WIDTH - 16) / 2;
    int16_t iconY = y + 15;
    uint16_t iconColor = selected ? Theme::WHITE : Theme::GRAY_LIGHTER;
    Display::drawBitmap(iconX, iconY, item.icon, 16, 16, iconColor);

    // Label (centered below icon)
    int16_t labelY = y + 45;
    uint16_t textColor = selected ? Theme::WHITE : Theme::TEXT_SECONDARY;

    // Truncate label if needed
    const char* label = Theme::truncateText(item.label, TILE_WIDTH - 8, Theme::FONT_SMALL);
    Display::drawTextCentered(x, labelY, TILE_WIDTH, label, textColor, Theme::FONT_SMALL);

    // Badge (if any)
    uint8_t badge = _badges[index];
    if (badge > 0) {
        // Badge position: top-right of tile
        int16_t badgeX = x + TILE_WIDTH - 14;
        int16_t badgeY = y + 4;

        // Badge background
        Display::fillCircle(badgeX + 5, badgeY + 5, 7, Theme::RED);

        // Badge text
        char badgeStr[4];
        if (badge > 9) {
            strcpy(badgeStr, "9+");
        } else {
            snprintf(badgeStr, sizeof(badgeStr), "%d", badge);
        }
        Display::drawTextCentered(badgeX - 1, badgeY + 2, 12, badgeStr, Theme::WHITE, 1);
    }
}

bool HomeScreen::handleInput(const InputData& input) {
    int row = getRow(_selectedItem);
    int col = getCol(_selectedItem);
    HomeMenuItem prevSelected = _selectedItem;

    switch (input.event) {
        case InputEvent::TRACKBALL_UP:
            if (row > 0) {
                _selectedItem = (HomeMenuItem)getIndex(row - 1, col);
            }
            break;

        case InputEvent::TRACKBALL_DOWN:
            if (row < ROWS - 1) {
                int newIndex = getIndex(row + 1, col);
                if (newIndex < HOME_ITEM_COUNT) {
                    _selectedItem = (HomeMenuItem)newIndex;
                }
            }
            break;

        case InputEvent::TRACKBALL_LEFT:
            if (col > 0) {
                _selectedItem = (HomeMenuItem)getIndex(row, col - 1);
            } else if (row > 0) {
                // Wrap to end of previous row
                _selectedItem = (HomeMenuItem)getIndex(row - 1, COLS - 1);
            }
            break;

        case InputEvent::TRACKBALL_RIGHT:
            if (col < COLS - 1) {
                int newIndex = getIndex(row, col + 1);
                if (newIndex < HOME_ITEM_COUNT) {
                    _selectedItem = (HomeMenuItem)newIndex;
                }
            } else if (row < ROWS - 1) {
                // Wrap to start of next row
                int newIndex = getIndex(row + 1, 0);
                if (newIndex < HOME_ITEM_COUNT) {
                    _selectedItem = (HomeMenuItem)newIndex;
                }
            }
            break;

        case InputEvent::TRACKBALL_CLICK:
        case InputEvent::KEY_PRESS:
            if (input.event == InputEvent::TRACKBALL_CLICK ||
                input.keyChar == '\n' || input.keyCode == KEY_ENTER) {
                // Open selected item
                ScreenId target = getScreenForItem(_selectedItem);
                if (target != ScreenId::NONE) {
                    Screens.navigateTo(target);
                    return true;
                }
            }
            // Handle shortcut keys
            if (input.event == InputEvent::KEY_PRESS) {
                char c = input.keyChar;
                if (c == 'm' || c == 'M') {
                    Screens.navigateTo(ScreenId::MESSAGES);
                    return true;
                } else if (c == 'c' || c == 'C') {
                    Screens.navigateTo(ScreenId::CONTACTS);
                    return true;
                } else if (c == 's' || c == 'S') {
                    Screens.navigateTo(ScreenId::SETTINGS);
                    return true;
                }
            }
            break;

        case InputEvent::BACK:
            // Can't go back from home
            return false;

        default:
            return false;
    }

    // Check if selection changed
    if (_selectedItem != prevSelected) {
        requestRedraw();
        return true;
    }

    return false;
}

void HomeScreen::update(uint32_t deltaMs) {
    // No animations currently
}

void HomeScreen::setBadge(HomeMenuItem item, uint8_t count) {
    if (item >= 0 && item < HOME_ITEM_COUNT) {
        if (_badges[item] != count) {
            _badges[item] = count;
            requestRedraw();
        }
    }
}

ScreenId HomeScreen::getScreenForItem(HomeMenuItem item) const {
    if (item >= 0 && item < HOME_ITEM_COUNT) {
        return menuItems[item].targetScreen;
    }
    return ScreenId::NONE;
}
