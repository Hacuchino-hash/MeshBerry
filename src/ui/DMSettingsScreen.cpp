/**
 * MeshBerry DM Settings Screen Implementation
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2026 NodakMesh (nodakmesh.org)
 */

#include "DMSettingsScreen.h"
#include "SoftKeyBar.h"
#include "Icons.h"
#include "../drivers/display.h"
#include "../settings/SettingsManager.h"
#include <Arduino.h>
#include <stdio.h>
#include <string.h>

DMSettingsScreen::DMSettingsScreen() {
    memset(_contactName, 0, sizeof(_contactName));
    memset(_menuItems, 0, sizeof(_menuItems));
    memset(_valueStrings, 0, sizeof(_valueStrings));
}

void DMSettingsScreen::setContact(uint32_t contactId) {
    _contactId = contactId;

    // Look up contact name
    ContactSettings& contacts = SettingsManager::getContactSettings();
    int idx = contacts.findContact(contactId);
    if (idx >= 0) {
        const ContactEntry* c = contacts.getContact(idx);
        if (c) {
            strncpy(_contactName, c->name, sizeof(_contactName) - 1);
            _contactName[sizeof(_contactName) - 1] = '\0';
        } else {
            snprintf(_contactName, sizeof(_contactName), "Node %08X", contactId);
        }
    } else {
        snprintf(_contactName, sizeof(_contactName), "Node %08X", contactId);
    }
}

void DMSettingsScreen::onEnter() {
    _selectingRepeater = false;
    buildMenu();
    _listView.setSelectedIndex(0);
    requestRedraw();
}

void DMSettingsScreen::onExit() {
    // Save contacts when leaving
    SettingsManager::saveContacts();
}

const char* DMSettingsScreen::getTitle() const {
    return "DM Settings";
}

void DMSettingsScreen::configureSoftKeys() {
    if (_selectingRepeater) {
        SoftKeyBar::setLabels("Select", nullptr, "Cancel");
    } else {
        SoftKeyBar::setLabels("Change", nullptr, "Back");
    }
}

const char* DMSettingsScreen::getRoutingModeName(DMRoutingMode mode) const {
    switch (mode) {
        case DM_ROUTE_AUTO:   return "Auto";
        case DM_ROUTE_FLOOD:  return "Flood";
        case DM_ROUTE_DIRECT: return "Direct";
        case DM_ROUTE_MANUAL: return "Manual";
        default:              return "Unknown";
    }
}

void DMSettingsScreen::buildMenu() {
    _menuItemCount = 0;

    ContactSettings& contacts = SettingsManager::getContactSettings();
    int idx = contacts.findContact(_contactId);
    if (idx < 0) return;

    ContactEntry* c = contacts.getContact(idx);
    if (!c) return;

    // Item 0: Routing Mode
    snprintf(_valueStrings[0], sizeof(_valueStrings[0]), "%s", getRoutingModeName(c->routingMode));
    _menuItems[0] = {
        "Routing Mode",
        _valueStrings[0],
        Icons::SETTINGS_ICON,
        Theme::ACCENT,
        false,
        0,
        nullptr
    };
    _menuItemCount++;

    // Item 1: Current Path Info
    if (c->outPathLen >= 0) {
        snprintf(_valueStrings[1], sizeof(_valueStrings[1]), "%d hops", c->outPathLen);
    } else {
        snprintf(_valueStrings[1], sizeof(_valueStrings[1]), "Unknown");
    }
    _menuItems[1] = {
        "Learned Path",
        _valueStrings[1],
        Icons::REPEATER_ICON,
        Theme::GREEN,
        c->outPathLen >= 0,
        Theme::GREEN,
        nullptr
    };
    _menuItemCount++;

    // Item 2: Clear Learned Path (only if path exists)
    if (c->outPathLen >= 0) {
        _menuItems[2] = {
            "Clear Path",
            "Force re-learn",
            Icons::SETTINGS_ICON,
            Theme::RED,
            false,
            0,
            nullptr
        };
        _menuItemCount++;
    }

    // Item 3: Manual Path (if routing mode is MANUAL)
    if (c->routingMode == DM_ROUTE_MANUAL) {
        if (c->manualPathLen > 0) {
            snprintf(_valueStrings[3], sizeof(_valueStrings[3]), "%d repeaters", c->manualPathLen);
        } else {
            snprintf(_valueStrings[3], sizeof(_valueStrings[3]), "Not set");
        }
        _menuItems[_menuItemCount] = {
            "Manual Path",
            _valueStrings[3],
            Icons::REPEATER_ICON,
            Theme::ACCENT,
            false,
            0,
            nullptr
        };
        _menuItemCount++;

        // Add individual repeater slots
        for (int i = 0; i < 3; i++) {  // Show up to 3 hop slots
            int slotIdx = _menuItemCount;
            if (i < c->manualPathLen) {
                // Look up repeater name by ID
                uint32_t repId = c->manualPath[i];
                int repIdx = contacts.findContact(repId);
                if (repIdx >= 0) {
                    const ContactEntry* rep = contacts.getContact(repIdx);
                    if (rep) {
                        snprintf(_valueStrings[slotIdx], sizeof(_valueStrings[slotIdx]), "%s", rep->name);
                    } else {
                        snprintf(_valueStrings[slotIdx], sizeof(_valueStrings[slotIdx]), "ID %08X", repId);
                    }
                } else {
                    snprintf(_valueStrings[slotIdx], sizeof(_valueStrings[slotIdx]), "ID %08X", repId);
                }
            } else {
                snprintf(_valueStrings[slotIdx], sizeof(_valueStrings[slotIdx]), "(empty)");
            }

            char hopLabel[16];
            snprintf(hopLabel, sizeof(hopLabel), "Hop %d", i + 1);
            _menuItems[_menuItemCount] = {
                hopLabel,
                _valueStrings[slotIdx],
                Icons::REPEATER_ICON,
                (i < c->manualPathLen) ? Theme::GREEN : Theme::GRAY_MID,
                false,
                0,
                (void*)(intptr_t)i  // Store hop index
            };
            _menuItemCount++;
        }
    }

    _listView.setItems(_menuItems, _menuItemCount);
    _listView.setBounds(0, Theme::CONTENT_Y + 50, Theme::SCREEN_WIDTH, Theme::CONTENT_HEIGHT - 50);
}

void DMSettingsScreen::drawHeader() {
    // Header background
    Display::fillRect(0, Theme::CONTENT_Y, Theme::SCREEN_WIDTH, 50, Theme::BG_SECONDARY);

    // Contact icon and name
    Display::drawBitmap(8, Theme::CONTENT_Y + 8, Icons::CONTACTS_ICON, 16, 16, Theme::GREEN);
    Display::drawText(32, Theme::CONTENT_Y + 12, _contactName, Theme::WHITE, 1);

    // Contact ID
    char idStr[16];
    snprintf(idStr, sizeof(idStr), "ID: %08X", _contactId);
    Display::drawText(32, Theme::CONTENT_Y + 28, idStr, Theme::GRAY_LIGHT, 1);

    // Divider
    Display::drawHLine(0, Theme::CONTENT_Y + 49, Theme::SCREEN_WIDTH, Theme::DIVIDER);
}

void DMSettingsScreen::drawPathInfo() {
    // Path info is now part of the menu items
}

void DMSettingsScreen::draw(bool fullRedraw) {
    if (fullRedraw) {
        // Clear content area
        Display::fillRect(0, Theme::CONTENT_Y, Theme::SCREEN_WIDTH,
                          Theme::CONTENT_HEIGHT, Theme::BG_PRIMARY);

        drawHeader();
    }

    _listView.draw(fullRedraw);
}

void DMSettingsScreen::onItemSelected(int index) {
    ContactSettings& contacts = SettingsManager::getContactSettings();
    int idx = contacts.findContact(_contactId);
    if (idx < 0) return;

    ContactEntry* c = contacts.getContact(idx);
    if (!c) return;

    if (index == 0) {
        // Cycle routing mode
        switch (c->routingMode) {
            case DM_ROUTE_AUTO:
                c->routingMode = DM_ROUTE_FLOOD;
                break;
            case DM_ROUTE_FLOOD:
                c->routingMode = DM_ROUTE_DIRECT;
                break;
            case DM_ROUTE_DIRECT:
                c->routingMode = DM_ROUTE_MANUAL;
                break;
            case DM_ROUTE_MANUAL:
                c->routingMode = DM_ROUTE_AUTO;
                break;
        }
        Serial.printf("[DM] Routing mode changed to %s for %s\n",
                      getRoutingModeName(c->routingMode), c->name);
        buildMenu();
        requestRedraw();
    }
    else if (index == 1) {
        // Learned path - just info, no action
    }
    else if (index == 2 && c->outPathLen >= 0) {
        // Clear learned path
        memset(c->outPath, 0, sizeof(c->outPath));
        c->outPathLen = -1;
        c->pathLearnedAt = 0;
        Serial.printf("[DM] Cleared learned path for %s\n", c->name);
        buildMenu();
        requestRedraw();
    }
    else if (c->routingMode == DM_ROUTE_MANUAL && index >= 3) {
        // Manual path hop selection
        int hopIdx = index - 4;  // Skip routing mode, learned path, clear path, manual path header
        if (hopIdx >= 0 && hopIdx < 3) {
            _selectingRepeater = true;
            _selectedRepeaterSlot = hopIdx;
            configureSoftKeys();
            SoftKeyBar::redraw();

            // TODO: Show repeater picker
            // For now, just cycle through available repeaters
            int repIndices[16];
            int repCount = contacts.getContactsByType(NODE_TYPE_REPEATER, repIndices, 16);

            if (repCount > 0) {
                // Find current repeater in list (if any)
                int currentRepIdx = -1;
                if (hopIdx < c->manualPathLen) {
                    for (int i = 0; i < repCount; i++) {
                        const ContactEntry* rep = contacts.getContact(repIndices[i]);
                        if (rep && rep->id == c->manualPath[hopIdx]) {
                            currentRepIdx = i;
                            break;
                        }
                    }
                }

                // Cycle to next repeater
                int nextRepIdx = (currentRepIdx + 1) % (repCount + 1);  // +1 for "none"

                if (nextRepIdx < repCount) {
                    const ContactEntry* newRep = contacts.getContact(repIndices[nextRepIdx]);
                    if (newRep) {
                        c->manualPath[hopIdx] = newRep->id;
                        if (hopIdx >= c->manualPathLen) {
                            c->manualPathLen = hopIdx + 1;
                        }
                        Serial.printf("[DM] Set hop %d to %s\n", hopIdx + 1, newRep->name);
                    }
                } else {
                    // Clear this hop (and all after it)
                    for (int i = hopIdx; i < 8; i++) {
                        c->manualPath[i] = 0;
                    }
                    c->manualPathLen = hopIdx;
                    Serial.printf("[DM] Cleared hop %d and beyond\n", hopIdx + 1);
                }
            }

            _selectingRepeater = false;
            configureSoftKeys();
            SoftKeyBar::redraw();
            buildMenu();
            requestRedraw();
        }
    }
}

bool DMSettingsScreen::handleInput(const InputData& input) {
    switch (input.event) {
        case InputEvent::TRACKBALL_UP:
        case InputEvent::TRACKBALL_DOWN:
        case InputEvent::TRACKBALL_CLICK:
            if (_listView.handleTrackball(
                    input.event == InputEvent::TRACKBALL_UP,
                    input.event == InputEvent::TRACKBALL_DOWN,
                    false, false,
                    input.event == InputEvent::TRACKBALL_CLICK)) {
                if (input.event == InputEvent::TRACKBALL_CLICK) {
                    onItemSelected(_listView.getSelectedIndex());
                }
                if (_listView.needsRedraw()) {
                    requestRedraw();
                }
                return true;
            }
            break;

        case InputEvent::SOFTKEY_LEFT:
            // Change selected item
            onItemSelected(_listView.getSelectedIndex());
            return true;

        case InputEvent::SOFTKEY_RIGHT:
        case InputEvent::BACK:
            Screens.goBack();
            return true;

        case InputEvent::TOUCH_TAP:
            // Handle touch on soft key bar
            if (input.touchY >= Theme::SOFTKEY_BAR_Y) {
                if (input.touchX >= 214) {
                    Screens.goBack();
                    return true;
                } else if (input.touchX < 107) {
                    onItemSelected(_listView.getSelectedIndex());
                    return true;
                }
            }
            // Handle touch on list
            else if (input.touchY >= Theme::CONTENT_Y + 50) {
                int itemIdx = (input.touchY - _listView.getY()) / _listView.getItemHeight() + _listView.getScrollOffset();
                if (itemIdx >= 0 && itemIdx < _listView.getItemCount()) {
                    _listView.setSelectedIndex(itemIdx);
                    onItemSelected(itemIdx);
                    requestRedraw();
                    return true;
                }
            }
            break;

        case InputEvent::TOUCH_DRAG:
            if (_listView.handleTouchDrag(input.dragDeltaY)) {
                requestRedraw();
                return true;
            }
            break;

        default:
            break;
    }

    return false;
}
