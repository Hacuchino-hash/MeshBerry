/**
 * MeshBerry Contacts Screen Implementation
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2026 NodakMesh (nodakmesh.org)
 */

#include "ContactsScreen.h"
#include "SoftKeyBar.h"
#include "Icons.h"
#include "../drivers/display.h"
#include "../drivers/keyboard.h"
#include "../settings/SettingsManager.h"
#include "RepeaterAdminScreen.h"
#include "DMChatScreen.h"
#include <Arduino.h>
#include <stdio.h>

// Forward declaration - defined in main.cpp
extern RepeaterAdminScreen repeaterAdminScreen;
extern DMChatScreen dmChatScreen;

ContactsScreen::ContactsScreen() {
    _listView.setBounds(0, Theme::CONTENT_Y + 30, Theme::SCREEN_WIDTH, Theme::CONTENT_HEIGHT - 30);
    _listView.setItemHeight(44);
}

void ContactsScreen::onEnter() {
    buildContactList();
    requestRedraw();
}

void ContactsScreen::configureSoftKeys() {
    SoftKeyBar::setLabels("Filter", "View", "Back");
}

void ContactsScreen::formatTimeAgo(uint32_t timestamp, char* buf, size_t bufSize) {
    if (timestamp == 0) {
        snprintf(buf, bufSize, "Unknown");
        return;
    }

    uint32_t now = millis();
    uint32_t diff = (now - timestamp) / 1000;  // Seconds

    if (diff < 60) {
        snprintf(buf, bufSize, "%ds ago", (int)diff);
    } else if (diff < 3600) {
        snprintf(buf, bufSize, "%dm ago", (int)(diff / 60));
    } else if (diff < 86400) {
        snprintf(buf, bufSize, "%dh ago", (int)(diff / 3600));
    } else {
        snprintf(buf, bufSize, "%dd ago", (int)(diff / 86400));
    }
}

void ContactsScreen::buildContactList() {
    ContactSettings& contacts = SettingsManager::getContactSettings();
    _contactCount = 0;

    for (int i = 0; i < contacts.numContacts && _contactCount < MAX_CONTACTS; i++) {
        const ContactEntry* c = contacts.getContact(i);
        if (!c) continue;

        // Copy name
        strncpy(_primaryStrings[_contactCount], c->name, 23);
        _primaryStrings[_contactCount][23] = '\0';

        // Build secondary string with type and signal
        const char* typeStr = "Node";
        uint16_t typeColor = Theme::WHITE;
        const uint8_t* icon = Icons::CONTACTS_ICON;

        switch (c->type) {
            case NODE_TYPE_REPEATER:
                typeStr = "Repeater";
                typeColor = Theme::GREEN;
                icon = Icons::REPEATER_ICON;
                break;
            case NODE_TYPE_ROOM:
                typeStr = "Room";
                typeColor = Theme::YELLOW;
                break;
            case NODE_TYPE_SENSOR:
                typeStr = "Sensor";
                typeColor = Theme::ACCENT;
                break;
            default:
                typeStr = "Chat";
                break;
        }

        char timeStr[16];
        formatTimeAgo(c->lastHeard, timeStr, sizeof(timeStr));

        snprintf(_secondaryStrings[_contactCount], 31, "%s | %ddBm | %s",
                 typeStr, c->lastRssi, timeStr);
        _secondaryStrings[_contactCount][31] = '\0';

        // Build list item
        _contactItems[_contactCount] = {
            _primaryStrings[_contactCount],
            _secondaryStrings[_contactCount],
            icon,
            typeColor,
            c->isFavorite,
            Theme::YELLOW,
            (void*)(intptr_t)i  // Store original index
        };

        _contactCount++;
    }

    _listView.setItems(_contactItems, _contactCount);
}

void ContactsScreen::draw(bool fullRedraw) {
    if (fullRedraw) {
        Display::fillRect(0, Theme::CONTENT_Y,
                          Theme::SCREEN_WIDTH, Theme::CONTENT_HEIGHT,
                          Theme::BG_PRIMARY);

        // Draw title
        Display::drawText(12, Theme::CONTENT_Y + 4, "Contacts", Theme::ACCENT, 2);

        // Contact count
        char countStr[16];
        snprintf(countStr, sizeof(countStr), "%d", _contactCount);
        Display::drawText(Theme::SCREEN_WIDTH - 30, Theme::CONTENT_Y + 8, countStr, Theme::TEXT_SECONDARY, 1);

        // Divider below title
        Display::drawHLine(12, Theme::CONTENT_Y + 26, Theme::SCREEN_WIDTH - 24, Theme::DIVIDER);
    }

    if (_contactCount == 0) {
        Display::drawTextCentered(0, Theme::CONTENT_Y + 80,
                                  Theme::SCREEN_WIDTH,
                                  "No contacts yet", Theme::TEXT_SECONDARY, 1);
        Display::drawTextCentered(0, Theme::CONTENT_Y + 100,
                                  Theme::SCREEN_WIDTH,
                                  "Nodes will appear when discovered", Theme::GRAY_LIGHT, 1);
    } else {
        _listView.draw(fullRedraw);
    }
}

bool ContactsScreen::handleInput(const InputData& input) {
    // Left soft key: "Filter" - cycle through filter modes
    if (input.event == InputEvent::SOFTKEY_LEFT) {
        // TODO: Implement filter (All / Repeaters / Chat nodes / Favorites)
        Screens.showStatus("Filter coming soon", 2000);
        return true;
    }

    // Center soft key: "View" - view selected contact
    if (input.event == InputEvent::SOFTKEY_CENTER ||
        input.event == InputEvent::TRACKBALL_CLICK) {
        if (_contactCount > 0) {
            onContactSelected(_listView.getSelectedIndex());
        }
        return true;
    }

    // Right soft key or Back: go back
    // Treat backspace as back since this screen has no text input
    bool isBackKey = (input.event == InputEvent::KEY_PRESS && input.keyCode == KEY_BACKSPACE);
    if (input.event == InputEvent::SOFTKEY_RIGHT ||
        input.event == InputEvent::BACK ||
        input.event == InputEvent::TRACKBALL_LEFT ||
        isBackKey) {
        Screens.goBack();
        return true;
    }

    // Let list view handle up/down
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

void ContactsScreen::onContactSelected(int index) {
    if (index < 0 || index >= _contactCount) return;

    // Get the original contact index from stored data
    int originalIdx = (int)(intptr_t)_contactItems[index].userData;

    ContactSettings& contacts = SettingsManager::getContactSettings();
    const ContactEntry* contact = contacts.getContact(originalIdx);
    if (!contact) return;

    // If it's a repeater, open the admin screen
    if (contact->type == NODE_TYPE_REPEATER) {
        repeaterAdminScreen.setRepeater(contact->id, contact->pubKey, contact->name);
        Screens.navigateTo(ScreenId::REPEATER_ADMIN);
    } else {
        // For other nodes (chat, room, sensor), open DM chat screen
        dmChatScreen.setContact(contact->id, contact->name);
        Screens.navigateTo(ScreenId::DM_CHAT);
    }
}
