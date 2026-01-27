/**
 * MeshBerry Home Screen
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2026 NodakMesh (nodakmesh.org)
 *
 * BlackBerry-inspired home screen with background image and bottom dock
 */

#ifndef MESHBERRY_HOMESCREEN_H
#define MESHBERRY_HOMESCREEN_H

#include "Screen.h"
#include "ScreenManager.h"

/**
 * Home screen dock items (4 icons at bottom)
 */
enum HomeMenuItem {
    HOME_MESSAGES = 0,
    HOME_CONTACTS,
    HOME_SETTINGS,
    HOME_MAP,       // Placeholder for future map feature
    HOME_ITEM_COUNT
};

constexpr int DOCK_ITEM_COUNT = HOME_ITEM_COUNT;

/**
 * Home Screen - Background image with bottom dock, no soft keys
 */
class HomeScreen : public Screen {
public:
    HomeScreen() = default;
    ~HomeScreen() override = default;

    // Screen interface
    ScreenId getId() const override { return ScreenId::HOME; }
    void onEnter() override;
    void onExit() override;
    void draw(bool fullRedraw) override;
    bool handleInput(const InputData& input) override;
    void update(uint32_t deltaMs) override;
    const char* getTitle() const override { return nullptr; }
    void configureSoftKeys() override;

    void setBadge(HomeMenuItem item, uint8_t count);
    HomeMenuItem getSelectedItem() const { return _selectedItem; }

private:
    // Drawing helpers
    void drawBackground();
    void drawDock(bool fullRedraw);
    void drawDockItem(int dockIndex, bool selected);

    // Get screen ID for menu item
    ScreenId getScreenForItem(HomeMenuItem item) const;

    // State
    HomeMenuItem _selectedItem = HOME_MESSAGES;
    HomeMenuItem _prevSelectedItem = HOME_MESSAGES;
    uint8_t _badges[HOME_ITEM_COUNT] = { 0 };

    // Dock replaces soft key bar at very bottom
    static constexpr int16_t DOCK_HEIGHT = Theme::SOFTKEY_BAR_HEIGHT;  // 30px
    static constexpr int16_t DOCK_Y = Theme::SOFTKEY_BAR_Y;            // 210

    // Background image area (from status bar to dock)
    static constexpr int16_t BG_START_Y = Theme::STATUS_BAR_HEIGHT;    // 20
    static constexpr int16_t BG_HEIGHT = DOCK_Y - BG_START_Y;          // 210 - 20 = 190
};

#endif // MESHBERRY_HOMESCREEN_H
