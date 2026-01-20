/**
 * MeshBerry Repeater CLI Screen
 *
 * Interactive terminal-style interface for sending CLI commands to repeaters.
 * Features scrollable command/response history and text input.
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2026 NodakMesh (nodakmesh.org)
 */

#ifndef MESHBERRY_REPEATER_CLI_SCREEN_H
#define MESHBERRY_REPEATER_CLI_SCREEN_H

#include "Screen.h"
#include "Theme.h"

class RepeaterCLIScreen : public Screen {
public:
    RepeaterCLIScreen();

    ScreenId getId() const override { return ScreenId::REPEATER_CLI; }
    void onEnter() override;
    void onExit() override;
    void draw(bool fullRedraw) override;
    bool handleInput(const InputData& input) override;
    const char* getTitle() const override { return "Terminal"; }
    void configureSoftKeys() override;

    // Set repeater info (called before navigating here)
    void setRepeater(const char* name);

    // Callback from mesh layer - adds response to history
    void onCLIResponse(const char* response);

    // Clear history (called on disconnect)
    void clearHistory();

private:
    static const int MAX_HISTORY = 32;
    static const int MAX_LINE_LEN = 128;
    static const int MAX_INPUT_LEN = 64;

    // History entry
    struct CLIEntry {
        char text[MAX_LINE_LEN];
        bool isCommand;  // true = command (green), false = response (white)
    };

    // History buffer
    CLIEntry _history[MAX_HISTORY];
    int _historyCount = 0;
    int _scrollOffset = 0;  // 0 = at bottom, positive = scrolled up

    // Input buffer
    char _inputBuffer[MAX_INPUT_LEN];
    int _inputPos = 0;

    // Repeater info
    char _repeaterName[32];

    // Cursor blink state
    uint32_t _lastBlinkTime = 0;
    bool _cursorVisible = true;

    // Internal methods
    void addToHistory(const char* text, bool isCommand);
    void sendCommand();
    void clearInput();
    void clearScreen();
    void showHelp();  // Display local help for MeshCore CLI commands

    void drawHeader();
    void drawHistory();
    void drawInputLine();

    int getVisibleLines() const;
    int getMaxScrollOffset() const;
};

#endif // MESHBERRY_REPEATER_CLI_SCREEN_H
