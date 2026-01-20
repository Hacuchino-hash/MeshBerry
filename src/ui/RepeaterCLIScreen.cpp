/**
 * MeshBerry Repeater CLI Screen Implementation
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2026 NodakMesh (nodakmesh.org)
 */

#include "RepeaterCLIScreen.h"
#include "SoftKeyBar.h"
#include "ScreenManager.h"
#include "../drivers/display.h"
#include "../drivers/keyboard.h"
#include "../mesh/MeshBerryMesh.h"
#include <Arduino.h>
#include <string.h>

// External mesh instance
extern MeshBerryMesh* theMesh;

// Layout constants
static const int16_t HEADER_HEIGHT = 24;
static const int16_t INPUT_HEIGHT = 20;
static const int16_t LINE_HEIGHT = 12;
static const int16_t CONTENT_TOP = Theme::CONTENT_Y + HEADER_HEIGHT;
static const int16_t CONTENT_BOTTOM = Theme::CONTENT_Y + Theme::CONTENT_HEIGHT - INPUT_HEIGHT;

RepeaterCLIScreen::RepeaterCLIScreen() {
    memset(_history, 0, sizeof(_history));
    memset(_inputBuffer, 0, sizeof(_inputBuffer));
    memset(_repeaterName, 0, sizeof(_repeaterName));
}

void RepeaterCLIScreen::setRepeater(const char* name) {
    if (name) {
        strlcpy(_repeaterName, name, sizeof(_repeaterName));
    }
}

void RepeaterCLIScreen::onEnter() {
    _scrollOffset = 0;
    _cursorVisible = true;
    _lastBlinkTime = millis();
    configureSoftKeys();
    requestRedraw();
}

void RepeaterCLIScreen::onExit() {
    // Keep history - user might come back
}

void RepeaterCLIScreen::configureSoftKeys() {
    SoftKeyBar::setLabels("Clear", "Send", "Back");
    SoftKeyBar::redraw();
}

void RepeaterCLIScreen::draw(bool fullRedraw) {
    if (fullRedraw) {
        clearScreen();
        drawHeader();
    }

    drawHistory();
    drawInputLine();
}

void RepeaterCLIScreen::drawHeader() {
    // Title bar
    char title[48];
    snprintf(title, sizeof(title), "%s CLI", _repeaterName);
    Display::drawText(12, Theme::CONTENT_Y + 6, title, Theme::ACCENT, 2);

    // Divider
    Display::drawHLine(8, Theme::CONTENT_Y + HEADER_HEIGHT - 2,
                       Theme::SCREEN_WIDTH - 16, Theme::DIVIDER);
}

void RepeaterCLIScreen::drawHistory() {
    // Clear history area
    Display::fillRect(0, CONTENT_TOP, Theme::SCREEN_WIDTH, CONTENT_BOTTOM - CONTENT_TOP, Theme::BG_PRIMARY);

    int visibleLines = getVisibleLines();
    if (_historyCount == 0) {
        Display::drawText(12, CONTENT_TOP + 20, "Type a command below", Theme::TEXT_SECONDARY, 1);
        Display::drawText(12, CONTENT_TOP + 36, "Examples: status, help", Theme::TEXT_SECONDARY, 1);
        return;
    }

    // Calculate which lines to show
    int startIdx = _historyCount - visibleLines - _scrollOffset;
    if (startIdx < 0) startIdx = 0;

    int endIdx = startIdx + visibleLines;
    if (endIdx > _historyCount) endIdx = _historyCount;

    // Draw visible history
    int16_t y = CONTENT_TOP + 2;
    for (int i = startIdx; i < endIdx && y < CONTENT_BOTTOM - LINE_HEIGHT; i++) {
        uint16_t color = _history[i].isCommand ? Theme::GREEN : Theme::WHITE;

        // Truncate long lines to fit screen
        char line[48];
        strlcpy(line, _history[i].text, sizeof(line));
        Display::drawText(8, y, line, color, 1);

        y += LINE_HEIGHT;
    }

    // Scroll indicators
    if (startIdx > 0) {
        // More content above
        Display::drawText(Theme::SCREEN_WIDTH - 20, CONTENT_TOP + 2, "^", Theme::TEXT_SECONDARY, 1);
    }
    if (endIdx < _historyCount) {
        // More content below
        Display::drawText(Theme::SCREEN_WIDTH - 20, CONTENT_BOTTOM - LINE_HEIGHT, "v", Theme::TEXT_SECONDARY, 1);
    }
}

void RepeaterCLIScreen::drawInputLine() {
    int16_t inputY = CONTENT_BOTTOM;

    // Input area background
    Display::fillRect(0, inputY, Theme::SCREEN_WIDTH, INPUT_HEIGHT, Theme::BG_SECONDARY);

    // Prompt
    Display::drawText(8, inputY + 4, ">", Theme::GREEN, 1);

    // Input text (truncated from left if too long)
    int maxChars = 42;  // Approximate chars that fit
    const char* displayText = _inputBuffer;
    int len = strlen(_inputBuffer);
    if (len > maxChars) {
        displayText = _inputBuffer + (len - maxChars);
    }
    Display::drawText(20, inputY + 4, displayText, Theme::WHITE, 1);

    // Cursor (blinking)
    uint32_t now = millis();
    if (now - _lastBlinkTime > 500) {
        _cursorVisible = !_cursorVisible;
        _lastBlinkTime = now;
    }

    if (_cursorVisible) {
        int cursorX = 20 + strlen(displayText) * 6;  // 6px per char at size 1
        if (cursorX > Theme::SCREEN_WIDTH - 16) cursorX = Theme::SCREEN_WIDTH - 16;
        Display::fillRect(cursorX, inputY + 4, 6, 8, Theme::ACCENT);
    }
}

bool RepeaterCLIScreen::handleInput(const InputData& input) {
    switch (input.event) {
        case InputEvent::KEY_PRESS:
            if (input.keyChar >= 32 && input.keyChar < 127) {
                // Add printable character
                if (_inputPos < MAX_INPUT_LEN - 1) {
                    _inputBuffer[_inputPos++] = input.keyChar;
                    _inputBuffer[_inputPos] = '\0';
                    requestRedraw();
                }
                return true;
            } else if (input.keyCode == KEY_BACKSPACE) {
                // Delete character
                if (_inputPos > 0) {
                    _inputBuffer[--_inputPos] = '\0';
                    requestRedraw();
                }
                return true;
            } else if (input.keyCode == KEY_ENTER) {
                // Send command
                sendCommand();
                return true;
            }
            break;

        case InputEvent::SOFTKEY_LEFT:
            // Clear input
            clearInput();
            requestRedraw();
            return true;

        case InputEvent::SOFTKEY_CENTER:
            // Send command
            sendCommand();
            return true;

        case InputEvent::SOFTKEY_RIGHT:
        case InputEvent::BACK:
            // Go back to admin screen
            Screens.goBack();
            return true;

        case InputEvent::TRACKBALL_UP:
            // Scroll up (show older)
            if (_scrollOffset < getMaxScrollOffset()) {
                _scrollOffset++;
                requestRedraw();
            }
            return true;

        case InputEvent::TRACKBALL_DOWN:
            // Scroll down (show newer)
            if (_scrollOffset > 0) {
                _scrollOffset--;
                requestRedraw();
            }
            return true;

        default:
            break;
    }

    return false;
}

void RepeaterCLIScreen::addToHistory(const char* text, bool isCommand) {
    if (!text || strlen(text) == 0) return;

    // If history is full, shift everything up
    if (_historyCount >= MAX_HISTORY) {
        memmove(&_history[0], &_history[1], sizeof(CLIEntry) * (MAX_HISTORY - 1));
        _historyCount = MAX_HISTORY - 1;
    }

    // Add new entry
    strlcpy(_history[_historyCount].text, text, MAX_LINE_LEN);
    _history[_historyCount].isCommand = isCommand;
    _historyCount++;

    // Auto-scroll to bottom
    _scrollOffset = 0;
    requestRedraw();
}

void RepeaterCLIScreen::sendCommand() {
    if (_inputPos == 0) return;

    // Add command to history with "> " prefix
    char cmdLine[MAX_LINE_LEN];
    snprintf(cmdLine, sizeof(cmdLine), "> %s", _inputBuffer);
    addToHistory(cmdLine, true);

    // Handle local "help" command (case-insensitive)
    if (strcasecmp(_inputBuffer, "help") == 0 || strcmp(_inputBuffer, "?") == 0) {
        showHelp();
        clearInput();
        return;
    }

    // Send to repeater
    if (theMesh && theMesh->isRepeaterConnected()) {
        if (!theMesh->sendRepeaterCommand(_inputBuffer)) {
            addToHistory("Error: Failed to send", false);
        }
    } else {
        addToHistory("Error: Not connected", false);
    }

    clearInput();
}

void RepeaterCLIScreen::clearInput() {
    _inputPos = 0;
    _inputBuffer[0] = '\0';
}

void RepeaterCLIScreen::clearScreen() {
    Display::fillRect(0, Theme::CONTENT_Y,
                      Theme::SCREEN_WIDTH, Theme::CONTENT_HEIGHT,
                      Theme::BG_PRIMARY);
}

void RepeaterCLIScreen::clearHistory() {
    _historyCount = 0;
    _scrollOffset = 0;
    memset(_history, 0, sizeof(_history));
}

void RepeaterCLIScreen::showHelp() {
    addToHistory("=== MeshCore CLI Commands ===", false);
    addToHistory("", false);
    addToHistory("System:", false);
    addToHistory("  reboot, ver, board, clock, advert", false);
    addToHistory("", false);
    addToHistory("Get Settings:", false);
    addToHistory("  get name, get freq, get tx, get radio", false);
    addToHistory("  get repeat, get advert.interval", false);
    addToHistory("  get flood.max, get lat, get lon", false);
    addToHistory("", false);
    addToHistory("Set Settings:", false);
    addToHistory("  set name <name>", false);
    addToHistory("  set tx <1-30>", false);
    addToHistory("  set repeat <on|off>", false);
    addToHistory("  set advert.interval <60-240>", false);
    addToHistory("  set flood.max <0-64>", false);
    addToHistory("  set lat/lon <value>", false);
    addToHistory("  password <new_password>", false);
    addToHistory("", false);
    addToHistory("Info:", false);
    addToHistory("  neighbors, clear stats", false);
    addToHistory("  log start|stop|erase", false);
    addToHistory("", false);
    addToHistory("GPS (if available):", false);
    addToHistory("  gps, gps on|off, gps sync", false);
}

void RepeaterCLIScreen::onCLIResponse(const char* response) {
    if (!response) return;

    // Split multi-line responses
    const char* start = response;
    const char* p = response;

    while (*p) {
        if (*p == '\n' || *p == '\r') {
            // Add line up to newline
            if (p > start) {
                char line[MAX_LINE_LEN];
                size_t len = p - start;
                if (len >= MAX_LINE_LEN) len = MAX_LINE_LEN - 1;
                strncpy(line, start, len);
                line[len] = '\0';
                addToHistory(line, false);
            }
            // Skip consecutive newlines
            while (*p == '\n' || *p == '\r') p++;
            start = p;
        } else {
            p++;
        }
    }

    // Add remaining text
    if (p > start && strlen(start) > 0) {
        addToHistory(start, false);
    }

    requestRedraw();
}

int RepeaterCLIScreen::getVisibleLines() const {
    return (CONTENT_BOTTOM - CONTENT_TOP - 4) / LINE_HEIGHT;
}

int RepeaterCLIScreen::getMaxScrollOffset() const {
    int visible = getVisibleLines();
    int max = _historyCount - visible;
    return max > 0 ? max : 0;
}
