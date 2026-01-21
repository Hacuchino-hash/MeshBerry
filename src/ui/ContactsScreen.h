/**
 * MeshBerry Contacts Screen
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2026 NodakMesh (nodakmesh.org)
 *
 * Node and repeater contact list with BlackBerry style
 */

#ifndef MESHBERRY_CONTACTSSCREEN_H
#define MESHBERRY_CONTACTSSCREEN_H

#include "Screen.h"
#include "ScreenManager.h"
#include "ListView.h"

// Forward declaration
struct ContactEntry;

/**
 * Contacts Screen - Node/repeater list
 */
class ContactsScreen : public Screen {
public:
    ContactsScreen();
    ~ContactsScreen() override = default;

    // Screen interface
    ScreenId getId() const override { return ScreenId::CONTACTS; }
    void onEnter() override;
    void onExit() override {}
    void draw(bool fullRedraw) override;
    bool handleInput(const InputData& input) override;
    const char* getTitle() const override { return "Contacts"; }
    void configureSoftKeys() override;

private:
    // Build contact list from settings
    void buildContactList();

    // Helper functions for building list with favorites sorted to top
    void addRepeaterToList(const ContactEntry* c, int originalIdx);
    void addContactToList(const ContactEntry* c, int originalIdx);

    // Handle contact selection
    void onContactSelected(int index);

    // Format time ago string
    void formatTimeAgo(uint32_t timestamp, char* buf, size_t bufSize);

    // List view
    ListView _listView;
    static constexpr int MAX_CONTACTS = 32;
    ListItem _contactItems[MAX_CONTACTS];
    char _primaryStrings[MAX_CONTACTS][32];  // Increased for section headers
    char _secondaryStrings[MAX_CONTACTS][32];
    int _contactCount = 0;

    // Section tracking
    int _repeaterHeaderIdx = -1;
    int _contactsHeaderIdx = -1;
    int _repeaterCount = 0;
    int _regularContactCount = 0;
};

#endif // MESHBERRY_CONTACTSSCREEN_H
