/**
 * MeshBerry Contact UI Implementation
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2026 NodakMesh (nodakmesh.org)
 *
 * This file is part of MeshBerry.
 */

#include "ContactUI.h"
#include "../drivers/display.h"
#include "../drivers/keyboard.h"
#include "../settings/SettingsManager.h"
#include "../mesh/MeshBerryMesh.h"
#include <string.h>

namespace ContactUI {

// UI state
static State currentState = STATE_LIST;
static int selectedIndex = 0;
static ContactFilter currentFilter = FILTER_ALL;
static int scrollOffset = 0;

// Filtered indices cache
static int filteredIndices[ContactSettings::MAX_CONTACTS];
static int filteredCount = 0;

// Admin session state
static MeshBerryMesh* theMesh = nullptr;
static char passwordBuffer[17] = "";
static int passwordPos = 0;
static int selectedRepeaterIdx = -1;
static char lastCLIResponse[64] = "";
static char selectedRepeaterName[32] = "";

// Forward declarations
static void updateFilteredList();
static const char* getFilterName(ContactFilter filter);
static const char* getTimeAgo(uint32_t timestamp);
static void showPasswordInputScreen();
static void showAdminSessionScreen();
static void attemptLogin();

void init() {
    currentState = STATE_LIST;
    selectedIndex = 0;
    currentFilter = FILTER_ALL;
    scrollOffset = 0;
    updateFilteredList();
}

char getTypeChar(NodeType type) {
    switch (type) {
        case NODE_TYPE_CHAT:     return 'C';
        case NODE_TYPE_REPEATER: return 'R';
        case NODE_TYPE_ROOM:     return 'S';
        case NODE_TYPE_SENSOR:   return 'X';
        default:                 return '?';
    }
}

static const char* getFilterName(ContactFilter filter) {
    switch (filter) {
        case FILTER_ALL:        return "All";
        case FILTER_REPEATERS:  return "Repeaters";
        case FILTER_CHAT_NODES: return "Nodes";
        case FILTER_FAVORITES:  return "Favorites";
        default:                return "?";
    }
}

static const char* getTimeAgo(uint32_t timestamp) {
    static char buf[16];
    uint32_t now = millis() / 1000;  // Simple approximation
    uint32_t diff = (now > timestamp) ? (now - timestamp) : 0;

    if (diff < 60) {
        snprintf(buf, sizeof(buf), "%ds", diff);
    } else if (diff < 3600) {
        snprintf(buf, sizeof(buf), "%dm", diff / 60);
    } else if (diff < 86400) {
        snprintf(buf, sizeof(buf), "%dh", diff / 3600);
    } else {
        snprintf(buf, sizeof(buf), "%dd", diff / 86400);
    }
    return buf;
}

static void updateFilteredList() {
    ContactSettings& settings = SettingsManager::getContactSettings();
    filteredCount = 0;

    for (int i = 0; i < settings.numContacts; i++) {
        const ContactEntry* c = settings.getContact(i);
        if (!c) continue;

        bool include = false;
        switch (currentFilter) {
            case FILTER_ALL:
                include = true;
                break;
            case FILTER_REPEATERS:
                include = (c->type == NODE_TYPE_REPEATER);
                break;
            case FILTER_CHAT_NODES:
                include = (c->type == NODE_TYPE_CHAT);
                break;
            case FILTER_FAVORITES:
                include = c->isFavorite;
                break;
        }

        if (include) {
            filteredIndices[filteredCount++] = i;
        }
    }

    // Clamp selection
    if (selectedIndex >= filteredCount) {
        selectedIndex = (filteredCount > 0) ? filteredCount - 1 : 0;
    }
}

void show() {
    Display::clear(COLOR_BACKGROUND);

    ContactSettings& settings = SettingsManager::getContactSettings();
    updateFilteredList();

    switch (currentState) {
        case STATE_LIST: {
            // Header with filter
            char header[32];
            snprintf(header, sizeof(header), "Contacts [%s]", getFilterName(currentFilter));
            Display::drawText(10, 10, header, COLOR_ACCENT, 2);

            // Count
            char countBuf[16];
            snprintf(countBuf, sizeof(countBuf), "%d", filteredCount);
            Display::drawText(280, 15, countBuf, COLOR_TEXT, 1);

            int y = 45;
            const int lineHeight = 22;
            const int maxVisible = 6;

            // Adjust scroll if needed
            if (selectedIndex < scrollOffset) {
                scrollOffset = selectedIndex;
            } else if (selectedIndex >= scrollOffset + maxVisible) {
                scrollOffset = selectedIndex - maxVisible + 1;
            }

            for (int i = 0; i < maxVisible && (scrollOffset + i) < filteredCount; i++) {
                int actualIdx = filteredIndices[scrollOffset + i];
                const ContactEntry* c = settings.getContact(actualIdx);
                if (!c) continue;

                char buf[48];
                bool isSelected = (scrollOffset + i == selectedIndex);

                // Format: [type] name    rssi
                // e.g. "> R Repeater-North  -85"
                const char* prefix = isSelected ? ">" : " ";
                const char* favStar = c->isFavorite ? "*" : " ";

                snprintf(buf, sizeof(buf), "%s%s%c %-16s %4d",
                         prefix, favStar, getTypeChar(c->type),
                         c->name, c->lastRssi);

                uint16_t color = isSelected ? COLOR_ACCENT : COLOR_TEXT;
                Display::drawText(10, y, buf, color, 1);
                y += lineHeight;
            }

            // Scroll indicators
            if (scrollOffset > 0) {
                Display::drawText(300, 45, "^", COLOR_WARNING, 1);
            }
            if (scrollOffset + maxVisible < filteredCount) {
                Display::drawText(300, 45 + (maxVisible - 1) * lineHeight, "v", COLOR_WARNING, 1);
            }

            // Instructions
            Display::drawText(10, 185, "UP/DN=Nav  L/R=Filter  F=Fav", COLOR_WARNING, 1);
            Display::drawText(10, 205, "ENTER=Details  D=Del  DEL=Back", COLOR_WARNING, 1);
            break;
        }

        case STATE_DETAIL: {
            if (filteredCount == 0 || selectedIndex >= filteredCount) {
                Display::drawText(10, 10, "No Contact", COLOR_ACCENT, 2);
                Display::drawText(10, 205, "ESC=Back", COLOR_WARNING, 1);
                break;
            }

            int actualIdx = filteredIndices[selectedIndex];
            const ContactEntry* c = settings.getContact(actualIdx);
            if (!c) {
                Display::drawText(10, 10, "Invalid Contact", COLOR_ACCENT, 2);
                Display::drawText(10, 205, "ESC=Back", COLOR_WARNING, 1);
                break;
            }

            // Title with favorite star
            char title[36];
            snprintf(title, sizeof(title), "%s%s", c->isFavorite ? "* " : "", c->name);
            Display::drawText(10, 10, title, COLOR_ACCENT, 2);

            int y = 50;
            char buf[48];

            // Type
            const char* typeName = "Unknown";
            switch (c->type) {
                case NODE_TYPE_CHAT:     typeName = "Chat Node"; break;
                case NODE_TYPE_REPEATER: typeName = "Repeater"; break;
                case NODE_TYPE_ROOM:     typeName = "Room Server"; break;
                case NODE_TYPE_SENSOR:   typeName = "Sensor"; break;
                default: break;
            }
            snprintf(buf, sizeof(buf), "Type: %s", typeName);
            Display::drawText(10, y, buf, COLOR_TEXT, 1);
            y += 22;

            // ID
            snprintf(buf, sizeof(buf), "ID: %08X", c->id);
            Display::drawText(10, y, buf, COLOR_TEXT, 1);
            y += 22;

            // Signal
            snprintf(buf, sizeof(buf), "RSSI: %d dBm", c->lastRssi);
            Display::drawText(10, y, buf, COLOR_TEXT, 1);
            y += 22;

            snprintf(buf, sizeof(buf), "SNR: %.1f dB", c->lastSnr);
            Display::drawText(10, y, buf, COLOR_TEXT, 1);
            y += 22;

            // Last heard
            snprintf(buf, sizeof(buf), "Last heard: %s ago", getTimeAgo(c->lastHeard));
            Display::drawText(10, y, buf, COLOR_TEXT, 1);

            // Show appropriate instructions based on type
            if (c->type == NODE_TYPE_REPEATER) {
                Display::drawText(10, 185, "A=Admin  F=Fav  D=Del", COLOR_WARNING, 1);
            } else {
                Display::drawText(10, 185, "F=Toggle Favorite", COLOR_WARNING, 1);
            }
            Display::drawText(10, 205, "DEL=Back", COLOR_WARNING, 1);
            break;
        }

        case STATE_PASSWORD_INPUT:
            showPasswordInputScreen();
            break;

        case STATE_ADMIN_SESSION:
            showAdminSessionScreen();
            break;

        case STATE_CONFIRM_DELETE: {
            Display::drawText(10, 10, "Delete Contact?", COLOR_ACCENT, 2);

            if (filteredCount > 0 && selectedIndex < filteredCount) {
                int actualIdx = filteredIndices[selectedIndex];
                const ContactEntry* c = settings.getContact(actualIdx);
                if (c) {
                    char buf[48];
                    snprintf(buf, sizeof(buf), "Delete \"%s\"?", c->name);
                    Display::drawText(10, 60, buf, COLOR_TEXT, 1);
                }
            }

            Display::drawText(10, 100, "This cannot be undone.", COLOR_WARNING, 1);
            Display::drawText(10, 205, "Y=Yes  N/DEL=Cancel", COLOR_WARNING, 1);
            break;
        }
    }
}

bool handleKey(uint8_t key) {
    ContactSettings& settings = SettingsManager::getContactSettings();
    char c = Keyboard::getChar(key);

    switch (currentState) {
        case STATE_LIST:
            if (key == KEY_ESC || key == KEY_BACKSPACE) {
                return true;  // Exit to main
            } else if (key == KEY_ENTER) {
                if (filteredCount > 0) {
                    currentState = STATE_DETAIL;
                    show();
                }
            } else if (c == 'f' || c == 'F') {
                // Toggle favorite
                if (filteredCount > 0 && selectedIndex < filteredCount) {
                    int actualIdx = filteredIndices[selectedIndex];
                    settings.toggleFavorite(actualIdx);
                    SettingsManager::saveContacts();
                    show();
                }
            } else if (c == 'd' || c == 'D') {
                // Delete contact
                if (filteredCount > 0 && selectedIndex < filteredCount) {
                    currentState = STATE_CONFIRM_DELETE;
                    show();
                }
            }
            break;

        case STATE_DETAIL:
            if (key == KEY_ESC || key == KEY_BACKSPACE) {
                currentState = STATE_LIST;
                show();
            } else if (c == 'f' || c == 'F') {
                // Toggle favorite
                if (filteredCount > 0 && selectedIndex < filteredCount) {
                    int actualIdx = filteredIndices[selectedIndex];
                    settings.toggleFavorite(actualIdx);
                    SettingsManager::saveContacts();
                    show();
                }
            } else if (c == 'd' || c == 'D') {
                currentState = STATE_CONFIRM_DELETE;
                show();
            } else if (c == 'a' || c == 'A') {
                // Admin login - only for repeaters
                if (filteredCount > 0 && selectedIndex < filteredCount) {
                    int actualIdx = filteredIndices[selectedIndex];
                    const ContactEntry* contact = settings.getContact(actualIdx);
                    if (contact && contact->type == NODE_TYPE_REPEATER) {
                        selectedRepeaterIdx = actualIdx;
                        strlcpy(selectedRepeaterName, contact->name, sizeof(selectedRepeaterName));

                        // Pre-fill password if saved
                        if (contact->hasSavedPassword()) {
                            strlcpy(passwordBuffer, contact->savedPassword, sizeof(passwordBuffer));
                            passwordPos = strlen(passwordBuffer);
                        } else {
                            passwordBuffer[0] = '\0';
                            passwordPos = 0;
                        }
                        lastCLIResponse[0] = '\0';
                        currentState = STATE_PASSWORD_INPUT;
                        show();
                    }
                }
            }
            break;

        case STATE_PASSWORD_INPUT:
            if (key == KEY_ESC || (key == KEY_BACKSPACE && passwordPos == 0)) {
                currentState = STATE_DETAIL;
                show();
            } else if (key == KEY_ENTER) {
                attemptLogin();
            } else if (key == KEY_BACKSPACE && passwordPos > 0) {
                passwordBuffer[--passwordPos] = '\0';
                show();
            } else if (c >= 32 && c < 127 && passwordPos < 16) {
                passwordBuffer[passwordPos++] = c;
                passwordBuffer[passwordPos] = '\0';
                show();
            }
            break;

        case STATE_ADMIN_SESSION:
            if (key == KEY_ESC || key == KEY_BACKSPACE) {
                currentState = STATE_DETAIL;
                show();
            } else if (c == 'l' || c == 'L') {
                // Logout
                if (theMesh) {
                    theMesh->disconnectRepeater();
                }
                lastCLIResponse[0] = '\0';
                currentState = STATE_DETAIL;
                show();
            } else if (c == '1' && theMesh) {
                theMesh->sendRepeaterCommand("advert");
                strlcpy(lastCLIResponse, "Sent: advert", sizeof(lastCLIResponse));
                show();
            } else if (c == '2' && theMesh) {
                theMesh->sendRepeaterCommand("status");
                strlcpy(lastCLIResponse, "Sent: status", sizeof(lastCLIResponse));
                show();
            } else if (c == '3' && theMesh) {
                theMesh->sendRepeaterCommand("get name");
                strlcpy(lastCLIResponse, "Sent: get name", sizeof(lastCLIResponse));
                show();
            }
            break;

        case STATE_CONFIRM_DELETE:
            if (key == KEY_ESC || key == KEY_BACKSPACE || c == 'n' || c == 'N') {
                currentState = STATE_LIST;
                show();
            } else if (c == 'y' || c == 'Y') {
                // Delete the contact
                if (filteredCount > 0 && selectedIndex < filteredCount) {
                    int actualIdx = filteredIndices[selectedIndex];
                    settings.removeContact(actualIdx);
                    SettingsManager::saveContacts();
                    updateFilteredList();
                }
                currentState = STATE_LIST;
                show();
            }
            break;
    }

    return false;
}

bool handleTrackball(bool up, bool down, bool left, bool right, bool click) {
    bool needsRefresh = false;

    switch (currentState) {
        case STATE_LIST:
            if (up && selectedIndex > 0) {
                selectedIndex--;
                needsRefresh = true;
            }
            if (down && selectedIndex < filteredCount - 1) {
                selectedIndex++;
                needsRefresh = true;
            }
            if (left) {
                // Previous filter
                if (currentFilter == FILTER_ALL) {
                    currentFilter = FILTER_FAVORITES;
                } else {
                    currentFilter = (ContactFilter)(currentFilter - 1);
                }
                updateFilteredList();
                needsRefresh = true;
            }
            if (right) {
                // Next filter
                if (currentFilter == FILTER_FAVORITES) {
                    currentFilter = FILTER_ALL;
                } else {
                    currentFilter = (ContactFilter)(currentFilter + 1);
                }
                updateFilteredList();
                needsRefresh = true;
            }
            if (click) {
                if (filteredCount > 0) {
                    currentState = STATE_DETAIL;
                    needsRefresh = true;
                }
            }
            break;

        case STATE_DETAIL:
            if (click || left) {
                currentState = STATE_LIST;
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

ContactFilter getFilter() {
    return currentFilter;
}

void setMesh(MeshBerryMesh* mesh) {
    theMesh = mesh;
}

// =============================================================================
// PASSWORD INPUT SCREEN
// =============================================================================

static void showPasswordInputScreen() {
    Display::clear(COLOR_BACKGROUND);
    Display::drawText(10, 10, "Admin Login", COLOR_ACCENT, 2);

    // Show repeater name
    char buf[48];
    snprintf(buf, sizeof(buf), "Repeater: %s", selectedRepeaterName);
    Display::drawText(10, 50, buf, COLOR_TEXT, 1);

    // Password input box
    Display::drawRect(10, 80, 200, 25, COLOR_TEXT);

    // Show asterisks for password
    char masked[17];
    for (int i = 0; i < passwordPos && i < 16; i++) {
        masked[i] = '*';
    }
    masked[passwordPos] = '\0';
    Display::drawText(15, 86, masked, COLOR_ACCENT, 1);

    // Show cursor
    int cursorX = 15 + passwordPos * 6;  // Approximate character width
    Display::fillRect(cursorX, 86, 2, 12, COLOR_ACCENT);

    // Status message
    if (lastCLIResponse[0]) {
        Display::drawText(10, 115, lastCLIResponse, COLOR_WARNING, 1);
    }

    // Instructions
    Display::drawText(10, 185, "Type password, ENTER=Login", COLOR_WARNING, 1);
    Display::drawText(10, 205, "DEL=Cancel", COLOR_WARNING, 1);
}

// =============================================================================
// ADMIN SESSION SCREEN
// =============================================================================

static void showAdminSessionScreen() {
    Display::clear(COLOR_BACKGROUND);
    Display::drawText(10, 10, "Repeater Admin", COLOR_ACCENT, 2);

    // Show connected repeater
    char buf[48];
    if (theMesh && theMesh->isRepeaterConnected()) {
        snprintf(buf, sizeof(buf), "Connected: %s", theMesh->getConnectedRepeaterName());
        Display::drawText(10, 45, buf, COLOR_ACCENT, 1);

        // Show permissions
        uint8_t perms = theMesh->getRepeaterPermissions();
        snprintf(buf, sizeof(buf), "Perms: %s", perms >= 3 ? "Admin" : "Guest");
        Display::drawText(10, 65, buf, COLOR_TEXT, 1);
    } else {
        Display::drawText(10, 45, "Disconnected", COLOR_WARNING, 1);
    }

    // Show last response/status
    if (lastCLIResponse[0]) {
        Display::drawText(10, 90, lastCLIResponse, COLOR_TEXT, 1);
    }

    // Commands
    int y = 115;
    Display::drawText(10, y, "1 = Send Advert", COLOR_ACCENT, 1);
    Display::drawText(10, y + 18, "2 = Get Status", COLOR_ACCENT, 1);
    Display::drawText(10, y + 36, "3 = Get Name", COLOR_ACCENT, 1);

    // Instructions
    Display::drawText(10, 185, "L=Logout", COLOR_WARNING, 1);
    Display::drawText(10, 205, "DEL=Back to Contact", COLOR_WARNING, 1);
}

// =============================================================================
// LOGIN ATTEMPT
// =============================================================================

static void attemptLogin() {
    if (!theMesh) {
        strlcpy(lastCLIResponse, "Error: No mesh", sizeof(lastCLIResponse));
        show();
        return;
    }

    ContactSettings& settings = SettingsManager::getContactSettings();
    ContactEntry* contact = settings.getContact(selectedRepeaterIdx);

    if (!contact) {
        strlcpy(lastCLIResponse, "Error: Invalid contact", sizeof(lastCLIResponse));
        show();
        return;
    }

    // Debug: log contact info at login time
    Serial.printf("[UI] Login attempt for contact idx=%d name='%s'\n", selectedRepeaterIdx, contact->name);
    Serial.print("[UI] Contact pubKey at login: ");
    for (int i = 0; i < 32; i++) Serial.printf("%02X", contact->pubKey[i]);
    Serial.println();

    // Check if we have the public key
    bool hasKey = false;
    for (int i = 0; i < 32; i++) {
        if (contact->pubKey[i] != 0) {
            hasKey = true;
            break;
        }
    }

    if (!hasKey) {
        strlcpy(lastCLIResponse, "No pubkey - wait for advert", sizeof(lastCLIResponse));
        show();
        return;
    }

    // Send login request
    if (theMesh->sendRepeaterLogin(contact->id, contact->pubKey, passwordBuffer)) {
        strlcpy(lastCLIResponse, "Logging in...", sizeof(lastCLIResponse));
        show();
    } else {
        strlcpy(lastCLIResponse, "Failed to send login", sizeof(lastCLIResponse));
        show();
    }
}

// =============================================================================
// CALLBACKS FROM MESH
// =============================================================================

void onLoginSuccess() {
    strlcpy(lastCLIResponse, "Login successful!", sizeof(lastCLIResponse));
    currentState = STATE_ADMIN_SESSION;
    show();
}

void onLoginFailed() {
    strlcpy(lastCLIResponse, "Login failed - bad password?", sizeof(lastCLIResponse));
    // Stay on password input screen
    show();
}

void onCLIResponse(const char* response) {
    if (response) {
        strlcpy(lastCLIResponse, response, sizeof(lastCLIResponse));
    }
    show();
}

} // namespace ContactUI
