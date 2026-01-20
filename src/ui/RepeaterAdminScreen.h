/**
 * MeshBerry Repeater Admin Screen
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2026 NodakMesh (nodakmesh.org)
 *
 * Dedicated screen for repeater administration:
 * - Login/logout
 * - Send commands (status, uptime, reboot, custom)
 * - Configure settings (advert interval, flood, zero hop)
 * - View CLI responses
 */

#ifndef MESHBERRY_REPEATER_ADMIN_SCREEN_H
#define MESHBERRY_REPEATER_ADMIN_SCREEN_H

#include "Screen.h"
#include "ScreenManager.h"

// Forward declaration
class MeshBerryMesh;

class RepeaterAdminScreen : public Screen {
public:
    ScreenId getId() const override { return ScreenId::REPEATER_ADMIN; }
    void onEnter() override;
    void onExit() override;
    void draw(bool fullRedraw) override;
    bool handleInput(const InputData& input) override;
    const char* getTitle() const override { return "Repeater Admin"; }
    void configureSoftKeys() override;

    // Set the mesh instance for sending commands
    void setMesh(MeshBerryMesh* mesh) { _mesh = mesh; }

    // Set the repeater to administer (call before navigating here)
    void setRepeater(uint32_t id, const uint8_t* pubKey, const char* name);

    // Callbacks from mesh layer
    void onLoginSuccess(uint8_t permissions);
    void onLoginFailed();
    void onCLIResponse(const char* response);
    void onDisconnected();

private:
    enum State {
        STATE_PASSWORD,     // Entering password
        STATE_CONNECTING,   // Waiting for login response
        STATE_CONNECTED,    // Logged in, showing command menu
        STATE_CUSTOM_CMD,   // Entering custom command
        STATE_SETTINGS,     // Settings menu
        STATE_EDIT_SETTING, // Editing a setting value
        STATE_STATUS,       // Viewing status info
        STATE_DISCONNECTED  // Session ended
    };

    State _state = STATE_PASSWORD;
    MeshBerryMesh* _mesh = nullptr;

    // Repeater info
    uint32_t _repeaterId = 0;
    uint8_t _repeaterPubKey[32];
    char _repeaterName[32];

    // Password input
    char _password[17];
    int _passwordPos = 0;
    bool _savePassword = true;

    // Custom command input
    char _customCmd[64];
    int _customCmdPos = 0;

    // Session info
    uint8_t _permissions = 0;
    bool _isConnected = false;

    // Command menu
    int _selectedCmd = 0;
    static const int NUM_COMMANDS = 8;

    // Settings menu
    int _selectedSetting = 0;
    static const int NUM_SETTINGS = 3;

    // Setting values (editable)
    char _settingValue[32];
    int _settingValuePos = 0;

    // Cached settings from repeater
    int _advertInterval = 0;    // seconds
    bool _floodEnabled = false;
    bool _zeroHopEnabled = false;
    bool _settingsLoaded = false;

    // Response display
    char _lastResponse[128];
    char _statusMessage[48];

    // Status info
    char _statusName[32];
    char _statusUptime[32];
    char _statusFreq[16];
    char _statusTxPower[8];

    // Predefined commands
    struct Command {
        const char* label;
        const char* command;
        uint8_t minPerm;  // Minimum permission level
    };
    static const Command _commands[NUM_COMMANDS];

    // Settings definitions
    struct Setting {
        const char* label;
        const char* getCmd;
        const char* setCmd;
        uint8_t minPerm;
    };
    static const Setting _settings[NUM_SETTINGS];

    void drawPasswordScreen(bool fullRedraw);
    void drawConnectingScreen(bool fullRedraw);
    void drawCommandMenu(bool fullRedraw);
    void drawCustomCmdScreen(bool fullRedraw);
    void drawSettingsMenu(bool fullRedraw);
    void drawEditSetting(bool fullRedraw);
    void drawStatusScreen(bool fullRedraw);
    void drawDisconnectedScreen(bool fullRedraw);

    bool handlePasswordInput(const InputData& input);
    bool handleCommandMenu(const InputData& input);
    bool handleCustomCmdInput(const InputData& input);
    bool handleSettingsMenu(const InputData& input);
    bool handleEditSetting(const InputData& input);
    bool handleStatusScreen(const InputData& input);

    void attemptLogin();
    void sendSelectedCommand();
    void sendCustomCommand();
    void logout();
    void loadSavedPassword();
    void savePasswordToContact();

    void fetchSettings();
    void fetchStatus();
    void applyCurrentSetting();
    void parseStatusResponse(const char* response);
    void clearScreen();
};

#endif // MESHBERRY_REPEATER_ADMIN_SCREEN_H
