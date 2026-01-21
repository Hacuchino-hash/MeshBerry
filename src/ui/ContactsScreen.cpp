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
    SoftKeyBar::setLabels("Fav", "View", "Back");
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

void ContactsScreen::addRepeaterToList(const ContactEntry* c, int originalIdx) {
    strncpy(_primaryStrings[_contactCount], c->name, sizeof(_primaryStrings[0]) - 1);
    _primaryStrings[_contactCount][sizeof(_primaryStrings[0]) - 1] = '\0';

    char timeStr[16];
    formatTimeAgo(c->lastHeard, timeStr, sizeof(timeStr));

    snprintf(_secondaryStrings[_contactCount], sizeof(_secondaryStrings[0]) - 1,
             "%ddBm | %s", c->lastRssi, timeStr);
    _secondaryStrings[_contactCount][sizeof(_secondaryStrings[0]) - 1] = '\0';

    _contactItems[_contactCount] = {
        _primaryStrings[_contactCount],
        _secondaryStrings[_contactCount],
        Icons::REPEATER_ICON,
        Theme::GREEN,
        c->isFavorite,
        Theme::YELLOW,
        (void*)(intptr_t)originalIdx
    };
    _contactCount++;
}

void ContactsScreen::addContactToList(const ContactEntry* c, int originalIdx) {
    strncpy(_primaryStrings[_contactCount], c->name, sizeof(_primaryStrings[0]) - 1);
    _primaryStrings[_contactCount][sizeof(_primaryStrings[0]) - 1] = '\0';

    const char* typeStr = "Chat";
    uint16_t typeColor = Theme::WHITE;
    const uint8_t* icon = Icons::CONTACTS_ICON;

    switch (c->type) {
        case NODE_TYPE_ROOM:
            typeStr = "Room";
            typeColor = Theme::YELLOW;
            break;
        case NODE_TYPE_SENSOR:
            typeStr = "Sensor";
            typeColor = Theme::ACCENT;
            break;
        default:
            break;
    }

    char timeStr[16];
    formatTimeAgo(c->lastHeard, timeStr, sizeof(timeStr));

    snprintf(_secondaryStrings[_contactCount], sizeof(_secondaryStrings[0]) - 1,
             "%s | %ddBm | %s", typeStr, c->lastRssi, timeStr);
    _secondaryStrings[_contactCount][sizeof(_secondaryStrings[0]) - 1] = '\0';

    _contactItems[_contactCount] = {
        _primaryStrings[_contactCount],
        _secondaryStrings[_contactCount],
        icon,
        typeColor,
        c->isFavorite,
        Theme::YELLOW,
        (void*)(intptr_t)originalIdx
    };
    _contactCount++;
}

void ContactsScreen::buildContactList() {
    ContactSettings& contacts = SettingsManager::getContactSettings();
    _contactCount = 0;
    _repeaterCount = 0;
    _regularContactCount = 0;
    _repeaterHeaderIdx = -1;
    _contactsHeaderIdx = -1;

    // First pass: count each type
    for (int i = 0; i < contacts.numContacts; i++) {
        const ContactEntry* c = contacts.getContact(i);
        if (!c || !c->isActive) continue;
        if (c->type == NODE_TYPE_REPEATER) {
            _repeaterCount++;
        } else {
            _regularContactCount++;
        }
    }

    // Add "Contacts" section header if we have any (clients first)
    if (_regularContactCount > 0) {
        _contactsHeaderIdx = _contactCount;
        snprintf(_primaryStrings[_contactCount], sizeof(_primaryStrings[0]),
                 "-- Contacts (%d) --", _regularContactCount);
        _secondaryStrings[_contactCount][0] = '\0';
        _contactItems[_contactCount] = {
            _primaryStrings[_contactCount],
            nullptr,
            nullptr,
            Theme::GRAY_LIGHT,
            false,
            0,
            (void*)(intptr_t)-1
        };
        _contactCount++;

        // Add favorite contacts first
        for (int i = 0; i < contacts.numContacts && _contactCount < MAX_CONTACTS; i++) {
            const ContactEntry* c = contacts.getContact(i);
            if (!c || !c->isActive) continue;
            if (c->type == NODE_TYPE_REPEATER) continue;
            if (!c->isFavorite) continue;  // Favorites first
            addContactToList(c, i);
        }

        // Add non-favorite contacts
        for (int i = 0; i < contacts.numContacts && _contactCount < MAX_CONTACTS; i++) {
            const ContactEntry* c = contacts.getContact(i);
            if (!c || !c->isActive) continue;
            if (c->type == NODE_TYPE_REPEATER) continue;
            if (c->isFavorite) continue;  // Skip favorites (already added)
            addContactToList(c, i);
        }
    }

    // Add "Repeaters" section header if we have any (below contacts)
    if (_repeaterCount > 0) {
        _repeaterHeaderIdx = _contactCount;
        snprintf(_primaryStrings[_contactCount], sizeof(_primaryStrings[0]),
                 "-- Repeaters (%d) --", _repeaterCount);
        _secondaryStrings[_contactCount][0] = '\0';
        _contactItems[_contactCount] = {
            _primaryStrings[_contactCount],
            nullptr,
            nullptr,
            Theme::GRAY_LIGHT,
            false,
            0,
            (void*)(intptr_t)-1
        };
        _contactCount++;

        // Add favorite repeaters first
        for (int i = 0; i < contacts.numContacts && _contactCount < MAX_CONTACTS; i++) {
            const ContactEntry* c = contacts.getContact(i);
            if (!c || !c->isActive) continue;
            if (c->type != NODE_TYPE_REPEATER) continue;
            if (!c->isFavorite) continue;  // Favorites first
            addRepeaterToList(c, i);
        }

        // Add non-favorite repeaters
        for (int i = 0; i < contacts.numContacts && _contactCount < MAX_CONTACTS; i++) {
            const ContactEntry* c = contacts.getContact(i);
            if (!c || !c->isActive) continue;
            if (c->type != NODE_TYPE_REPEATER) continue;
            if (c->isFavorite) continue;  // Skip favorites (already added)
            addRepeaterToList(c, i);
        }
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
    // Left soft key: "Fav" - toggle favorite on selected contact
    if (input.event == InputEvent::SOFTKEY_LEFT) {
        if (_contactCount > 0) {
            int idx = _listView.getSelectedIndex();
            int originalIdx = (int)(intptr_t)_contactItems[idx].userData;
            if (originalIdx >= 0) {  // Not a header
                ContactSettings& contacts = SettingsManager::getContactSettings();
                contacts.toggleFavorite(originalIdx);
                SettingsManager::saveContacts();
                buildContactList();  // Rebuild to re-sort
                requestRedraw();
            }
        }
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
    bool movingUp = (input.event == InputEvent::TRACKBALL_UP);
    bool movingDown = (input.event == InputEvent::TRACKBALL_DOWN);
    if (_listView.handleTrackball(movingUp, movingDown, false, false, false)) {
        // Skip section headers when navigating
        int idx = _listView.getSelectedIndex();
        int userData = (int)(intptr_t)_contactItems[idx].userData;
        if (userData == -1) {
            // On a header, move to next valid item
            if (movingDown && idx + 1 < _contactCount) {
                _listView.setSelectedIndex(idx + 1);
            } else if (movingUp && idx > 0) {
                _listView.setSelectedIndex(idx - 1);
            }
        }
        requestRedraw();
        return true;
    }

    // Handle touch tap
    if (input.event == InputEvent::TOUCH_TAP) {
        int16_t ty = input.touchY;
        int16_t tx = input.touchX;

        // Soft key bar touch (Y >= 210)
        if (ty >= Theme::SOFTKEY_BAR_Y) {
            if (tx >= 214) {
                // Right soft key = Back
                Screens.goBack();
            } else if (tx >= 107) {
                // Center soft key = View
                if (_contactCount > 0) {
                    onContactSelected(_listView.getSelectedIndex());
                }
            } else {
                // Left soft key = Fav toggle
                if (_contactCount > 0) {
                    int idx = _listView.getSelectedIndex();
                    int originalIdx = (int)(intptr_t)_contactItems[idx].userData;
                    if (originalIdx >= 0) {
                        ContactSettings& contacts = SettingsManager::getContactSettings();
                        contacts.toggleFavorite(originalIdx);
                        SettingsManager::saveContacts();
                        buildContactList();
                        requestRedraw();
                    }
                }
            }
            return true;
        }

        // List touch - select and open contact
        int16_t listTop = Theme::CONTENT_Y + 30;
        if (ty >= listTop && _contactCount > 0) {
            int itemIndex = _listView.getScrollOffset() + (ty - listTop) / 44;
            if (itemIndex >= 0 && itemIndex < _contactCount) {
                // Skip headers - don't select or open them
                int userData = (int)(intptr_t)_contactItems[itemIndex].userData;
                if (userData != -1) {
                    _listView.setSelectedIndex(itemIndex);
                    onContactSelected(itemIndex);  // Open directly on tap
                }
            }
        }
        return true;
    }

    return false;
}

void ContactsScreen::onContactSelected(int index) {
    if (index < 0 || index >= _contactCount) return;

    // Get the original contact index from stored data
    int originalIdx = (int)(intptr_t)_contactItems[index].userData;

    // Skip section headers (marked with -1)
    if (originalIdx == -1) return;

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
