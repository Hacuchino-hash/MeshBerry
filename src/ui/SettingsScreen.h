/**
 * MeshBerry Settings Screen
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2026 NodakMesh (nodakmesh.org)
 *
 * Hierarchical settings menu with BlackBerry style
 */

#ifndef MESHBERRY_SETTINGSSCREEN_H
#define MESHBERRY_SETTINGSSCREEN_H

#include "Screen.h"
#include "ScreenManager.h"
#include "ListView.h"

/**
 * Settings menu levels
 */
enum SettingsLevel {
    SETTINGS_MAIN = 0,
    SETTINGS_RADIO,
    SETTINGS_DISPLAY,
    SETTINGS_NETWORK,
    SETTINGS_GPS,
    SETTINGS_POWER,
    SETTINGS_AUDIO,
    SETTINGS_ABOUT
};

/**
 * Settings Screen - Hierarchical settings menu
 */
class SettingsScreen : public Screen {
public:
    SettingsScreen();
    ~SettingsScreen() override = default;

    // Screen interface
    ScreenId getId() const override { return ScreenId::SETTINGS; }
    void onEnter() override;
    void onExit() override;
    void draw(bool fullRedraw) override;
    bool handleInput(const InputData& input) override;
    const char* getTitle() const override;
    void configureSoftKeys() override;

private:
    // Build menu items for current level
    void buildMenu();

    // Handle item selection
    void onItemSelected(int index);

    // Apply radio settings changes
    void applyRadioSettings();

    // Navigation
    SettingsLevel _currentLevel = SETTINGS_MAIN;
    int _editingIndex = -1;  // -1 = not editing, >= 0 = editing that item

    // List view
    ListView _listView;
    static constexpr int MAX_ITEMS = 8;
    ListItem _menuItems[MAX_ITEMS];
    char _valueStrings[MAX_ITEMS][32];  // Buffer for dynamic value strings
    int _menuItemCount = 0;
};

#endif // MESHBERRY_SETTINGSSCREEN_H
