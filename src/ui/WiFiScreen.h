/**
 * MeshBerry WiFi Settings Screen
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2026 NodakMesh (nodakmesh.org)
 *
 * This file is part of MeshBerry.
 *
 * WiFi network configuration UI - manual SSID and password entry
 */

#ifndef MESHBERRY_WIFI_SCREEN_H
#define MESHBERRY_WIFI_SCREEN_H

#include "Screen.h"
#include "ListView.h"
#include "../drivers/wifi.h"

class WiFiScreen : public Screen {
public:
    ScreenId getId() const override { return ScreenId::SETTINGS_WIFI; }

    void onEnter() override;
    void onExit() override;
    void draw(bool fullRedraw) override;
    bool handleInput(const InputData& input) override;
    void update(uint32_t deltaMs) override;
    const char* getTitle() const override { return "WiFi"; }
    void configureSoftKeys() override;

private:
    // Screen modes
    enum Mode {
        MODE_MENU,           // Main menu
        MODE_ENTER_SSID,     // SSID entry
        MODE_ENTER_PASSWORD, // Password entry
        MODE_CONNECTING      // Connection in progress
    };

    Mode _mode = MODE_MENU;

    // List view for menus
    ListView _listView;
    ListItem _menuItems[8];
    int _menuItemCount = 0;
    char _valueStrings[4][32];  // Buffer for dynamic values

    // SSID entry
    char _ssidBuffer[33];
    int _ssidPos = 0;

    // Password entry
    char _passwordBuffer[65];
    int _passwordPos = 0;
    bool _showPassword = false;

    // Connection state
    uint32_t _connectStartTime = 0;
    int _animFrame = 0;

    // Build menu items
    void buildMainMenu();

    // Actions
    void handleMenuSelect(int index);
    void startSSIDEntry();
    void startPasswordEntry();
    void connectToNetwork();

    // Drawing helpers
    void drawSSIDEntry();
    void drawPasswordEntry();
    void drawConnectingAnimation();
};

#endif // MESHBERRY_WIFI_SCREEN_H
