/**
 * MeshBerry DM Settings Screen
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2026 NodakMesh (nodakmesh.org)
 *
 * Full-screen DM routing settings for a contact
 */

#ifndef MESHBERRY_DMSETTINGSSCREEN_H
#define MESHBERRY_DMSETTINGSSCREEN_H

#include "Screen.h"
#include "ScreenManager.h"
#include "ListView.h"
#include "../settings/ContactSettings.h"

/**
 * DM Settings Screen - Configure routing for a specific contact
 */
class DMSettingsScreen : public Screen {
public:
    DMSettingsScreen();
    ~DMSettingsScreen() override = default;

    // Screen interface
    ScreenId getId() const override { return ScreenId::DM_SETTINGS; }
    void onEnter() override;
    void onExit() override;
    void draw(bool fullRedraw) override;
    bool handleInput(const InputData& input) override;
    const char* getTitle() const override;
    void configureSoftKeys() override;

    /**
     * Set the contact to configure
     * @param contactId Node ID of the contact
     */
    void setContact(uint32_t contactId);

private:
    // Build menu items
    void buildMenu();

    // Handle item selection
    void onItemSelected(int index);

    // Draw contact info header
    void drawHeader();

    // Draw path info section
    void drawPathInfo();

    // Get routing mode name
    const char* getRoutingModeName(DMRoutingMode mode) const;

    // Contact ID being configured
    uint32_t _contactId = 0;
    char _contactName[32];

    // List view for routing options
    ListView _listView;
    static constexpr int MAX_ITEMS = 8;
    ListItem _menuItems[MAX_ITEMS];
    char _valueStrings[MAX_ITEMS][32];
    int _menuItemCount = 0;

    // Repeater selection state (for manual path)
    bool _selectingRepeater = false;
    int _selectedRepeaterSlot = 0;  // Which slot in manual path we're editing
};

#endif // MESHBERRY_DMSETTINGSSCREEN_H
