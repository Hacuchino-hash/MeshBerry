/**
 * MeshBerry Emoji Picker Screen
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2026 NodakMesh (nodakmesh.org)
 *
 * Grid-based emoji picker with category tabs
 */

#ifndef MESHBERRY_EMOJI_PICKER_SCREEN_H
#define MESHBERRY_EMOJI_PICKER_SCREEN_H

#include "Screen.h"
#include "ScreenManager.h"
#include "Emoji.h"

class EmojiPickerScreen : public Screen {
public:
    EmojiPickerScreen() = default;

    ScreenId getId() const override { return ScreenId::EMOJI_PICKER; }

    void onEnter() override;
    void onExit() override;
    void draw(bool fullRedraw = false) override;
    bool handleInput(const InputData& input) override;
    void configureSoftKeys() override;

    /**
     * Get the selected emoji (UTF-8 encoded)
     * Call this after the screen exits if an emoji was selected
     * @param buf Output buffer (must be at least 5 bytes)
     * @return true if an emoji was selected
     */
    bool getSelectedEmoji(char* buf);

    /**
     * Check if an emoji was selected (vs cancelled)
     */
    bool wasEmojiSelected() const { return _emojiSelected; }

private:
    // Grid layout constants
    static const int COLS = 8;           // Emoji per row
    static const int ROWS = 5;           // Visible rows
    static const int CELL_SIZE = 16;     // Cell size (emoji is 12x12)
    static const int GRID_START_X = 8;
    static const int GRID_START_Y = 52;  // Below category tabs

    // Draw helpers
    void drawCategoryTabs();
    void drawGrid(bool fullRedraw);
    void drawCell(int col, int row, bool selected);

    // Navigation helpers
    void nextCategory();
    void prevCategory();
    void selectEmoji();
    int getGridIndex(int col, int row) const;
    int getCategoryEmojiCount() const;

    // State
    EmojiCategory _currentCategory = EmojiCategory::FACES;
    int _selectedCol = 0;
    int _selectedRow = 0;
    int _scrollOffset = 0;   // First visible row in category
    bool _emojiSelected = false;
    uint32_t _selectedCodepoint = 0;
    const char* _selectedShortcode = nullptr;  // Store shortcode for display
};

#endif // MESHBERRY_EMOJI_PICKER_SCREEN_H
