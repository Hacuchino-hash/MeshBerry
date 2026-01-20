/**
 * MeshBerry Placeholder Screens
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2026 NodakMesh (nodakmesh.org)
 *
 * Placeholder screens for features not yet implemented
 */

#ifndef MESHBERRY_PLACEHOLDERSCREEN_H
#define MESHBERRY_PLACEHOLDERSCREEN_H

#include "Screen.h"
#include "ScreenManager.h"
#include "SoftKeyBar.h"
#include "../drivers/display.h"
#include "../drivers/keyboard.h"

/**
 * Generic placeholder screen template
 */
template<ScreenId ID>
class PlaceholderScreen : public Screen {
public:
    PlaceholderScreen(const char* title) : _title(title) {}
    ~PlaceholderScreen() override = default;

    ScreenId getId() const override { return ID; }

    void onEnter() override { requestRedraw(); }

    void draw(bool fullRedraw) override {
        if (fullRedraw) {
            Display::fillRect(0, Theme::CONTENT_Y,
                              Theme::SCREEN_WIDTH, Theme::CONTENT_HEIGHT,
                              Theme::BG_PRIMARY);
        }

        // Title
        Display::drawText(12, Theme::CONTENT_Y + 8, _title, Theme::ACCENT, 2);

        // Coming soon message
        Display::drawTextCentered(0, Theme::CONTENT_Y + 80,
                                  Theme::SCREEN_WIDTH,
                                  "Coming Soon", Theme::TEXT_SECONDARY, 2);

        Display::drawTextCentered(0, Theme::CONTENT_Y + 120,
                                  Theme::SCREEN_WIDTH,
                                  "This feature is under development",
                                  Theme::GRAY_LIGHT, 1);
    }

    bool handleInput(const InputData& input) override {
        // Treat backspace as back since placeholder screens have no text input
        bool isBackKey = (input.event == InputEvent::KEY_PRESS && input.keyCode == KEY_BACKSPACE);
        if (input.event == InputEvent::BACK ||
            input.event == InputEvent::TRACKBALL_LEFT ||
            isBackKey) {
            Screens.goBack();
            return true;
        }
        return false;
    }

    void configureSoftKeys() override {
        SoftKeyBar::setLabels(nullptr, nullptr, "Back");
    }

    const char* getTitle() const override { return _title; }

private:
    const char* _title;
};

// Concrete placeholder instances
using MessagesPlaceholder = PlaceholderScreen<ScreenId::MESSAGES>;
using ContactsPlaceholder = PlaceholderScreen<ScreenId::CONTACTS>;
using SettingsPlaceholder = PlaceholderScreen<ScreenId::SETTINGS>;
using ChannelsPlaceholder = PlaceholderScreen<ScreenId::CHANNELS>;
using GpsPlaceholder = PlaceholderScreen<ScreenId::GPS>;
using AboutPlaceholder = PlaceholderScreen<ScreenId::ABOUT>;

#endif // MESHBERRY_PLACEHOLDERSCREEN_H
