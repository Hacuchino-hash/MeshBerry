/**
 * MeshBerry Channel UI Implementation
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2026 NodakMesh (nodakmesh.org)
 *
 * This file is part of MeshBerry.
 */

#include "ChannelUI.h"
#include "../drivers/display.h"
#include "../drivers/keyboard.h"
#include "../settings/SettingsManager.h"
#include <string.h>

namespace ChannelUI {

// UI state
static State currentState = STATE_LIST;
static int selectedIndex = 0;
static int addTypeSelection = 0;  // 0 = hashtag, 1 = custom PSK

// Input buffers for adding channels
static char inputName[32] = "";
static char inputPSK[64] = "";
static int inputPos = 0;

void init() {
    currentState = STATE_LIST;
    selectedIndex = 0;
    inputName[0] = '\0';
    inputPSK[0] = '\0';
    inputPos = 0;
}

void show() {
    Display::clear(COLOR_BACKGROUND);

    ChannelSettings& settings = SettingsManager::getChannelSettings();

    switch (currentState) {
        case STATE_LIST: {
            Display::drawText(10, 10, "Channels", COLOR_ACCENT, 2);

            int y = 45;
            const int lineHeight = 22;

            for (int i = 0; i < settings.numChannels && i < 7; i++) {
                const ChannelEntry& ch = settings.channels[i];
                char buf[48];

                // Selection indicator
                const char* prefix = (i == selectedIndex) ? ">" : " ";

                snprintf(buf, sizeof(buf), "%s %s", prefix, ch.name);
                uint16_t color = (i == selectedIndex) ? COLOR_ACCENT : COLOR_TEXT;
                Display::drawText(10, y, buf, color, 1);
                y += lineHeight;
            }

            // Instructions
            Display::drawText(10, 185, "UP/DN=Select  ENTER=Switch", COLOR_WARNING, 1);
            Display::drawText(10, 205, "A=Add  D=Delete  ESC=Back", COLOR_WARNING, 1);
            break;
        }

        case STATE_ADD_TYPE: {
            Display::drawText(10, 10, "Add Channel", COLOR_ACCENT, 2);
            Display::drawText(10, 50, "Choose type:", COLOR_TEXT, 1);

            const char* opt1 = (addTypeSelection == 0) ? "> #hashtag (auto-key)" : "  #hashtag (auto-key)";
            const char* opt2 = (addTypeSelection == 1) ? "> Custom PSK" : "  Custom PSK";

            Display::drawText(20, 80, opt1, addTypeSelection == 0 ? COLOR_ACCENT : COLOR_TEXT, 1);
            Display::drawText(20, 105, opt2, addTypeSelection == 1 ? COLOR_ACCENT : COLOR_TEXT, 1);

            Display::drawText(10, 150, "Key auto-generated from name", COLOR_TEXT, 1);
            Display::drawText(10, 170, "for #hashtag channels.", COLOR_TEXT, 1);

            Display::drawText(10, 205, "UP/DN=Select  ENTER=Next  ESC=Cancel", COLOR_WARNING, 1);
            break;
        }

        case STATE_ADD_HASHTAG: {
            Display::drawText(10, 10, "Add #Channel", COLOR_ACCENT, 2);
            Display::drawText(10, 50, "Enter channel name:", COLOR_TEXT, 1);

            // Input box
            Display::drawRect(10, 75, 300, 28, COLOR_TEXT);
            char displayBuf[34];
            snprintf(displayBuf, sizeof(displayBuf), "#%s_", inputName);
            Display::drawText(15, 82, displayBuf, COLOR_ACCENT, 1);

            Display::drawText(10, 120, "(Key will be auto-generated)", COLOR_TEXT, 1);
            Display::drawText(10, 150, "Anyone with same #name", COLOR_TEXT, 1);
            Display::drawText(10, 170, "will share the channel.", COLOR_TEXT, 1);

            Display::drawText(10, 205, "ENTER=Create  ESC=Cancel", COLOR_WARNING, 1);
            break;
        }

        case STATE_ADD_PSK_NAME: {
            Display::drawText(10, 10, "Add Custom Channel", COLOR_ACCENT, 2);
            Display::drawText(10, 50, "Enter channel name:", COLOR_TEXT, 1);

            // Input box
            Display::drawRect(10, 75, 300, 28, COLOR_TEXT);
            char displayBuf[34];
            snprintf(displayBuf, sizeof(displayBuf), "%s_", inputName);
            Display::drawText(15, 82, displayBuf, COLOR_ACCENT, 1);

            Display::drawText(10, 205, "ENTER=Next  ESC=Cancel", COLOR_WARNING, 1);
            break;
        }

        case STATE_ADD_PSK_KEY: {
            Display::drawText(10, 10, "Add Custom Channel", COLOR_ACCENT, 2);

            char nameBuf[40];
            snprintf(nameBuf, sizeof(nameBuf), "Name: %s", inputName);
            Display::drawText(10, 45, nameBuf, COLOR_TEXT, 1);

            Display::drawText(10, 70, "Enter PSK (base64):", COLOR_TEXT, 1);

            // Input box - wider for base64
            Display::drawRect(10, 95, 300, 28, COLOR_TEXT);
            char displayBuf[50];
            // Show last ~30 chars if longer
            int pskLen = strlen(inputPSK);
            if (pskLen > 28) {
                snprintf(displayBuf, sizeof(displayBuf), "...%s_", inputPSK + pskLen - 25);
            } else {
                snprintf(displayBuf, sizeof(displayBuf), "%s_", inputPSK);
            }
            Display::drawText(15, 102, displayBuf, COLOR_ACCENT, 1);

            Display::drawText(10, 140, "Example (Public):", COLOR_TEXT, 1);
            Display::drawText(10, 160, "izOH6cXN6mrJ5e26oRXNcg==", COLOR_TEXT, 1);

            Display::drawText(10, 205, "ENTER=Create  ESC=Cancel", COLOR_WARNING, 1);
            break;
        }

        case STATE_CONFIRM_DELETE: {
            Display::drawText(10, 10, "Delete Channel?", COLOR_ACCENT, 2);

            if (selectedIndex < settings.numChannels) {
                char buf[48];
                snprintf(buf, sizeof(buf), "Delete \"%s\"?", settings.channels[selectedIndex].name);
                Display::drawText(10, 60, buf, COLOR_TEXT, 1);
            }

            Display::drawText(10, 100, "This cannot be undone.", COLOR_WARNING, 1);

            Display::drawText(10, 205, "Y=Yes  N/ESC=Cancel", COLOR_WARNING, 1);
            break;
        }
    }
}

bool handleKey(uint8_t key) {
    ChannelSettings& settings = SettingsManager::getChannelSettings();
    char c = Keyboard::getChar(key);

    switch (currentState) {
        case STATE_LIST:
            if (key == KEY_ESC || key == KEY_BACKSPACE) {
                return true;  // Exit to main (Delete/Backspace acts as back)
            } else if (c == 'a' || c == 'A') {
                // Add channel
                currentState = STATE_ADD_TYPE;
                addTypeSelection = 0;
                inputName[0] = '\0';
                inputPSK[0] = '\0';
                inputPos = 0;
                show();
            } else if (c == 'd' || c == 'D') {
                // Delete channel (not Public)
                if (selectedIndex > 0 && selectedIndex < settings.numChannels) {
                    currentState = STATE_CONFIRM_DELETE;
                    show();
                }
            } else if (key == KEY_ENTER) {
                // View channel (legacy - no action now)
                show();
            }
            break;

        case STATE_ADD_TYPE:
            if (key == KEY_ESC) {
                currentState = STATE_LIST;
                show();
            } else if (key == KEY_ENTER) {
                if (addTypeSelection == 0) {
                    currentState = STATE_ADD_HASHTAG;
                } else {
                    currentState = STATE_ADD_PSK_NAME;
                }
                inputName[0] = '\0';
                inputPos = 0;
                show();
            }
            break;

        case STATE_ADD_HASHTAG:
            if (key == KEY_ESC) {
                currentState = STATE_LIST;
                show();
            } else if (key == KEY_ENTER) {
                // Create hashtag channel
                if (strlen(inputName) > 0) {
                    char fullName[32];
                    snprintf(fullName, sizeof(fullName), "#%s", inputName);
                    int idx = settings.addHashtagChannel(fullName);
                    if (idx >= 0) {
                        SettingsManager::save();
                        selectedIndex = idx;
                    }
                }
                currentState = STATE_LIST;
                show();
            } else if (c == '\b' && inputPos > 0) {
                inputName[--inputPos] = '\0';
                show();
            } else if (c >= 32 && c < 127 && inputPos < 28) {
                // Don't allow # in the middle
                if (c != '#') {
                    inputName[inputPos++] = c;
                    inputName[inputPos] = '\0';
                    show();
                }
            }
            break;

        case STATE_ADD_PSK_NAME:
            if (key == KEY_ESC) {
                currentState = STATE_LIST;
                show();
            } else if (key == KEY_ENTER) {
                if (strlen(inputName) > 0) {
                    currentState = STATE_ADD_PSK_KEY;
                    inputPSK[0] = '\0';
                    inputPos = 0;
                }
                show();
            } else if (c == '\b' && inputPos > 0) {
                inputName[--inputPos] = '\0';
                show();
            } else if (c >= 32 && c < 127 && inputPos < 28) {
                inputName[inputPos++] = c;
                inputName[inputPos] = '\0';
                show();
            }
            break;

        case STATE_ADD_PSK_KEY:
            if (key == KEY_ESC) {
                currentState = STATE_LIST;
                show();
            } else if (key == KEY_ENTER) {
                // Create custom PSK channel
                if (strlen(inputPSK) > 0) {
                    int idx = settings.addChannel(inputName, inputPSK);
                    if (idx >= 0) {
                        SettingsManager::save();
                        selectedIndex = idx;
                    }
                }
                currentState = STATE_LIST;
                show();
            } else if (c == '\b' && inputPos > 0) {
                inputPSK[--inputPos] = '\0';
                show();
            } else if (c >= 32 && c < 127 && inputPos < 60) {
                inputPSK[inputPos++] = c;
                inputPSK[inputPos] = '\0';
                show();
            }
            break;

        case STATE_CONFIRM_DELETE:
            if (key == KEY_ESC || key == KEY_BACKSPACE || c == 'n' || c == 'N') {
                currentState = STATE_LIST;
                show();
            } else if (c == 'y' || c == 'Y') {
                // Delete the channel
                if (settings.removeChannel(selectedIndex)) {
                    SettingsManager::save();
                    if (selectedIndex >= settings.numChannels) {
                        selectedIndex = settings.numChannels - 1;
                    }
                }
                currentState = STATE_LIST;
                show();
            }
            break;
    }

    return false;
}

bool handleTrackball(bool up, bool down, bool left, bool right, bool click) {
    ChannelSettings& settings = SettingsManager::getChannelSettings();
    bool needsRefresh = false;

    switch (currentState) {
        case STATE_LIST:
            if (up && selectedIndex > 0) {
                selectedIndex--;
                needsRefresh = true;
            }
            if (down && selectedIndex < settings.numChannels - 1) {
                selectedIndex++;
                needsRefresh = true;
            }
            if (click) {
                // View channel (legacy - no action now)
                needsRefresh = true;
            }
            break;

        case STATE_ADD_TYPE:
            if (up && addTypeSelection > 0) {
                addTypeSelection--;
                needsRefresh = true;
            }
            if (down && addTypeSelection < 1) {
                addTypeSelection++;
                needsRefresh = true;
            }
            break;

        default:
            break;
    }

    if (needsRefresh) {
        show();
    }

    return needsRefresh;
}

State getState() {
    return currentState;
}

int getSelectedIndex() {
    return selectedIndex;
}

} // namespace ChannelUI
