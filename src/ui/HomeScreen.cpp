/**
 * MeshBerry Home Screen Implementation
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2026 NodakMesh (nodakmesh.org)
 *
 * BlackBerry-inspired home screen with background image and bottom dock
 */

#include "HomeScreen.h"
#include "SoftKeyBar.h"
#include "Icons.h"
#include "Icons32.h"
#include "HomeBg.h"
#include "../drivers/display.h"
#include "../drivers/keyboard.h"

// Menu item configuration
struct MenuItemConfig {
    const char* label;
    const uint8_t* icon16;
    const uint8_t* icon32;
    ScreenId targetScreen;
};

static const MenuItemConfig menuItems[HOME_ITEM_COUNT] = {
    { "Messages",  Icons::MSG_ICON,       Icons32::MSG_ICON,      ScreenId::MESSAGES },
    { "Contacts",  Icons::CONTACTS_ICON,  Icons32::CONTACTS_ICON, ScreenId::CONTACTS },
    { "Settings",  Icons::SETTINGS_ICON,  Icons32::SETTINGS_ICON, ScreenId::SETTINGS },
    { "Map",       Icons::MAP_ICON,       Icons32::MAP_ICON,      ScreenId::GPS }  // Placeholder - will be map screen
};

void HomeScreen::onEnter() {
    requestRedraw();
}

void HomeScreen::onExit() {
}

void HomeScreen::configureSoftKeys() {
    // No soft keys on home screen - hide the bar
    SoftKeyBar::setLabels(nullptr, nullptr, nullptr);
}

void HomeScreen::draw(bool fullRedraw) {
    if (fullRedraw) {
        // Draw background image
        drawBackground();

        // Draw dock (bottom)
        drawDock(true);

        _prevSelectedItem = _selectedItem;
    } else {
        // Partial update - only redraw changed dock items
        if (_selectedItem != _prevSelectedItem) {
            drawDockItem(_prevSelectedItem, false);
            drawDockItem(_selectedItem, true);
            _prevSelectedItem = _selectedItem;
        }
    }
}

void HomeScreen::drawBackground() {
    // Check if we have a real background image
    if (HomeBg::HAS_IMAGE) {
        // Draw the RGB565 background image
        Display::drawRGB565(0, BG_START_Y, HomeBg::IMAGE, HomeBg::WIDTH, HomeBg::HEIGHT);
    } else {
        // Fallback: solid dark background with MeshBerry branding
        Display::fillRect(0, BG_START_Y, Theme::SCREEN_WIDTH, BG_HEIGHT, Theme::BG_PRIMARY);

        // Center "MeshBerry" text
        Display::drawTextCentered(0, BG_START_Y + BG_HEIGHT / 2 - 16,
                                  Theme::SCREEN_WIDTH, "MeshBerry", Theme::ACCENT_PRIMARY, 2);
        Display::drawTextCentered(0, BG_START_Y + BG_HEIGHT / 2 + 8,
                                  Theme::SCREEN_WIDTH, "Off-Grid Messenger", Theme::TEXT_SECONDARY, 1);
    }
}

void HomeScreen::drawDock(bool fullRedraw) {
    // Dock background - slightly elevated, at very bottom
    Display::fillRect(0, DOCK_Y, Theme::SCREEN_WIDTH, DOCK_HEIGHT, Theme::BG_ELEVATED);

    // Separator line at top of dock
    Display::drawHLine(0, DOCK_Y, Theme::SCREEN_WIDTH, Theme::DIVIDER);

    // Draw each dock item
    for (int i = 0; i < DOCK_ITEM_COUNT; i++) {
        drawDockItem(i, i == _selectedItem);
    }
}

void HomeScreen::drawDockItem(int dockIndex, bool selected) {
    if (dockIndex < 0 || dockIndex >= DOCK_ITEM_COUNT) return;

    const MenuItemConfig& item = menuItems[dockIndex];

    // Calculate position (4 items across)
    int16_t itemWidth = Theme::SCREEN_WIDTH / DOCK_ITEM_COUNT;  // 80px each
    int16_t x = dockIndex * itemWidth;
    int16_t y = DOCK_Y;

    // Clear item area
    Display::fillRect(x, y + 1, itemWidth, DOCK_HEIGHT - 1, Theme::BG_ELEVATED);

    // Selection highlight - BB style (subtle background)
    if (selected) {
        Display::fillRoundRect(x + 2, y + 2, itemWidth - 4, DOCK_HEIGHT - 4,
                               Theme::RADIUS_SMALL, Theme::ACCENT_PRESSED);
        Display::drawRoundRect(x + 2, y + 2, itemWidth - 4, DOCK_HEIGHT - 4,
                               Theme::RADIUS_SMALL, Theme::ACCENT_PRIMARY);
    }

    // Icon (16x16, centered in 30px height)
    int16_t iconX = x + (itemWidth - 16) / 2;
    int16_t iconY = y + (DOCK_HEIGHT - 16) / 2;  // Center vertically
    uint16_t iconColor = selected ? Theme::WHITE : Theme::TEXT_PRIMARY;
    Display::drawBitmap(iconX, iconY, item.icon16, 16, 16, iconColor);

    // Badge (if any) - positioned at top-right of icon
    uint8_t badge = _badges[dockIndex];
    if (badge > 0) {
        int16_t badgeX = iconX + 12;
        int16_t badgeY = iconY - 4;
        char badgeStr[4];
        int badgeWidth = (badge > 9) ? 16 : 12;
        if (badge > 9) {
            strcpy(badgeStr, "9+");
        } else {
            snprintf(badgeStr, sizeof(badgeStr), "%d", badge);
        }
        Display::fillRoundRect(badgeX, badgeY, badgeWidth, 10, 5, Theme::ERROR);
        Display::drawTextCentered(badgeX, badgeY + 1, badgeWidth, badgeStr, Theme::WHITE, 1);
    }
}

bool HomeScreen::handleInput(const InputData& input) {
    HomeMenuItem prevSelected = _selectedItem;

    switch (input.event) {
        case InputEvent::TRACKBALL_LEFT:
            if (_selectedItem > 0) {
                _selectedItem = (HomeMenuItem)(_selectedItem - 1);
            }
            break;

        case InputEvent::TRACKBALL_RIGHT:
            if (_selectedItem < DOCK_ITEM_COUNT - 1) {
                _selectedItem = (HomeMenuItem)(_selectedItem + 1);
            }
            break;

        case InputEvent::TRACKBALL_UP:
        case InputEvent::TRACKBALL_DOWN:
            // No list items, so up/down do nothing
            break;

        case InputEvent::TRACKBALL_CLICK:
        case InputEvent::SOFTKEY_CENTER:
            {
                ScreenId target = getScreenForItem(_selectedItem);
                if (target != ScreenId::NONE) {
                    Screens.navigateTo(target);
                    return true;
                }
            }
            break;

        case InputEvent::KEY_PRESS:
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
            return false;

        case InputEvent::TOUCH_TAP:
            {
                int16_t tx = input.touchX;
                int16_t ty = input.touchY;

                // Check if tap is in dock area
                if (ty >= DOCK_Y) {
                    int itemWidth = Theme::SCREEN_WIDTH / DOCK_ITEM_COUNT;
                    int dockIdx = tx / itemWidth;
                    if (dockIdx >= 0 && dockIdx < DOCK_ITEM_COUNT) {
                        _selectedItem = (HomeMenuItem)dockIdx;
                        requestRedraw();
                        ScreenId target = getScreenForItem(_selectedItem);
                        if (target != ScreenId::NONE) {
                            Screens.navigateTo(target);
                            return true;
                        }
                    }
                }
            }
            return true;

        default:
            return false;
    }

    if (_selectedItem != prevSelected) {
        requestRedraw();
        return true;
    }

    return false;
}

void HomeScreen::update(uint32_t deltaMs) {
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
