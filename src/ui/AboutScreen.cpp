/**
 * MeshBerry About Screen Implementation
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2026 NodakMesh (nodakmesh.org)
 */

#include "AboutScreen.h"
#include "SoftKeyBar.h"
#include "Icons.h"
#include "../drivers/display.h"
#include "../drivers/keyboard.h"
#include "../config.h"

AboutScreen::AboutScreen() {
    _listView.setBounds(0, Theme::CONTENT_Y, Theme::SCREEN_WIDTH, Theme::CONTENT_HEIGHT);
    _listView.setItemHeight(40);
}

void AboutScreen::onEnter() {
    // Build menu items
    _menuItemCount = 0;

    _menuItems[0] = { "Version", MESHBERRY_VERSION, nullptr, Theme::ACCENT, false, 0, nullptr };
    _menuItems[1] = { "License", "GPL-3.0-or-later", nullptr, Theme::ACCENT, false, 0, nullptr };
    _menuItems[2] = { "Organization", "NodakMesh", nullptr, Theme::ACCENT, false, 0, nullptr };
    _menuItems[3] = { "Website", "nodakmesh.org", nullptr, Theme::ACCENT, false, 0, nullptr };
    _menuItems[4] = { "Hardware", "LilyGo T-Deck", nullptr, Theme::ACCENT, false, 0, nullptr };
    _menuItems[5] = { "MeshCore", "Open Source Mesh", nullptr, Theme::ACCENT, false, 0, nullptr };
    _menuItemCount = 6;

    _listView.setItems(_menuItems, _menuItemCount);
    _listView.setSelectedIndex(0);
    requestRedraw();
}

void AboutScreen::configureSoftKeys() {
    SoftKeyBar::setLabels(nullptr, nullptr, "Back");
}

void AboutScreen::draw(bool fullRedraw) {
    if (fullRedraw) {
        Display::fillRect(0, Theme::CONTENT_Y,
                          Theme::SCREEN_WIDTH, Theme::CONTENT_HEIGHT,
                          Theme::BG_PRIMARY);

        // Draw title
        Display::drawText(12, Theme::CONTENT_Y + 4, "About MeshBerry", Theme::ACCENT, 2);

        // Divider below title
        Display::drawHLine(12, Theme::CONTENT_Y + 26, Theme::SCREEN_WIDTH - 24, Theme::DIVIDER);
    }

    // Adjust list bounds below title
    _listView.setBounds(0, Theme::CONTENT_Y + 30, Theme::SCREEN_WIDTH, Theme::CONTENT_HEIGHT - 30);
    _listView.draw(fullRedraw);
}

bool AboutScreen::handleInput(const InputData& input) {
    // Handle back navigation
    // Treat backspace as back since this screen has no text input
    bool isBackKey = (input.event == InputEvent::KEY_PRESS && input.keyCode == KEY_BACKSPACE);
    if (input.event == InputEvent::BACK ||
        input.event == InputEvent::TRACKBALL_LEFT ||
        isBackKey) {
        Screens.goBack();
        return true;
    }

    // Let list view handle up/down (for scrolling info)
    if (_listView.handleTrackball(
            input.event == InputEvent::TRACKBALL_UP,
            input.event == InputEvent::TRACKBALL_DOWN,
            false, false,
            false)) {
        requestRedraw();
        return true;
    }

    return false;
}
