/**
 * MeshBerry Emoji Picker Screen Implementation
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2026 NodakMesh (nodakmesh.org)
 */

#include "EmojiPickerScreen.h"
#include "SoftKeyBar.h"
#include "Theme.h"
#include "../drivers/display.h"
#include <Arduino.h>

void EmojiPickerScreen::onEnter() {
    _currentCategory = EmojiCategory::FACES;
    _selectedCol = 0;
    _selectedRow = 0;
    _scrollOffset = 0;
    _emojiSelected = false;
    _selectedCodepoint = 0;
    requestRedraw();
}

void EmojiPickerScreen::onExit() {
    // Nothing to clean up
}

void EmojiPickerScreen::configureSoftKeys() {
    SoftKeyBar::setLabels("Category", "Insert", "Back");
}

void EmojiPickerScreen::draw(bool fullRedraw) {
    if (fullRedraw) {
        // Clear content area
        Display::fillRect(0, Theme::CONTENT_Y,
                          Theme::SCREEN_WIDTH, Theme::CONTENT_HEIGHT,
                          Theme::BG_PRIMARY);

        // Title
        Display::drawText(8, Theme::CONTENT_Y + 4, "Select Emoji", Theme::ACCENT, 1);
    }

    drawCategoryTabs();
    drawGrid(fullRedraw);
}

void EmojiPickerScreen::drawCategoryTabs() {
    // Category tab bar
    int16_t tabY = Theme::CONTENT_Y + 20;
    int16_t tabHeight = 20;

    Display::fillRect(0, tabY, Theme::SCREEN_WIDTH, tabHeight, Theme::BG_SECONDARY);
    Display::drawHLine(0, tabY + tabHeight - 1, Theme::SCREEN_WIDTH, Theme::DIVIDER);

    // Draw category names (abbreviated)
    const char* shortNames[] = {"Face", "Hand", "Ppl", "Love", "Anim", "Food", "Sport", "Car", "Obj", "Sym", "Flag"};
    int numCats = (int)EmojiCategory::CATEGORY_COUNT;

    // Calculate positions - show current category centered with arrows
    int16_t textX = 40;
    int16_t textY = tabY + 6;

    // Left arrow if not at first category
    if ((int)_currentCategory > 0) {
        Display::drawText(8, textY, "<", Theme::GRAY_LIGHT, 1);
    }

    // Current category name
    const char* catName = Emoji::getCategoryName(_currentCategory);
    Display::drawTextCentered(40, textY, Theme::SCREEN_WIDTH - 80, catName, Theme::WHITE, 1);

    // Right arrow if not at last category
    if ((int)_currentCategory < numCats - 1) {
        Display::drawText(Theme::SCREEN_WIDTH - 16, textY, ">", Theme::GRAY_LIGHT, 1);
    }
}

void EmojiPickerScreen::drawGrid(bool fullRedraw) {
    // Grid background
    int16_t gridY = GRID_START_Y;
    int16_t gridHeight = ROWS * CELL_SIZE;

    if (fullRedraw) {
        Display::fillRect(GRID_START_X - 2, gridY - 2,
                          COLS * CELL_SIZE + 4, gridHeight + 4,
                          Theme::BG_SECONDARY);
    }

    // Get emoji for current category
    int catStart = Emoji::getCategoryStart(_currentCategory);
    int catCount = Emoji::getCategoryCount(_currentCategory);

    // Draw visible emoji in grid
    for (int row = 0; row < ROWS; row++) {
        for (int col = 0; col < COLS; col++) {
            int emojiIdx = catStart + (_scrollOffset + row) * COLS + col;

            // Check if this cell has an emoji
            if (emojiIdx < catStart + catCount) {
                bool isSelected = (col == _selectedCol && row == _selectedRow);
                drawCell(col, row, isSelected);
            } else {
                // Empty cell
                int16_t cellX = GRID_START_X + col * CELL_SIZE;
                int16_t cellY = gridY + row * CELL_SIZE;
                Display::fillRect(cellX, cellY, CELL_SIZE, CELL_SIZE, Theme::BG_SECONDARY);
            }
        }
    }

    // Scroll indicators
    int totalRows = (catCount + COLS - 1) / COLS;
    if (totalRows > ROWS) {
        if (_scrollOffset > 0) {
            // Up arrow
            Display::drawText(Theme::SCREEN_WIDTH - 16, gridY, "^", Theme::GRAY_LIGHT, 1);
        }
        if (_scrollOffset + ROWS < totalRows) {
            // Down arrow
            Display::drawText(Theme::SCREEN_WIDTH - 16, gridY + gridHeight - 10, "v", Theme::GRAY_LIGHT, 1);
        }
    }
}

void EmojiPickerScreen::drawCell(int col, int row, bool selected) {
    int16_t cellX = GRID_START_X + col * CELL_SIZE;
    int16_t cellY = GRID_START_Y + row * CELL_SIZE;

    // Background
    uint16_t bgColor = selected ? Theme::BLUE_DARK : Theme::BG_SECONDARY;
    Display::fillRect(cellX, cellY, CELL_SIZE, CELL_SIZE, bgColor);

    // Selection border
    if (selected) {
        Display::drawRect(cellX, cellY, CELL_SIZE, CELL_SIZE, Theme::BLUE);
    }

    // Get emoji at this position
    int catStart = Emoji::getCategoryStart(_currentCategory);
    int emojiIdx = catStart + (_scrollOffset + row) * COLS + col;
    const EmojiEntry* emoji = Emoji::getByIndex(emojiIdx);

    if (emoji && emoji->bitmap) {
        // Center emoji in cell (12x12 in 16x16 cell)
        int16_t emojiX = cellX + (CELL_SIZE - EMOJI_WIDTH) / 2;
        int16_t emojiY = cellY + (CELL_SIZE - EMOJI_HEIGHT) / 2;
        Display::drawRGB565(emojiX, emojiY, emoji->bitmap, EMOJI_WIDTH, EMOJI_HEIGHT);
    }
}

bool EmojiPickerScreen::handleInput(const InputData& input) {
    int catCount = getCategoryEmojiCount();
    int totalRows = (catCount + COLS - 1) / COLS;

    switch (input.event) {
        case InputEvent::TRACKBALL_UP:
            if (_selectedRow > 0) {
                _selectedRow--;
            } else if (_scrollOffset > 0) {
                _scrollOffset--;
            }
            requestRedraw();
            return true;

        case InputEvent::TRACKBALL_DOWN:
            if (_selectedRow < ROWS - 1 && (_scrollOffset + _selectedRow + 1) * COLS < catCount) {
                _selectedRow++;
            } else if (_scrollOffset + ROWS < totalRows) {
                _scrollOffset++;
            }
            requestRedraw();
            return true;

        case InputEvent::TRACKBALL_LEFT:
            if (_selectedCol > 0) {
                _selectedCol--;
            } else {
                prevCategory();
            }
            requestRedraw();
            return true;

        case InputEvent::TRACKBALL_RIGHT:
            {
                int currentIdx = getGridIndex(_selectedCol, _selectedRow);
                if (_selectedCol < COLS - 1 && currentIdx + 1 < catCount) {
                    _selectedCol++;
                } else {
                    nextCategory();
                }
            }
            requestRedraw();
            return true;

        case InputEvent::TRACKBALL_CLICK:
        case InputEvent::SOFTKEY_CENTER:
            selectEmoji();
            return true;

        case InputEvent::SOFTKEY_LEFT:
            nextCategory();
            requestRedraw();
            return true;

        case InputEvent::SOFTKEY_RIGHT:
        case InputEvent::BACK:
            _emojiSelected = false;
            Screens.goBack();
            return true;

        case InputEvent::TOUCH_TAP:
            {
                int16_t tx = input.touchX;
                int16_t ty = input.touchY;

                // Check soft key bar
                if (ty >= Theme::SOFTKEY_BAR_Y) {
                    if (tx >= 214) {
                        _emojiSelected = false;
                        Screens.goBack();
                    } else if (tx >= 107) {
                        selectEmoji();
                    } else {
                        nextCategory();
                        requestRedraw();
                    }
                    return true;
                }

                // Check grid area
                if (ty >= GRID_START_Y && ty < GRID_START_Y + ROWS * CELL_SIZE &&
                    tx >= GRID_START_X && tx < GRID_START_X + COLS * CELL_SIZE) {
                    int col = (tx - GRID_START_X) / CELL_SIZE;
                    int row = (ty - GRID_START_Y) / CELL_SIZE;

                    int idx = getGridIndex(col, row);
                    if (idx < catCount) {
                        _selectedCol = col;
                        _selectedRow = row;
                        selectEmoji();
                    }
                    return true;
                }

                // Check category tab area for left/right navigation
                int16_t tabY = Theme::CONTENT_Y + 20;
                if (ty >= tabY && ty < tabY + 20) {
                    if (tx < 40) {
                        prevCategory();
                    } else if (tx > Theme::SCREEN_WIDTH - 40) {
                        nextCategory();
                    }
                    requestRedraw();
                    return true;
                }
            }
            return true;

        default:
            return false;
    }
}

void EmojiPickerScreen::nextCategory() {
    int cat = (int)_currentCategory;
    if (cat < (int)EmojiCategory::CATEGORY_COUNT - 1) {
        _currentCategory = (EmojiCategory)(cat + 1);
        _selectedCol = 0;
        _selectedRow = 0;
        _scrollOffset = 0;
    }
}

void EmojiPickerScreen::prevCategory() {
    int cat = (int)_currentCategory;
    if (cat > 0) {
        _currentCategory = (EmojiCategory)(cat - 1);
        _selectedCol = 0;
        _selectedRow = 0;
        _scrollOffset = 0;
    }
}

void EmojiPickerScreen::selectEmoji() {
    int emojiIdx = Emoji::getCategoryStart(_currentCategory) + getGridIndex(_selectedCol, _selectedRow);
    const EmojiEntry* emoji = Emoji::getByIndex(emojiIdx);

    if (emoji) {
        _selectedCodepoint = emoji->codepoint;
        _selectedShortcode = emoji->shortcode;  // Store shortcode for display
        _emojiSelected = true;

        // Go back - ChatScreen/DMChatScreen will check our selection
        Screens.goBack();
    }
}

int EmojiPickerScreen::getGridIndex(int col, int row) const {
    return (_scrollOffset + row) * COLS + col;
}

int EmojiPickerScreen::getCategoryEmojiCount() const {
    return Emoji::getCategoryCount(_currentCategory);
}

bool EmojiPickerScreen::getSelectedEmoji(char* buf) {
    if (!_emojiSelected || !buf || !_selectedShortcode) return false;

    // Return shortcode format like ":smile:" so user can see/learn it
    snprintf(buf, 32, ":%s:", _selectedShortcode);

    // Clear selection state after retrieval (prevent re-use/stacking)
    _emojiSelected = false;
    _selectedCodepoint = 0;
    _selectedShortcode = nullptr;

    return true;
}
