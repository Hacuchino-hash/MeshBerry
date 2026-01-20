/**
 * MeshBerry About Screen
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2026 NodakMesh (nodakmesh.org)
 *
 * Shows application version, license, and organization info
 */

#ifndef MESHBERRY_ABOUTSCREEN_H
#define MESHBERRY_ABOUTSCREEN_H

#include "Screen.h"
#include "ScreenManager.h"
#include "ListView.h"

/**
 * About Screen - Shows application information
 */
class AboutScreen : public Screen {
public:
    AboutScreen();
    ~AboutScreen() override = default;

    // Screen interface
    ScreenId getId() const override { return ScreenId::ABOUT; }
    void onEnter() override;
    void onExit() override {}
    void draw(bool fullRedraw) override;
    bool handleInput(const InputData& input) override;
    const char* getTitle() const override { return "About"; }
    void configureSoftKeys() override;

private:
    ListView _listView;
    static constexpr int MAX_ITEMS = 6;
    ListItem _menuItems[MAX_ITEMS];
    int _menuItemCount = 0;
};

#endif // MESHBERRY_ABOUTSCREEN_H
