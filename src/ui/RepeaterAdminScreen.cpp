/**
 * MeshBerry Repeater Admin Screen Implementation
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2026 NodakMesh (nodakmesh.org)
 */

#include "RepeaterAdminScreen.h"
#include "RepeaterCLIScreen.h"
#include "SoftKeyBar.h"
#include "../drivers/display.h"
#include "../drivers/keyboard.h"
#include "../mesh/MeshBerryMesh.h"
#include "../settings/SettingsManager.h"
#include <Arduino.h>
#include <string.h>

// External CLI screen reference (set by main.cpp)
extern RepeaterCLIScreen* repeaterCLIScreen;

// Predefined admin commands
const RepeaterAdminScreen::Command RepeaterAdminScreen::_commands[NUM_COMMANDS] = {
    {"View Status",     nullptr,    1},  // Guest+ (opens status view)
    {"Settings",        nullptr,    2},  // Operator+ (opens settings menu)
    {"Send Advert",     "advert",   2},  // Operator+
    {"Get Uptime",      "uptime",   1},  // Guest+
    {"Get Config",      "get name", 2},  // Operator+
    {"Reboot",          "reboot",   3},  // Admin only
    {"Terminal",        nullptr,    2},  // Operator+ (opens CLI screen)
    {"Logout",          nullptr,    0}   // Always available
};

// Settings definitions
const RepeaterAdminScreen::Setting RepeaterAdminScreen::_settings[NUM_SETTINGS] = {
    {"Advert Interval", "get advert_interval", "set advert_interval", 2},
    {"Flood Mode",      "get flood",           "set flood",           2},
    {"Zero Hop",        "get zero_hop",        "set zero_hop",        2}
};

void RepeaterAdminScreen::clearScreen() {
    Display::fillRect(0, Theme::CONTENT_Y,
                      Theme::SCREEN_WIDTH, Theme::CONTENT_HEIGHT,
                      Theme::BG_PRIMARY);
}

void RepeaterAdminScreen::onEnter() {
    _state = STATE_PASSWORD;
    _selectedCmd = 0;
    _selectedSetting = 0;
    _lastResponse[0] = '\0';
    _statusMessage[0] = '\0';
    _customCmd[0] = '\0';
    _customCmdPos = 0;
    _isConnected = false;
    _permissions = 0;
    _settingsLoaded = false;

    // Clear status info
    _statusName[0] = '\0';
    _statusUptime[0] = '\0';
    _statusFreq[0] = '\0';
    _statusTxPower[0] = '\0';

    // Load saved password if available
    loadSavedPassword();

    requestRedraw();
}

void RepeaterAdminScreen::onExit() {
    // Don't auto-logout, let user stay connected
}

void RepeaterAdminScreen::configureSoftKeys() {
    switch (_state) {
        case STATE_PASSWORD:
            SoftKeyBar::setLabels(_savePassword ? "NoSave" : "Save", "Login", "Back");
            break;
        case STATE_CONNECTING:
            SoftKeyBar::setLabels(nullptr, nullptr, "Cancel");
            break;
        case STATE_CONNECTED:
            SoftKeyBar::setLabels(nullptr, "Select", "Back");
            break;
        case STATE_CUSTOM_CMD:
            SoftKeyBar::setLabels("Clear", "Send", "Back");
            break;
        case STATE_SETTINGS:
            SoftKeyBar::setLabels("Refresh", "Edit", "Back");
            break;
        case STATE_EDIT_SETTING:
            SoftKeyBar::setLabels("Clear", "Apply", "Cancel");
            break;
        case STATE_STATUS:
            SoftKeyBar::setLabels("Refresh", nullptr, "Back");
            break;
        case STATE_DISCONNECTED:
            SoftKeyBar::setLabels(nullptr, "Retry", "Back");
            break;
    }
}

void RepeaterAdminScreen::setRepeater(uint32_t id, const uint8_t* pubKey, const char* name) {
    _repeaterId = id;
    if (pubKey) {
        memcpy(_repeaterPubKey, pubKey, 32);
    } else {
        memset(_repeaterPubKey, 0, 32);
    }
    strlcpy(_repeaterName, name ? name : "Unknown", sizeof(_repeaterName));
}

void RepeaterAdminScreen::loadSavedPassword() {
    ContactSettings& contacts = SettingsManager::getContactSettings();

    for (int i = 0; i < contacts.numContacts; i++) {
        ContactEntry* c = contacts.getContact(i);
        if (c && c->id == _repeaterId) {
            if (c->hasSavedPassword()) {
                strlcpy(_password, c->savedPassword, sizeof(_password));
                _passwordPos = strlen(_password);
            } else {
                _password[0] = '\0';
                _passwordPos = 0;
            }
            return;
        }
    }

    _password[0] = '\0';
    _passwordPos = 0;
}

void RepeaterAdminScreen::savePasswordToContact() {
    ContactSettings& contacts = SettingsManager::getContactSettings();

    for (int i = 0; i < contacts.numContacts; i++) {
        ContactEntry* c = contacts.getContact(i);
        if (c && c->id == _repeaterId) {
            contacts.savePassword(i, _password);
            SettingsManager::saveContacts();
            return;
        }
    }
}

void RepeaterAdminScreen::draw(bool fullRedraw) {
    switch (_state) {
        case STATE_PASSWORD:
            drawPasswordScreen(fullRedraw);
            break;
        case STATE_CONNECTING:
            drawConnectingScreen(fullRedraw);
            break;
        case STATE_CONNECTED:
            drawCommandMenu(fullRedraw);
            break;
        case STATE_CUSTOM_CMD:
            drawCustomCmdScreen(fullRedraw);
            break;
        case STATE_SETTINGS:
            drawSettingsMenu(fullRedraw);
            break;
        case STATE_EDIT_SETTING:
            drawEditSetting(fullRedraw);
            break;
        case STATE_STATUS:
            drawStatusScreen(fullRedraw);
            break;
        case STATE_DISCONNECTED:
            drawDisconnectedScreen(fullRedraw);
            break;
    }
}

void RepeaterAdminScreen::drawPasswordScreen(bool fullRedraw) {
    if (fullRedraw) {
        clearScreen();
        Display::drawText(12, Theme::CONTENT_Y + 8, "Admin Login", Theme::ACCENT, 2);
        Display::drawHLine(12, Theme::CONTENT_Y + 30, Theme::SCREEN_WIDTH - 24, Theme::DIVIDER);

        char buf[48];
        snprintf(buf, sizeof(buf), "Repeater: %s", _repeaterName);
        Display::drawText(16, Theme::CONTENT_Y + 40, buf, Theme::TEXT_SECONDARY, 1);
    }

    // Clear password area
    Display::fillRect(16, Theme::CONTENT_Y + 60, Theme::SCREEN_WIDTH - 32, 50, Theme::BG_PRIMARY);

    Display::drawText(16, Theme::CONTENT_Y + 65, "Password:", Theme::TEXT_SECONDARY, 1);
    Display::drawRoundRect(16, Theme::CONTENT_Y + 80, 200, 24, 4, Theme::GRAY_MID);

    // Show asterisks
    char masked[17];
    for (int i = 0; i < _passwordPos && i < 16; i++) {
        masked[i] = '*';
    }
    masked[_passwordPos] = '\0';
    Display::drawText(22, Theme::CONTENT_Y + 86, masked, Theme::WHITE, 1);

    // Cursor
    int cursorX = 22 + _passwordPos * 6;
    Display::fillRect(cursorX, Theme::CONTENT_Y + 85, 2, 14, Theme::ACCENT);

    // Save password checkbox
    const char* saveText = _savePassword ? "[x] Save password" : "[ ] Save password";
    Display::drawText(16, Theme::CONTENT_Y + 115, saveText, Theme::TEXT_SECONDARY, 1);

    if (_statusMessage[0]) {
        Display::drawText(16, Theme::CONTENT_Y + 140, _statusMessage, Theme::YELLOW, 1);
    }
}

void RepeaterAdminScreen::drawConnectingScreen(bool fullRedraw) {
    if (fullRedraw) {
        clearScreen();
        Display::drawText(12, Theme::CONTENT_Y + 8, "Admin Login", Theme::ACCENT, 2);
        Display::drawHLine(12, Theme::CONTENT_Y + 30, Theme::SCREEN_WIDTH - 24, Theme::DIVIDER);
    }

    Display::fillRect(0, Theme::CONTENT_Y + 50, Theme::SCREEN_WIDTH, 60, Theme::BG_PRIMARY);
    Display::drawTextCentered(0, Theme::CONTENT_Y + 70, Theme::SCREEN_WIDTH,
                              "Connecting...", Theme::ACCENT, 2);

    static int dots = 0;
    dots = (dots + 1) % 4;
    char dotStr[5] = "   ";
    for (int i = 0; i < dots; i++) dotStr[i] = '.';
    Display::drawText(Theme::SCREEN_WIDTH / 2 + 60, Theme::CONTENT_Y + 70, dotStr, Theme::ACCENT, 2);
}

void RepeaterAdminScreen::drawCommandMenu(bool fullRedraw) {
    // Always clear entire content area to prevent overlays
    clearScreen();

    Display::drawText(12, Theme::CONTENT_Y + 8, "Repeater Admin", Theme::ACCENT, 2);
    Display::drawHLine(12, Theme::CONTENT_Y + 30, Theme::SCREEN_WIDTH - 24, Theme::DIVIDER);

    int16_t y = Theme::CONTENT_Y + 36;

    // Compact status line: "Connected: Name (Admin)"
    char buf[64];
    const char* permName = "Guest";
    if (_permissions >= 3) permName = "Admin";
    else if (_permissions >= 2) permName = "Operator";
    snprintf(buf, sizeof(buf), "%s (%s)", _repeaterName, permName);
    Display::drawText(16, y, buf, Theme::GREEN, 1);
    y += 14;

    Display::drawHLine(16, y, Theme::SCREEN_WIDTH - 32, Theme::DIVIDER);
    y += 4;

    // Draw command list (8 items, 15px each = 120px total)
    for (int i = 0; i < NUM_COMMANDS; i++) {
        bool canExecute = (_permissions >= _commands[i].minPerm);
        bool isSelected = (i == _selectedCmd);

        // Draw selection background for ALL items consistently
        if (isSelected) {
            Display::fillRoundRect(12, y - 1, Theme::SCREEN_WIDTH - 24, 14, 3, Theme::BLUE);
        }

        uint16_t textColor = canExecute ? Theme::WHITE : Theme::GRAY_MID;
        Display::drawText(20, y, _commands[i].label, textColor, 1);

        // Show [locked] for items that require higher permissions (except Logout which is always available)
        if (!canExecute && _commands[i].minPerm > 0) {
            Display::drawText(200, y, "[locked]", Theme::GRAY_DARK, 1);
        }

        y += 15;
    }

    // Response area at bottom
    y += 2;
    Display::drawHLine(16, y, Theme::SCREEN_WIDTH - 32, Theme::DIVIDER);
    y += 4;

    if (_lastResponse[0]) {
        char truncated[50];
        strlcpy(truncated, _lastResponse, sizeof(truncated));
        Display::drawText(16, y, truncated, Theme::TEXT_SECONDARY, 1);
    }
}

void RepeaterAdminScreen::drawCustomCmdScreen(bool fullRedraw) {
    // Always clear to prevent overlay issues
    clearScreen();

    Display::drawText(12, Theme::CONTENT_Y + 8, "Custom Command", Theme::ACCENT, 2);
    Display::drawHLine(12, Theme::CONTENT_Y + 30, Theme::SCREEN_WIDTH - 24, Theme::DIVIDER);

    Display::drawText(16, Theme::CONTENT_Y + 45, "Enter CLI command:", Theme::TEXT_SECONDARY, 1);

    // Input box
    Display::drawRoundRect(16, Theme::CONTENT_Y + 62, Theme::SCREEN_WIDTH - 32, 24, 4, Theme::GRAY_MID);
    Display::drawText(22, Theme::CONTENT_Y + 68, _customCmd, Theme::WHITE, 1);

    // Cursor
    int cursorX = 22 + _customCmdPos * 6;
    if (cursorX < Theme::SCREEN_WIDTH - 30) {
        Display::fillRect(cursorX, Theme::CONTENT_Y + 67, 2, 14, Theme::ACCENT);
    }

    // Examples
    Display::drawText(16, Theme::CONTENT_Y + 100, "Examples:", Theme::TEXT_SECONDARY, 1);
    Display::drawText(16, Theme::CONTENT_Y + 116, "  status, uptime, reboot", Theme::GRAY_LIGHT, 1);
    Display::drawText(16, Theme::CONTENT_Y + 130, "  get name, set name X", Theme::GRAY_LIGHT, 1);
    Display::drawText(16, Theme::CONTENT_Y + 144, "  get advert_interval", Theme::GRAY_LIGHT, 1);
}

void RepeaterAdminScreen::drawSettingsMenu(bool fullRedraw) {
    clearScreen();

    Display::drawText(12, Theme::CONTENT_Y + 8, "Repeater Settings", Theme::ACCENT, 2);
    Display::drawHLine(12, Theme::CONTENT_Y + 30, Theme::SCREEN_WIDTH - 24, Theme::DIVIDER);

    int16_t y = Theme::CONTENT_Y + 40;

    // Instructions
    Display::drawText(16, y, "Select setting to edit:", Theme::TEXT_SECONDARY, 1);
    y += 20;

    // Draw settings list
    for (int i = 0; i < NUM_SETTINGS; i++) {
        bool isSelected = (i == _selectedSetting);

        if (isSelected) {
            Display::fillRoundRect(12, y - 2, Theme::SCREEN_WIDTH - 24, 28, 4, Theme::BLUE);
        }

        Display::drawText(20, y, _settings[i].label, Theme::WHITE, 1);

        // Show current value if loaded
        char valBuf[24];
        if (_settingsLoaded) {
            switch (i) {
                case 0:  // Advert Interval
                    snprintf(valBuf, sizeof(valBuf), "%d sec", _advertInterval);
                    break;
                case 1:  // Flood
                    snprintf(valBuf, sizeof(valBuf), "%s", _floodEnabled ? "ON" : "OFF");
                    break;
                case 2:  // Zero Hop
                    snprintf(valBuf, sizeof(valBuf), "%s", _zeroHopEnabled ? "ON" : "OFF");
                    break;
            }
            Display::drawText(180, y, valBuf, Theme::ACCENT, 1);
        } else {
            Display::drawText(180, y, "---", Theme::GRAY_MID, 1);
        }

        y += 30;
    }

    // Hint
    y += 10;
    Display::drawText(16, y, "Press Q/Refresh to load values", Theme::GRAY_LIGHT, 1);

    // Response
    if (_lastResponse[0]) {
        y += 20;
        Display::drawText(16, y, _lastResponse, Theme::TEXT_SECONDARY, 1);
    }
}

void RepeaterAdminScreen::drawEditSetting(bool fullRedraw) {
    clearScreen();

    char title[48];
    snprintf(title, sizeof(title), "Edit: %s", _settings[_selectedSetting].label);
    Display::drawText(12, Theme::CONTENT_Y + 8, title, Theme::ACCENT, 2);
    Display::drawHLine(12, Theme::CONTENT_Y + 30, Theme::SCREEN_WIDTH - 24, Theme::DIVIDER);

    int16_t y = Theme::CONTENT_Y + 45;

    // Show current value
    char currentVal[32];
    switch (_selectedSetting) {
        case 0:
            snprintf(currentVal, sizeof(currentVal), "Current: %d sec", _advertInterval);
            break;
        case 1:
            snprintf(currentVal, sizeof(currentVal), "Current: %s", _floodEnabled ? "ON" : "OFF");
            break;
        case 2:
            snprintf(currentVal, sizeof(currentVal), "Current: %s", _zeroHopEnabled ? "ON" : "OFF");
            break;
    }
    Display::drawText(16, y, currentVal, Theme::TEXT_SECONDARY, 1);
    y += 20;

    // Input prompt
    const char* prompt = "Enter new value:";
    if (_selectedSetting == 1 || _selectedSetting == 2) {
        prompt = "Enter 0/1 or on/off:";
    }
    Display::drawText(16, y, prompt, Theme::TEXT_SECONDARY, 1);
    y += 20;

    // Input box
    Display::drawRoundRect(16, y, Theme::SCREEN_WIDTH - 32, 24, 4, Theme::GRAY_MID);
    Display::drawText(22, y + 6, _settingValue, Theme::WHITE, 1);

    // Cursor
    int cursorX = 22 + _settingValuePos * 6;
    if (cursorX < Theme::SCREEN_WIDTH - 30) {
        Display::fillRect(cursorX, y + 5, 2, 14, Theme::ACCENT);
    }

    // Help text
    y += 40;
    if (_selectedSetting == 0) {
        Display::drawText(16, y, "Advert interval in seconds", Theme::GRAY_LIGHT, 1);
        Display::drawText(16, y + 14, "e.g., 300 = 5 minutes", Theme::GRAY_LIGHT, 1);
    } else {
        Display::drawText(16, y, "0 or off = disabled", Theme::GRAY_LIGHT, 1);
        Display::drawText(16, y + 14, "1 or on = enabled", Theme::GRAY_LIGHT, 1);
    }
}

void RepeaterAdminScreen::drawStatusScreen(bool fullRedraw) {
    clearScreen();

    Display::drawText(12, Theme::CONTENT_Y + 8, "Repeater Status", Theme::ACCENT, 2);
    Display::drawHLine(12, Theme::CONTENT_Y + 30, Theme::SCREEN_WIDTH - 24, Theme::DIVIDER);

    int16_t y = Theme::CONTENT_Y + 45;
    int16_t labelX = 16;
    int16_t valueX = 120;

    // Name
    Display::drawText(labelX, y, "Name:", Theme::TEXT_SECONDARY, 1);
    Display::drawText(valueX, y, _statusName[0] ? _statusName : "---", Theme::WHITE, 1);
    y += 18;

    // Uptime
    Display::drawText(labelX, y, "Uptime:", Theme::TEXT_SECONDARY, 1);
    Display::drawText(valueX, y, _statusUptime[0] ? _statusUptime : "---", Theme::WHITE, 1);
    y += 18;

    // Frequency
    Display::drawText(labelX, y, "Frequency:", Theme::TEXT_SECONDARY, 1);
    Display::drawText(valueX, y, _statusFreq[0] ? _statusFreq : "---", Theme::WHITE, 1);
    y += 18;

    // TX Power
    Display::drawText(labelX, y, "TX Power:", Theme::TEXT_SECONDARY, 1);
    Display::drawText(valueX, y, _statusTxPower[0] ? _statusTxPower : "---", Theme::WHITE, 1);
    y += 18;

    // Settings
    y += 8;
    Display::drawHLine(16, y, Theme::SCREEN_WIDTH - 32, Theme::DIVIDER);
    y += 12;

    char buf[32];
    Display::drawText(labelX, y, "Advert Int:", Theme::TEXT_SECONDARY, 1);
    if (_settingsLoaded) {
        snprintf(buf, sizeof(buf), "%d sec", _advertInterval);
        Display::drawText(valueX, y, buf, Theme::WHITE, 1);
    } else {
        Display::drawText(valueX, y, "---", Theme::GRAY_MID, 1);
    }
    y += 18;

    Display::drawText(labelX, y, "Flood:", Theme::TEXT_SECONDARY, 1);
    Display::drawText(valueX, y, _settingsLoaded ? (_floodEnabled ? "ON" : "OFF") : "---",
                      _floodEnabled ? Theme::GREEN : Theme::WHITE, 1);
    y += 18;

    Display::drawText(labelX, y, "Zero Hop:", Theme::TEXT_SECONDARY, 1);
    Display::drawText(valueX, y, _settingsLoaded ? (_zeroHopEnabled ? "ON" : "OFF") : "---",
                      _zeroHopEnabled ? Theme::GREEN : Theme::WHITE, 1);

    // Last response at bottom
    if (_lastResponse[0]) {
        Display::drawText(16, Theme::CONTENT_Y + Theme::CONTENT_HEIGHT - 20,
                          _lastResponse, Theme::GRAY_LIGHT, 1);
    }
}

void RepeaterAdminScreen::drawDisconnectedScreen(bool fullRedraw) {
    if (fullRedraw) {
        clearScreen();
        Display::drawText(12, Theme::CONTENT_Y + 8, "Disconnected", Theme::RED, 2);
        Display::drawHLine(12, Theme::CONTENT_Y + 30, Theme::SCREEN_WIDTH - 24, Theme::DIVIDER);
    }

    Display::drawTextCentered(0, Theme::CONTENT_Y + 80, Theme::SCREEN_WIDTH,
                              "Session ended", Theme::TEXT_SECONDARY, 1);

    if (_statusMessage[0]) {
        Display::drawTextCentered(0, Theme::CONTENT_Y + 110, Theme::SCREEN_WIDTH,
                                  _statusMessage, Theme::YELLOW, 1);
    }
}

bool RepeaterAdminScreen::handleInput(const InputData& input) {
    switch (_state) {
        case STATE_PASSWORD:
            return handlePasswordInput(input);
        case STATE_CONNECTING:
            if (input.event == InputEvent::BACK || input.event == InputEvent::SOFTKEY_RIGHT) {
                _state = STATE_PASSWORD;
                configureSoftKeys();
                requestRedraw();
                return true;
            }
            break;
        case STATE_CONNECTED:
            return handleCommandMenu(input);
        case STATE_CUSTOM_CMD:
            return handleCustomCmdInput(input);
        case STATE_SETTINGS:
            return handleSettingsMenu(input);
        case STATE_EDIT_SETTING:
            return handleEditSetting(input);
        case STATE_STATUS:
            return handleStatusScreen(input);
        case STATE_DISCONNECTED:
            if (input.event == InputEvent::BACK || input.event == InputEvent::SOFTKEY_RIGHT) {
                Screens.goBack();
                return true;
            }
            if (input.event == InputEvent::SOFTKEY_CENTER || input.event == InputEvent::TRACKBALL_CLICK) {
                _state = STATE_PASSWORD;
                configureSoftKeys();
                requestRedraw();
                return true;
            }
            break;
    }
    return false;
}

bool RepeaterAdminScreen::handlePasswordInput(const InputData& input) {
    switch (input.event) {
        case InputEvent::BACK:
        case InputEvent::SOFTKEY_RIGHT:
            Screens.goBack();
            return true;

        case InputEvent::SOFTKEY_LEFT:
            _savePassword = !_savePassword;
            configureSoftKeys();
            requestRedraw();
            return true;

        case InputEvent::SOFTKEY_CENTER:
        case InputEvent::TRACKBALL_CLICK:
            attemptLogin();
            return true;

        case InputEvent::KEY_PRESS:
            if (input.keyCode == KEY_ENTER) {
                attemptLogin();
                return true;
            }
            if (input.keyCode == KEY_BACKSPACE && _passwordPos > 0) {
                _password[--_passwordPos] = '\0';
                requestRedraw();
                return true;
            }
            if (input.keyChar >= 32 && input.keyChar < 127 && _passwordPos < 16) {
                _password[_passwordPos++] = input.keyChar;
                _password[_passwordPos] = '\0';
                requestRedraw();
                return true;
            }
            break;

        default:
            break;
    }
    return false;
}

bool RepeaterAdminScreen::handleCommandMenu(const InputData& input) {
    switch (input.event) {
        case InputEvent::BACK:
        case InputEvent::SOFTKEY_RIGHT:
            Screens.goBack();
            return true;

        case InputEvent::TRACKBALL_UP:
            if (_selectedCmd > 0) {
                _selectedCmd--;
                requestRedraw();
            }
            return true;

        case InputEvent::TRACKBALL_DOWN:
            if (_selectedCmd < NUM_COMMANDS - 1) {
                _selectedCmd++;
                requestRedraw();
            }
            return true;

        case InputEvent::SOFTKEY_CENTER:
        case InputEvent::TRACKBALL_CLICK:
            sendSelectedCommand();
            return true;

        case InputEvent::KEY_PRESS:
            if (input.keyCode == KEY_ENTER) {
                sendSelectedCommand();
                return true;
            }
            break;

        default:
            break;
    }
    return false;
}

bool RepeaterAdminScreen::handleCustomCmdInput(const InputData& input) {
    switch (input.event) {
        case InputEvent::BACK:
        case InputEvent::SOFTKEY_RIGHT:
            _state = STATE_CONNECTED;
            configureSoftKeys();
            requestRedraw();
            return true;

        case InputEvent::SOFTKEY_LEFT:
            _customCmd[0] = '\0';
            _customCmdPos = 0;
            requestRedraw();
            return true;

        case InputEvent::SOFTKEY_CENTER:
        case InputEvent::TRACKBALL_CLICK:
            sendCustomCommand();
            return true;

        case InputEvent::KEY_PRESS:
            if (input.keyCode == KEY_ENTER) {
                sendCustomCommand();
                return true;
            }
            if (input.keyCode == KEY_BACKSPACE && _customCmdPos > 0) {
                _customCmd[--_customCmdPos] = '\0';
                requestRedraw();
                return true;
            }
            if (input.keyChar >= 32 && input.keyChar < 127 && _customCmdPos < 62) {
                _customCmd[_customCmdPos++] = input.keyChar;
                _customCmd[_customCmdPos] = '\0';
                requestRedraw();
                return true;
            }
            break;

        default:
            break;
    }
    return false;
}

bool RepeaterAdminScreen::handleSettingsMenu(const InputData& input) {
    switch (input.event) {
        case InputEvent::BACK:
        case InputEvent::SOFTKEY_RIGHT:
            _state = STATE_CONNECTED;
            configureSoftKeys();
            requestRedraw();
            return true;

        case InputEvent::SOFTKEY_LEFT:
            // Refresh - fetch current settings
            fetchSettings();
            return true;

        case InputEvent::TRACKBALL_UP:
            if (_selectedSetting > 0) {
                _selectedSetting--;
                requestRedraw();
            }
            return true;

        case InputEvent::TRACKBALL_DOWN:
            if (_selectedSetting < NUM_SETTINGS - 1) {
                _selectedSetting++;
                requestRedraw();
            }
            return true;

        case InputEvent::SOFTKEY_CENTER:
        case InputEvent::TRACKBALL_CLICK:
            // Edit selected setting
            _settingValue[0] = '\0';
            _settingValuePos = 0;
            _state = STATE_EDIT_SETTING;
            configureSoftKeys();
            requestRedraw();
            return true;

        case InputEvent::KEY_PRESS:
            if (input.keyCode == KEY_ENTER) {
                _settingValue[0] = '\0';
                _settingValuePos = 0;
                _state = STATE_EDIT_SETTING;
                configureSoftKeys();
                requestRedraw();
                return true;
            }
            break;

        default:
            break;
    }
    return false;
}

bool RepeaterAdminScreen::handleEditSetting(const InputData& input) {
    switch (input.event) {
        case InputEvent::BACK:
        case InputEvent::SOFTKEY_RIGHT:
            _state = STATE_SETTINGS;
            configureSoftKeys();
            requestRedraw();
            return true;

        case InputEvent::SOFTKEY_LEFT:
            _settingValue[0] = '\0';
            _settingValuePos = 0;
            requestRedraw();
            return true;

        case InputEvent::SOFTKEY_CENTER:
        case InputEvent::TRACKBALL_CLICK:
            applyCurrentSetting();
            return true;

        case InputEvent::KEY_PRESS:
            if (input.keyCode == KEY_ENTER) {
                applyCurrentSetting();
                return true;
            }
            if (input.keyCode == KEY_BACKSPACE && _settingValuePos > 0) {
                _settingValue[--_settingValuePos] = '\0';
                requestRedraw();
                return true;
            }
            if (input.keyChar >= 32 && input.keyChar < 127 && _settingValuePos < 30) {
                _settingValue[_settingValuePos++] = input.keyChar;
                _settingValue[_settingValuePos] = '\0';
                requestRedraw();
                return true;
            }
            break;

        default:
            break;
    }
    return false;
}

bool RepeaterAdminScreen::handleStatusScreen(const InputData& input) {
    switch (input.event) {
        case InputEvent::BACK:
        case InputEvent::SOFTKEY_RIGHT:
            _state = STATE_CONNECTED;
            configureSoftKeys();
            requestRedraw();
            return true;

        case InputEvent::SOFTKEY_LEFT:
            // Refresh status
            fetchStatus();
            return true;

        default:
            break;
    }
    return false;
}

void RepeaterAdminScreen::attemptLogin() {
    if (!_mesh) {
        strlcpy(_statusMessage, "Error: No mesh", sizeof(_statusMessage));
        requestRedraw();
        return;
    }

    bool hasKey = false;
    for (int i = 0; i < 32; i++) {
        if (_repeaterPubKey[i] != 0) {
            hasKey = true;
            break;
        }
    }

    if (!hasKey) {
        strlcpy(_statusMessage, "No pubkey - wait for advert", sizeof(_statusMessage));
        requestRedraw();
        return;
    }

    if (_mesh->sendRepeaterLogin(_repeaterId, _repeaterPubKey, _password)) {
        _state = STATE_CONNECTING;
        strlcpy(_statusMessage, "", sizeof(_statusMessage));
        configureSoftKeys();
        requestRedraw();
    } else {
        strlcpy(_statusMessage, "Failed to send login", sizeof(_statusMessage));
        requestRedraw();
    }
}

void RepeaterAdminScreen::sendSelectedCommand() {
    if (!_mesh || !_isConnected) return;

    // Handle special menu items
    switch (_selectedCmd) {
        case 0:  // View Status
            fetchStatus();
            _state = STATE_STATUS;
            configureSoftKeys();
            requestRedraw();
            return;

        case 1:  // Settings
            if (_permissions < 2) {
                strlcpy(_lastResponse, "Permission denied", sizeof(_lastResponse));
                requestRedraw();
                return;
            }
            _state = STATE_SETTINGS;
            configureSoftKeys();
            requestRedraw();
            return;

        case 6:  // Terminal
            if (_permissions < 2) {
                strlcpy(_lastResponse, "Permission denied", sizeof(_lastResponse));
                requestRedraw();
                return;
            }
            // Navigate to CLI screen
            if (repeaterCLIScreen) {
                repeaterCLIScreen->setRepeater(_repeaterName);
                Screens.navigateTo(ScreenId::REPEATER_CLI);
            }
            return;

        case 7:  // Logout
            logout();
            return;
    }

    // Check permission for regular commands
    if (_permissions < _commands[_selectedCmd].minPerm) {
        strlcpy(_lastResponse, "Permission denied", sizeof(_lastResponse));
        requestRedraw();
        return;
    }

    // Send the command
    if (_commands[_selectedCmd].command && _mesh->sendRepeaterCommand(_commands[_selectedCmd].command)) {
        char msg[48];
        snprintf(msg, sizeof(msg), "Sent: %s", _commands[_selectedCmd].command);
        strlcpy(_lastResponse, msg, sizeof(_lastResponse));
    } else {
        strlcpy(_lastResponse, "Failed to send", sizeof(_lastResponse));
    }
    requestRedraw();
}

void RepeaterAdminScreen::sendCustomCommand() {
    if (!_mesh || !_isConnected || _customCmdPos == 0) {
        return;
    }

    if (_mesh->sendRepeaterCommand(_customCmd)) {
        char msg[80];
        snprintf(msg, sizeof(msg), "Sent: %s", _customCmd);
        strlcpy(_lastResponse, msg, sizeof(_lastResponse));
        _state = STATE_CONNECTED;
        configureSoftKeys();
    } else {
        strlcpy(_lastResponse, "Failed to send", sizeof(_lastResponse));
    }
    requestRedraw();
}

void RepeaterAdminScreen::logout() {
    if (_mesh) {
        _mesh->disconnectRepeater();
    }
    _isConnected = false;
    _permissions = 0;
    strlcpy(_statusMessage, "Logged out", sizeof(_statusMessage));
    _state = STATE_DISCONNECTED;
    configureSoftKeys();
    requestRedraw();
}

void RepeaterAdminScreen::fetchSettings() {
    if (!_mesh || !_isConnected) return;

    // Send get commands for all settings
    // Note: We'll parse responses as they come in via onCLIResponse
    _mesh->sendRepeaterCommand("get advert_interval");
    strlcpy(_lastResponse, "Fetching settings...", sizeof(_lastResponse));
    requestRedraw();

    // The responses will update _advertInterval, _floodEnabled, _zeroHopEnabled
    // This is a simplified approach - in practice you might want to queue these
}

void RepeaterAdminScreen::fetchStatus() {
    if (!_mesh || !_isConnected) return;

    // Clear old status
    _statusName[0] = '\0';
    _statusUptime[0] = '\0';
    _statusFreq[0] = '\0';
    _statusTxPower[0] = '\0';

    // Request status info
    _mesh->sendRepeaterCommand("status");
    strlcpy(_lastResponse, "Fetching status...", sizeof(_lastResponse));
    requestRedraw();
}

void RepeaterAdminScreen::applyCurrentSetting() {
    if (!_mesh || !_isConnected || _settingValuePos == 0) {
        return;
    }

    char cmd[64];
    snprintf(cmd, sizeof(cmd), "%s %s", _settings[_selectedSetting].setCmd, _settingValue);

    if (_mesh->sendRepeaterCommand(cmd)) {
        char msg[80];
        snprintf(msg, sizeof(msg), "Sent: %s", cmd);
        strlcpy(_lastResponse, msg, sizeof(_lastResponse));

        // Optimistically update local cache based on what we sent
        if (_selectedSetting == 0) {
            _advertInterval = atoi(_settingValue);
        } else if (_selectedSetting == 1) {
            _floodEnabled = (strcmp(_settingValue, "1") == 0 ||
                            strcasecmp(_settingValue, "on") == 0);
        } else if (_selectedSetting == 2) {
            _zeroHopEnabled = (strcmp(_settingValue, "1") == 0 ||
                              strcasecmp(_settingValue, "on") == 0);
        }
        _settingsLoaded = true;

        _state = STATE_SETTINGS;
        configureSoftKeys();
    } else {
        strlcpy(_lastResponse, "Failed to send", sizeof(_lastResponse));
    }
    requestRedraw();
}

void RepeaterAdminScreen::parseStatusResponse(const char* response) {
    // Try to parse common status fields from response
    // Format varies, but often includes "name:", "uptime:", "freq:", "power:"
    // This is best-effort parsing

    if (strstr(response, "name") || strstr(response, "Name")) {
        // Try to extract name
        const char* p = strstr(response, ":");
        if (p) {
            p++;
            while (*p == ' ') p++;
            strlcpy(_statusName, p, sizeof(_statusName));
            // Trim at newline or comma
            char* end = strpbrk(_statusName, "\n,");
            if (end) *end = '\0';
        }
    }
}

// Callbacks from mesh

void RepeaterAdminScreen::onLoginSuccess(uint8_t permissions) {
    _isConnected = true;
    _permissions = permissions;
    _state = STATE_CONNECTED;

    if (_savePassword) {
        savePasswordToContact();
    }

    strlcpy(_lastResponse, "Login successful", sizeof(_lastResponse));
    configureSoftKeys();
    requestRedraw();
}

void RepeaterAdminScreen::onLoginFailed() {
    _isConnected = false;
    _state = STATE_PASSWORD;
    strlcpy(_statusMessage, "Login failed - bad password?", sizeof(_statusMessage));
    configureSoftKeys();
    requestRedraw();
}

void RepeaterAdminScreen::onCLIResponse(const char* response) {
    if (!response) return;

    strlcpy(_lastResponse, response, sizeof(_lastResponse));

    // Try to parse settings from response
    if (strstr(response, "advert_interval") || strstr(response, "advert interval")) {
        // Extract number
        const char* p = response;
        while (*p && !isdigit(*p)) p++;
        if (*p) {
            _advertInterval = atoi(p);
            _settingsLoaded = true;
        }
    } else if (strstr(response, "flood")) {
        _floodEnabled = (strstr(response, "on") || strstr(response, "ON") ||
                        strstr(response, "true") || strstr(response, "1"));
        _settingsLoaded = true;
    } else if (strstr(response, "zero_hop") || strstr(response, "zero hop")) {
        _zeroHopEnabled = (strstr(response, "on") || strstr(response, "ON") ||
                          strstr(response, "true") || strstr(response, "1"));
        _settingsLoaded = true;
    } else if (strstr(response, "uptime")) {
        strlcpy(_statusUptime, response, sizeof(_statusUptime));
    }

    // Also try generic status parsing
    parseStatusResponse(response);

    requestRedraw();
}

void RepeaterAdminScreen::onDisconnected() {
    _isConnected = false;
    _permissions = 0;
    _state = STATE_DISCONNECTED;
    strlcpy(_statusMessage, "Connection lost", sizeof(_statusMessage));
    configureSoftKeys();
    requestRedraw();
}
