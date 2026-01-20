/**
 * MeshBerry Home Screen
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2026 NodakMesh (nodakmesh.org)
 *
 * BlackBerry-style home screen with icon grid
 */

#ifndef MESHBERRY_HOMESCREEN_H
#define MESHBERRY_HOMESCREEN_H

#include "Screen.h"
#include "ScreenManager.h"

/**
 * Home screen menu items
 * Note: Channels moved to Messages screen (channels are group chats)
 */
enum HomeMenuItem {
    HOME_MESSAGES = 0,
    HOME_CONTACTS,
    HOME_SETTINGS,
    HOME_STATUS,
    HOME_REPEATERS,
    HOME_GPS,
    HOME_ABOUT,
    HOME_ITEM_COUNT  // Now 7 items
};

/**
 * Home Screen - Main navigation hub
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
    const char* getTitle() const override { return nullptr; }  // No title on home
    void configureSoftKeys() override;

    /**
     * Set notification badge for a menu item
     * @param item Which menu item
     * @param count Badge count (0 to hide)
     */
    void setBadge(HomeMenuItem item, uint8_t count);

    /**
     * Get current selected item
     */
    HomeMenuItem getSelectedItem() const { return _selectedItem; }

private:
    // Draw a single tile
    void drawTile(int index, bool selected);

    // Get screen ID for menu item
    ScreenId getScreenForItem(HomeMenuItem item) const;

    // Grid navigation
    int getRow(int index) const { return index / 4; }
    int getCol(int index) const { return index % 4; }
    int getIndex(int row, int col) const { return row * 4 + col; }

    // State
    HomeMenuItem _selectedItem = HOME_MESSAGES;
    uint8_t _badges[HOME_ITEM_COUNT] = { 0 };

    // Layout
    static constexpr int COLS = 4;
    static constexpr int ROWS = 2;
    static constexpr int16_t TILE_WIDTH = 76;
    static constexpr int16_t TILE_HEIGHT = 85;
    static constexpr int16_t TILE_MARGIN = 4;
    static constexpr int16_t GRID_START_X = 6;
    static constexpr int16_t GRID_START_Y = Theme::CONTENT_Y + 5;
};

#endif // MESHBERRY_HOMESCREEN_H
