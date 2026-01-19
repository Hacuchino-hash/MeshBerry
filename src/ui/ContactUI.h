/**
 * MeshBerry Contact UI
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2026 NodakMesh (nodakmesh.org)
 *
 * This file is part of MeshBerry.
 *
 * Contact management UI screen.
 * Displays discovered nodes with type filtering (repeaters vs chat nodes).
 */

#ifndef MESHBERRY_CONTACT_UI_H
#define MESHBERRY_CONTACT_UI_H

#include <Arduino.h>
#include "../settings/ContactSettings.h"

namespace ContactUI {

/**
 * UI State
 */
enum State {
    STATE_LIST,           // Viewing contact list
    STATE_DETAIL,         // Viewing contact details
    STATE_CONFIRM_DELETE, // Confirming contact deletion
    STATE_PASSWORD_INPUT, // Entering password for admin login
    STATE_ADMIN_SESSION   // Active admin session with repeater
};

/**
 * Initialize the contact UI
 */
void init();

/**
 * Show the contact screen
 */
void show();

/**
 * Handle a key press
 * @param key Key code from keyboard
 * @return true if screen should exit back to main
 */
bool handleKey(uint8_t key);

/**
 * Handle trackball movement
 * @param up Up pressed
 * @param down Down pressed
 * @param left Left pressed
 * @param right Right pressed
 * @param click Click pressed
 * @return true if UI needs refresh
 */
bool handleTrackball(bool up, bool down, bool left, bool right, bool click);

/**
 * Get current state
 */
State getState();

/**
 * Get selected contact index
 */
int getSelectedIndex();

/**
 * Get current filter mode
 */
ContactFilter getFilter();

/**
 * Get type character for display (C=Chat, R=Repeater, S=Room, X=Sensor)
 */
char getTypeChar(NodeType type);

/**
 * Set the mesh instance for admin operations
 */
void setMesh(class MeshBerryMesh* mesh);

/**
 * Callback when login succeeds
 */
void onLoginSuccess();

/**
 * Callback when login fails
 */
void onLoginFailed();

/**
 * Callback when CLI response is received
 */
void onCLIResponse(const char* response);

} // namespace ContactUI

#endif // MESHBERRY_CONTACT_UI_H
