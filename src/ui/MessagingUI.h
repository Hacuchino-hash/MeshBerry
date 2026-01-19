/**
 * MeshBerry Messaging UI
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2026 NodakMesh (nodakmesh.org)
 *
 * This file is part of MeshBerry.
 *
 * Messaging screen for viewing and sending channel messages.
 */

#ifndef MESHBERRY_MESSAGING_UI_H
#define MESHBERRY_MESSAGING_UI_H

#include <Arduino.h>

// Maximum messages to store in UI buffer
#define MSG_UI_MAX_MESSAGES 20

// Chat message structure
struct ChatMessage {
    char sender[16];      // Sender name
    char text[128];       // Message text
    uint32_t timestamp;   // Millis timestamp
    uint8_t channelIdx;   // Which channel this message belongs to
    bool isOutgoing;      // True if sent by us
};

namespace MessagingUI {

/**
 * Initialize the messaging UI
 */
void init();

/**
 * Show the messaging screen
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
 * Add an incoming message to the buffer
 * @param sender Sender name
 * @param text Message text
 * @param timestamp Timestamp (millis)
 * @param channelIdx Channel index
 */
void addIncomingMessage(const char* sender, const char* text, uint32_t timestamp, uint8_t channelIdx);

/**
 * Add an outgoing message to the buffer (sent by us)
 * @param text Message text
 * @param timestamp Timestamp (millis)
 * @param channelIdx Channel index
 */
void addOutgoingMessage(const char* text, uint32_t timestamp, uint8_t channelIdx);

/**
 * Get count of messages for current channel
 */
int getMessageCount();

/**
 * Callback type for sending messages
 */
typedef void (*SendMessageCallback)(const char* text);

/**
 * Set the callback for sending messages
 */
void setSendCallback(SendMessageCallback callback);

} // namespace MessagingUI

#endif // MESHBERRY_MESSAGING_UI_H
