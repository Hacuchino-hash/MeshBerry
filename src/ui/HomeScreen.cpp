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
// Note: Repeaters removed - ContactsScreen now has separate sections
static const MenuItemConfig menuItems[HOME_ITEM_COUNT] = {
    { "Messages",  Icons::MSG_ICON,       ScreenId::MESSAGES },
    { "Contacts",  Icons::CONTACTS_ICON,  ScreenId::CONTACTS },
    { "Settings",  Icons::SETTINGS_ICON,  ScreenId::SETTINGS },
    { "Status",    Icons::INFO_ICON,      ScreenId::STATUS },
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
        _prevSelectedItem = _selectedItem;
    } else {
        // Partial update - only redraw tiles that changed selection
        if (_selectedItem != _prevSelectedItem) {
            // Redraw previously selected (now deselected)
            drawTile(_prevSelectedItem, false);
            // Redraw newly selected
            drawTile(_selectedItem, true);
            _prevSelectedItem = _selectedItem;
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

    // Modern card background with gradient and shadow
    uint16_t bgColorTop, bgColorBottom;
    if (selected) {
        // Selected: Blue gradient with shadow
        bgColorTop = Theme::COLOR_PRIMARY_LIGHT;
        bgColorBottom = Theme::COLOR_PRIMARY_DARK;
        // Draw shadow first
        Display::drawShadow(x, y, TILE_WIDTH, TILE_HEIGHT, 4, 50);
    } else {
        // Normal: Subtle gray gradient
        bgColorTop = Theme::COLOR_BG_ELEVATED;
        bgColorBottom = Theme::COLOR_BG_CARD;
        // Subtle shadow
        Display::drawShadow(x, y, TILE_WIDTH, TILE_HEIGHT, 2, 30);
    }

    // Draw gradient background
    Display::fillGradient(x, y, TILE_WIDTH, TILE_HEIGHT, bgColorTop, bgColorBottom);

    // Draw card border
    if (selected) {
        Display::drawRoundRect(x, y, TILE_WIDTH, TILE_HEIGHT, Theme::CARD_RADIUS, Theme::COLOR_PRIMARY);
    } else {
        Display::drawRoundRect(x, y, TILE_WIDTH, TILE_HEIGHT, Theme::CARD_RADIUS, Theme::COLOR_BORDER);
    }

    // Icon (centered horizontally, larger and with background circle)
    int16_t iconX = x + (TILE_WIDTH - 16) / 2;
    int16_t iconY = y + 12;
    uint16_t iconBgColor = selected ? Theme::COLOR_PRIMARY : Theme::COLOR_BG_INPUT;
    uint16_t iconColor = selected ? Theme::WHITE : Theme::COLOR_PRIMARY_LIGHT;

    // Draw icon with circular background (14px radius to fully cover 16x16 icon)
    Display::fillCircle(iconX + 8, iconY + 8, 14, iconBgColor);
    Display::drawBitmapBg(iconX, iconY, item.icon, 16, 16, iconColor, iconBgColor);

    // Label (centered below icon, larger text)
    int16_t labelY = y + 38;
    uint16_t textColor = selected ? Theme::WHITE : Theme::COLOR_TEXT_PRIMARY;

    // Truncate label if needed
    const char* label = Theme::truncateText(item.label, TILE_WIDTH - 8, Theme::FONT_MEDIUM);
    Display::drawTextCentered(x, labelY, TILE_WIDTH, label, textColor, Theme::FONT_MEDIUM);

    // Status text (below label) - shows badge count or status
    int16_t statusY = y + 55;
    uint16_t statusColor = selected ? Theme::COLOR_TEXT_SECONDARY : Theme::COLOR_TEXT_HINT;

    char statusText[16] = "";
    uint8_t badge = _badges[index];
    if (badge > 0) {
        if (badge > 99) {
            snprintf(statusText, sizeof(statusText), "99+ new");
        } else if (badge == 1) {
            snprintf(statusText, sizeof(statusText), "1 new");
        } else {
            snprintf(statusText, sizeof(statusText), "%d new", badge);
        }
    } else {
        // Show helpful status text per tile
        switch (index) {
            case HOME_MESSAGES:
                strcpy(statusText, "No msgs");
                break;
            case HOME_CONTACTS:
                strcpy(statusText, "All nodes");
                break;
            case HOME_SETTINGS:
                strcpy(statusText, "Configure");
                break;
            case HOME_STATUS:
                strcpy(statusText, "System");
                break;
            case HOME_GPS:
                strcpy(statusText, "Location");
                break;
            case HOME_ABOUT:
                strcpy(statusText, "Info");
                break;
        }
    }

    Display::drawTextCentered(x, statusY, TILE_WIDTH, statusText, statusColor, Theme::FONT_SMALL);

    // Badge indicator (small dot in corner if there are notifications)
    if (badge > 0) {
        int16_t dotX = x + TILE_WIDTH - 8;
        int16_t dotY = y + 8;
        Display::fillCircle(dotX, dotY, 4, COLOR_ERROR);  // Use macro from config.h
        // Add white outline for visibility
        Display::drawCircle(dotX, dotY, 4, Theme::WHITE);
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
        case InputEvent::SOFTKEY_CENTER:
            // Open selected item (trackball click or Enter key)
            {
                ScreenId target = getScreenForItem(_selectedItem);
                Serial.printf("[HOME] Open item %d -> screen %d\n",
                              (int)_selectedItem, (int)target);
                if (target != ScreenId::NONE) {
                    Screens.navigateTo(target);
                    return true;
                }
            }
            break;

        case InputEvent::KEY_PRESS:
            // Handle shortcut keys
            {
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

        case InputEvent::TOUCH_TAP:
            // Map touch coordinates to grid tile
            {
                int16_t tx = input.touchX;
                int16_t ty = input.touchY;

                // Ignore touches in soft key bar area (no back from home)
                if (ty >= Theme::SOFTKEY_BAR_Y) {
                    return true;
                }

                // Check if touch is in grid area
                if (ty >= GRID_START_Y && ty < GRID_START_Y + ROWS * (TILE_HEIGHT + TILE_MARGIN)) {
                    // Calculate which row
                    int touchRow = (ty - GRID_START_Y) / (TILE_HEIGHT + TILE_MARGIN);
                    if (touchRow >= ROWS) touchRow = ROWS - 1;

                    // Calculate which column
                    int touchCol = -1;
                    for (int c = 0; c < COLS; c++) {
                        int16_t colX = GRID_START_X + c * (TILE_WIDTH + TILE_MARGIN);
                        if (tx >= colX && tx < colX + TILE_WIDTH) {
                            touchCol = c;
                            break;
                        }
                    }

                    if (touchCol >= 0) {
                        int touchIndex = getIndex(touchRow, touchCol);
                        if (touchIndex >= 0 && touchIndex < HOME_ITEM_COUNT) {
                            // Select and open the touched item
                            _selectedItem = (HomeMenuItem)touchIndex;
                            requestRedraw();

                            ScreenId target = getScreenForItem(_selectedItem);
                            Serial.printf("[HOME] Touch tap at (%d,%d) -> item %d -> screen %d\n",
                                          tx, ty, touchIndex, (int)target);
                            if (target != ScreenId::NONE) {
                                Screens.navigateTo(target);
                                return true;
                            }
                        }
                    }
                }
            }
            return true;  // Consume touch event even if not on a tile

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
