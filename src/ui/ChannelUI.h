/**
 * MeshBerry Channel UI
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2026 NodakMesh (nodakmesh.org)
 *
 * This file is part of MeshBerry.
 *
 * Channel management UI screen.
 */

#ifndef MESHBERRY_CHANNEL_UI_H
#define MESHBERRY_CHANNEL_UI_H

#include <Arduino.h>
#include "../settings/ChannelSettings.h"

namespace ChannelUI {

/**
 * UI State
 */
enum State {
    STATE_LIST,          // Viewing channel list
    STATE_ADD_TYPE,      // Choosing add type (#hashtag or custom PSK)
    STATE_ADD_HASHTAG,   // Entering hashtag channel name
    STATE_ADD_PSK_NAME,  // Entering custom channel name
    STATE_ADD_PSK_KEY,   // Entering custom PSK
    STATE_CONFIRM_DELETE // Confirming channel deletion
};

/**
 * Initialize the channel UI
 */
void init();

/**
 * Show the channel screen
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
 * Get selected channel index
 */
int getSelectedIndex();

} // namespace ChannelUI

#endif // MESHBERRY_CHANNEL_UI_H
