/**
 * MeshBerry WiFi Settings Screen Implementation
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2026 NodakMesh (nodakmesh.org)
 *
 * Manual SSID and password entry flow (MeshCore style)
 */

#include "WiFiScreen.h"
#include "ScreenManager.h"
#include "SoftKeyBar.h"
#include "Icons.h"
#include "../drivers/display.h"
#include "../drivers/keyboard.h"
#include "../settings/SettingsManager.h"

void WiFiScreen::onEnter() {
    _mode = MODE_MENU;
    _ssidBuffer[0] = '\0';
    _ssidPos = 0;
    _passwordBuffer[0] = '\0';
    _passwordPos = 0;
    _showPassword = false;
    _listView.setBounds(0, Theme::CONTENT_Y + 30, Theme::SCREEN_WIDTH, Theme::CONTENT_HEIGHT - 30);
    _listView.setItemHeight(40);
    buildMainMenu();
    requestRedraw();
}

void WiFiScreen::onExit() {
    // Nothing to clean up
}

void WiFiScreen::configureSoftKeys() {
    switch (_mode) {
        case MODE_MENU:
            SoftKeyBar::setLabels(nullptr, "Select", "Back");
            break;
        case MODE_ENTER_SSID:
            SoftKeyBar::setLabels("Clear", "Next", "Cancel");
            break;
        case MODE_ENTER_PASSWORD:
            SoftKeyBar::setLabels(_showPassword ? "Hide" : "Show", "Connect", "Cancel");
            break;
        case MODE_CONNECTING:
            SoftKeyBar::setLabels(nullptr, nullptr, "Cancel");
            break;
    }
}

void WiFiScreen::buildMainMenu() {
    _menuItemCount = 0;

    DeviceSettings& device = SettingsManager::getDeviceSettings();
    bool connected = WiFiManager::isConnected();

    // Status
    const char* status = connected ? "Connected" : "Disconnected";
    _menuItems[_menuItemCount++] = { "Status", status, nullptr,
                                      connected ? Theme::GREEN : Theme::GRAY_LIGHT,
                                      false, 0, nullptr };

    // Current network
    if (connected) {
        String ssid = WiFiManager::getSSID();
        strncpy(_valueStrings[0], ssid.c_str(), 31);
        _valueStrings[0][31] = '\0';
    } else if (device.wifiSSID[0]) {
        strncpy(_valueStrings[0], device.wifiSSID, 31);
        _valueStrings[0][31] = '\0';
    } else {
        strcpy(_valueStrings[0], "None");
    }
    _menuItems[_menuItemCount++] = { "Network", _valueStrings[0], nullptr,
                                      Theme::ACCENT, false, 0, nullptr };

    // Signal strength and IP (if connected)
    if (connected) {
        int8_t rssi = WiFiManager::getRSSI();
        snprintf(_valueStrings[1], 32, "%d dBm", rssi);
        _menuItems[_menuItemCount++] = { "Signal", _valueStrings[1], nullptr,
                                          Theme::TEXT_SECONDARY, false, 0, nullptr };

        String ip = WiFiManager::getIP();
        strncpy(_valueStrings[2], ip.c_str(), 31);
        _valueStrings[2][31] = '\0';
        _menuItems[_menuItemCount++] = { "IP Address", _valueStrings[2], nullptr,
                                          Theme::TEXT_SECONDARY, false, 0, nullptr };
    }

    // Divider
    _menuItems[_menuItemCount++] = { "---", nullptr, nullptr, Theme::DIVIDER, false, 0, nullptr };

    // Set Network action
    _menuItems[_menuItemCount++] = { "Set Network", "Enter SSID and password", nullptr,
                                      Theme::ACCENT, false, 0, nullptr };

    // Disconnect (if connected)
    if (connected) {
        _menuItems[_menuItemCount++] = { "Disconnect", nullptr, nullptr,
                                          Theme::RED, false, 0, nullptr };
    }

    _listView.setItems(_menuItems, _menuItemCount);
    _listView.setSelectedIndex(0);
}

void WiFiScreen::handleMenuSelect(int index) {
    bool connected = WiFiManager::isConnected();

    // Calculate actual item indices based on menu structure
    // Disconnected: 0=status, 1=network, 2=divider, 3=set_network
    // Connected: 0=status, 1=network, 2=signal, 3=IP, 4=divider, 5=set_network, 6=disconnect
    int setNetworkIndex = connected ? 5 : 3;
    int disconnectIndex = connected ? 6 : -1;

    if (index == setNetworkIndex) {
        startSSIDEntry();
    } else if (connected && index == disconnectIndex) {
        WiFiManager::disconnect();
        DeviceSettings& device = SettingsManager::getDeviceSettings();
        device.wifiEnabled = false;
        SettingsManager::saveDeviceSettings();
        buildMainMenu();
        requestRedraw();
    }
}

void WiFiScreen::startSSIDEntry() {
    _mode = MODE_ENTER_SSID;

    // Pre-fill with saved SSID if available
    DeviceSettings& device = SettingsManager::getDeviceSettings();
    if (device.wifiSSID[0]) {
        strncpy(_ssidBuffer, device.wifiSSID, 32);
        _ssidBuffer[32] = '\0';
        _ssidPos = strlen(_ssidBuffer);
    } else {
        _ssidBuffer[0] = '\0';
        _ssidPos = 0;
    }

    configureSoftKeys();
    requestRedraw();
}

void WiFiScreen::startPasswordEntry() {
    _mode = MODE_ENTER_PASSWORD;

    // Pre-fill with saved password if SSID matches
    DeviceSettings& device = SettingsManager::getDeviceSettings();
    if (strcmp(_ssidBuffer, device.wifiSSID) == 0 && device.wifiPassword[0]) {
        strncpy(_passwordBuffer, device.wifiPassword, 64);
        _passwordBuffer[64] = '\0';
        _passwordPos = strlen(_passwordBuffer);
    } else {
        _passwordBuffer[0] = '\0';
        _passwordPos = 0;
    }

    _showPassword = false;
    configureSoftKeys();
    requestRedraw();
}

void WiFiScreen::connectToNetwork() {
    _mode = MODE_CONNECTING;
    _connectStartTime = millis();
    _animFrame = 0;
    configureSoftKeys();
    requestRedraw();

    // Attempt connection
    bool success = WiFiManager::connect(_ssidBuffer, _passwordBuffer, 15000);

    if (success) {
        // Save credentials
        DeviceSettings& device = SettingsManager::getDeviceSettings();
        strncpy(device.wifiSSID, _ssidBuffer, 32);
        device.wifiSSID[32] = '\0';
        strncpy(device.wifiPassword, _passwordBuffer, 64);
        device.wifiPassword[64] = '\0';
        device.wifiEnabled = true;
        SettingsManager::saveDeviceSettings();

        // Try NTP sync if enabled
        if (device.ntpEnabled) {
            WiFiManager::syncTime();
        }

        // Return to main menu
        _mode = MODE_MENU;
        buildMainMenu();
    } else {
        // Connection failed - go back to password entry
        _mode = MODE_ENTER_PASSWORD;
    }

    configureSoftKeys();
    requestRedraw();
}

void WiFiScreen::draw(bool fullRedraw) {
    if (fullRedraw) {
        Display::fillRect(0, Theme::CONTENT_Y,
                          Theme::SCREEN_WIDTH, Theme::CONTENT_HEIGHT,
                          Theme::BG_PRIMARY);
    }

    switch (_mode) {
        case MODE_MENU:
            if (fullRedraw) {
                Display::drawText(12, Theme::CONTENT_Y + 4, "WiFi", Theme::ACCENT, 2);
                Display::drawHLine(12, Theme::CONTENT_Y + 26, Theme::SCREEN_WIDTH - 24, Theme::DIVIDER);
            }
            _listView.draw(fullRedraw);
            break;

        case MODE_ENTER_SSID:
            drawSSIDEntry();
            break;

        case MODE_ENTER_PASSWORD:
            drawPasswordEntry();
            break;

        case MODE_CONNECTING:
            drawConnectingAnimation();
            break;
    }
}

void WiFiScreen::drawSSIDEntry() {
    Display::fillRect(0, Theme::CONTENT_Y,
                      Theme::SCREEN_WIDTH, Theme::CONTENT_HEIGHT,
                      Theme::BG_PRIMARY);

    // Title
    Display::drawText(12, Theme::CONTENT_Y + 4, "Enter Network Name", Theme::ACCENT, 2);
    Display::drawHLine(12, Theme::CONTENT_Y + 26, Theme::SCREEN_WIDTH - 24, Theme::DIVIDER);

    // SSID label
    Display::drawText(12, Theme::CONTENT_Y + 40, "Network SSID:", Theme::TEXT_SECONDARY, 1);

    // SSID field background
    Display::fillRect(12, Theme::CONTENT_Y + 56, Theme::SCREEN_WIDTH - 24, 32, Theme::BG_SECONDARY);
    Display::drawRect(12, Theme::CONTENT_Y + 56, Theme::SCREEN_WIDTH - 24, 32, Theme::DIVIDER);

    // SSID text with cursor
    char displayText[35];
    snprintf(displayText, sizeof(displayText), "%s_", _ssidBuffer);

    // Truncate display if too long
    int maxChars = (Theme::SCREEN_WIDTH - 40) / 8;
    if ((int)strlen(displayText) > maxChars) {
        int offset = strlen(displayText) - maxChars;
        Display::drawText(16, Theme::CONTENT_Y + 66, displayText + offset, Theme::TEXT_PRIMARY, 1);
    } else {
        Display::drawText(16, Theme::CONTENT_Y + 66, displayText, Theme::TEXT_PRIMARY, 1);
    }

    // Hint text
    Display::drawText(12, Theme::CONTENT_Y + 100, "Type network name, press Next", Theme::TEXT_SECONDARY, 1);
}

void WiFiScreen::drawPasswordEntry() {
    Display::fillRect(0, Theme::CONTENT_Y,
                      Theme::SCREEN_WIDTH, Theme::CONTENT_HEIGHT,
                      Theme::BG_PRIMARY);

    // Title: Network name
    Display::drawText(12, Theme::CONTENT_Y + 4, "Connect to:", Theme::TEXT_SECONDARY, 1);
    Display::drawText(12, Theme::CONTENT_Y + 18, _ssidBuffer, Theme::ACCENT, 2);

    // Divider
    Display::drawHLine(12, Theme::CONTENT_Y + 42, Theme::SCREEN_WIDTH - 24, Theme::DIVIDER);

    // Password label
    Display::drawText(12, Theme::CONTENT_Y + 52, "Password:", Theme::TEXT_SECONDARY, 1);

    // Password field background
    Display::fillRect(12, Theme::CONTENT_Y + 68, Theme::SCREEN_WIDTH - 24, 32, Theme::BG_SECONDARY);
    Display::drawRect(12, Theme::CONTENT_Y + 68, Theme::SCREEN_WIDTH - 24, 32, Theme::DIVIDER);

    // Password text (masked or visible)
    char displayText[66];
    if (_showPassword) {
        snprintf(displayText, sizeof(displayText), "%s_", _passwordBuffer);
    } else {
        int len = strlen(_passwordBuffer);
        for (int i = 0; i < len && i < 64; i++) {
            displayText[i] = '*';
        }
        displayText[len] = '_';
        displayText[len + 1] = '\0';
    }

    // Truncate display if too long
    int maxChars = (Theme::SCREEN_WIDTH - 40) / 8;
    if ((int)strlen(displayText) > maxChars) {
        int offset = strlen(displayText) - maxChars;
        Display::drawText(16, Theme::CONTENT_Y + 78, displayText + offset, Theme::TEXT_PRIMARY, 1);
    } else {
        Display::drawText(16, Theme::CONTENT_Y + 78, displayText, Theme::TEXT_PRIMARY, 1);
    }

    // Hint text
    Display::drawText(12, Theme::CONTENT_Y + 110, "Type password, Enter to connect", Theme::TEXT_SECONDARY, 1);
}

void WiFiScreen::drawConnectingAnimation() {
    Display::fillRect(0, Theme::CONTENT_Y,
                      Theme::SCREEN_WIDTH, Theme::CONTENT_HEIGHT,
                      Theme::BG_PRIMARY);

    // Show connecting to network name
    Display::drawText(12, Theme::CONTENT_Y + 40, "Connecting to:", Theme::TEXT_SECONDARY, 1);
    Display::drawText(12, Theme::CONTENT_Y + 56, _ssidBuffer, Theme::ACCENT, 2);

    // Animated dots
    char dots[4] = "   ";
    int dotCount = (_animFrame / 10) % 4;
    for (int i = 0; i < dotCount; i++) {
        dots[i] = '.';
    }
    int textWidth = strlen(_ssidBuffer) * 12;
    Display::drawText(12 + textWidth + 4, Theme::CONTENT_Y + 56, dots, Theme::ACCENT, 2);
}

void WiFiScreen::update(uint32_t deltaMs) {
    if (_mode == MODE_CONNECTING) {
        _animFrame++;
        if (_animFrame % 10 == 0) {
            requestRedraw();
        }
    }
}

bool WiFiScreen::handleInput(const InputData& input) {
    // Handle SSID entry mode
    if (_mode == MODE_ENTER_SSID) {
        if (input.event == InputEvent::KEY_PRESS) {
            char c = input.keyChar;

            if (input.keyCode == KEY_ENTER) {
                // Proceed to password entry if SSID is not empty
                if (_ssidPos > 0) {
                    startPasswordEntry();
                }
                return true;
            } else if (input.keyCode == KEY_BACKSPACE) {
                if (_ssidPos > 0) {
                    _ssidBuffer[--_ssidPos] = '\0';
                    requestRedraw();
                }
                return true;
            } else if (c >= 32 && c <= 126) {
                if (_ssidPos < 32) {
                    _ssidBuffer[_ssidPos++] = c;
                    _ssidBuffer[_ssidPos] = '\0';
                    requestRedraw();
                }
                return true;
            }
        } else if (input.event == InputEvent::SOFTKEY_LEFT) {
            // Clear SSID
            _ssidBuffer[0] = '\0';
            _ssidPos = 0;
            requestRedraw();
            return true;
        } else if (input.event == InputEvent::SOFTKEY_CENTER) {
            // Next - proceed to password entry
            if (_ssidPos > 0) {
                startPasswordEntry();
            }
            return true;
        } else if (input.event == InputEvent::BACK ||
                   input.event == InputEvent::SOFTKEY_RIGHT) {
            // Cancel - go back to menu
            _mode = MODE_MENU;
            buildMainMenu();
            configureSoftKeys();
            requestRedraw();
            return true;
        }
        return false;
    }

    // Handle password entry mode
    if (_mode == MODE_ENTER_PASSWORD) {
        if (input.event == InputEvent::KEY_PRESS) {
            char c = input.keyChar;

            if (input.keyCode == KEY_ENTER) {
                connectToNetwork();
                return true;
            } else if (input.keyCode == KEY_BACKSPACE) {
                if (_passwordPos > 0) {
                    _passwordBuffer[--_passwordPos] = '\0';
                    requestRedraw();
                }
                return true;
            } else if (c >= 32 && c <= 126) {
                if (_passwordPos < 64) {
                    _passwordBuffer[_passwordPos++] = c;
                    _passwordBuffer[_passwordPos] = '\0';
                    requestRedraw();
                }
                return true;
            }
        } else if (input.event == InputEvent::SOFTKEY_LEFT) {
            // Toggle password visibility
            _showPassword = !_showPassword;
            configureSoftKeys();
            requestRedraw();
            return true;
        } else if (input.event == InputEvent::SOFTKEY_CENTER) {
            // Connect
            connectToNetwork();
            return true;
        } else if (input.event == InputEvent::BACK ||
                   input.event == InputEvent::SOFTKEY_RIGHT) {
            // Cancel - go back to SSID entry
            _mode = MODE_ENTER_SSID;
            configureSoftKeys();
            requestRedraw();
            return true;
        }
        return false;
    }

    // Handle connecting mode - only allow cancel
    if (_mode == MODE_CONNECTING) {
        if (input.event == InputEvent::BACK || input.event == InputEvent::SOFTKEY_RIGHT) {
            WiFiManager::disconnect();
            _mode = MODE_MENU;
            buildMainMenu();
            configureSoftKeys();
            requestRedraw();
            return true;
        }
        return false;
    }

    // Handle menu navigation
    bool isBackKey = (input.event == InputEvent::KEY_PRESS && input.keyCode == KEY_BACKSPACE);

    if (input.event == InputEvent::BACK ||
        input.event == InputEvent::SOFTKEY_RIGHT ||
        isBackKey) {
        Screens.goBack();
        return true;
    }

    if (input.event == InputEvent::TRACKBALL_UP) {
        _listView.handleTrackball(true, false, false, false, false);
        requestRedraw();
        return true;
    }

    if (input.event == InputEvent::TRACKBALL_DOWN) {
        _listView.handleTrackball(false, true, false, false, false);
        requestRedraw();
        return true;
    }

    if (input.event == InputEvent::TRACKBALL_CLICK ||
        input.event == InputEvent::SOFTKEY_CENTER ||
        (input.event == InputEvent::KEY_PRESS && input.keyCode == KEY_ENTER)) {
        int selected = _listView.getSelectedIndex();
        handleMenuSelect(selected);
        return true;
    }

    // Handle touch drag for scrolling
    if (input.event == InputEvent::TOUCH_DRAG) {
        _listView.handleTouchDrag(input.dragDeltaY);
        requestRedraw();
        return true;
    }

    return false;
}
