/**
 * MeshBerry Messaging UI Implementation
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2026 NodakMesh (nodakmesh.org)
 *
 * This file is part of MeshBerry.
 */

#include "MessagingUI.h"
#include "../drivers/display.h"
#include "../drivers/keyboard.h"
#include "../settings/SettingsManager.h"
#include <string.h>

namespace MessagingUI {

// Message buffer (circular)
static ChatMessage messageBuffer[MSG_UI_MAX_MESSAGES];
static int messageHead = 0;  // Next write position
static int messageCount = 0; // Total messages stored

// Input buffer
static char inputBuffer[128] = "";
static int inputPos = 0;

// Scroll offset for viewing messages
static int scrollOffset = 0;

// Send callback
static SendMessageCallback sendCallback = nullptr;

// Helper to add a message to the circular buffer
static void addMessage(const ChatMessage& msg) {
    messageBuffer[messageHead] = msg;
    messageHead = (messageHead + 1) % MSG_UI_MAX_MESSAGES;
    if (messageCount < MSG_UI_MAX_MESSAGES) {
        messageCount++;
    }
}

// Get message at logical index (0 = oldest)
static ChatMessage* getMessageAt(int idx) {
    if (idx < 0 || idx >= messageCount) return nullptr;

    // Calculate actual index in circular buffer
    int start = (messageHead - messageCount + MSG_UI_MAX_MESSAGES) % MSG_UI_MAX_MESSAGES;
    int actualIdx = (start + idx) % MSG_UI_MAX_MESSAGES;
    return &messageBuffer[actualIdx];
}

// Count messages for channel 0 (Public) - legacy function
static int countChannelMessages() {
    int count = 0;
    for (int i = 0; i < messageCount; i++) {
        ChatMessage* msg = getMessageAt(i);
        if (msg && msg->channelIdx == 0) {
            count++;
        }
    }
    return count;
}

void init() {
    inputBuffer[0] = '\0';
    inputPos = 0;
    scrollOffset = 0;
}

void show() {
    Display::clear(COLOR_BACKGROUND);

    ChannelSettings& chSettings = SettingsManager::getChannelSettings();

    // Header - legacy UI, show "Messages - Public"
    Display::drawText(10, 10, "Messages - Public", COLOR_ACCENT, 2);

    // Draw messages area (y: 40 to 170)
    const int msgAreaTop = 40;
    const int msgAreaHeight = 130;
    const int lineHeight = 16;
    const int maxVisibleLines = msgAreaHeight / lineHeight;

    // Collect messages for channel 0 (Public) - legacy
    int channelMsgCount = 0;
    int channelMsgIndices[MSG_UI_MAX_MESSAGES];

    for (int i = 0; i < messageCount; i++) {
        ChatMessage* msg = getMessageAt(i);
        if (msg && msg->channelIdx == 0) {
            channelMsgIndices[channelMsgCount++] = i;
        }
    }

    // Calculate scroll bounds
    int maxScroll = (channelMsgCount > maxVisibleLines) ? (channelMsgCount - maxVisibleLines) : 0;
    if (scrollOffset > maxScroll) scrollOffset = maxScroll;
    if (scrollOffset < 0) scrollOffset = 0;

    // Draw visible messages
    int y = msgAreaTop;
    for (int i = 0; i < maxVisibleLines && (scrollOffset + i) < channelMsgCount; i++) {
        int msgIdx = channelMsgIndices[scrollOffset + i];
        ChatMessage* msg = getMessageAt(msgIdx);
        if (!msg) continue;

        // Format: "Sender: text" or "You: text"
        char line[160];
        const char* senderName = msg->isOutgoing ? "You" : msg->sender;
        snprintf(line, sizeof(line), "%s: %s", senderName, msg->text);

        // Truncate if too long for display
        if (strlen(line) > 40) {
            line[37] = '.';
            line[38] = '.';
            line[39] = '.';
            line[40] = '\0';
        }

        uint16_t color = msg->isOutgoing ? COLOR_ACCENT : COLOR_TEXT;
        Display::drawText(10, y, line, color, 1);
        y += lineHeight;
    }

    // Draw scroll indicator if needed
    if (channelMsgCount > maxVisibleLines) {
        char scrollInfo[16];
        snprintf(scrollInfo, sizeof(scrollInfo), "[%d/%d]", scrollOffset + 1, maxScroll + 1);
        Display::drawText(270, msgAreaTop, scrollInfo, COLOR_WARNING, 1);
    }

    // Draw input separator line (use thin rect since no drawLine)
    Display::fillRect(10, 175, 300, 1, COLOR_TEXT);

    // Draw input box
    Display::drawText(10, 180, ">", COLOR_ACCENT, 1);

    // Show input with cursor
    char displayBuf[50];
    int inputLen = strlen(inputBuffer);
    if (inputLen > 35) {
        // Show last 35 chars
        snprintf(displayBuf, sizeof(displayBuf), "...%s_", inputBuffer + inputLen - 32);
    } else {
        snprintf(displayBuf, sizeof(displayBuf), "%s_", inputBuffer);
    }
    Display::drawText(25, 180, displayBuf, COLOR_TEXT, 1);

    // Instructions
    Display::drawText(10, 205, "ENTER=Send  ESC/DEL=Back  UP/DN=Scroll", COLOR_WARNING, 1);
}

bool handleKey(uint8_t key) {
    char c = Keyboard::getChar(key);

    if (key == KEY_ESC || (key == KEY_BACKSPACE && inputPos == 0)) {
        // Exit if ESC or backspace with empty input
        return true;
    } else if (key == KEY_ENTER) {
        // Send message
        if (inputPos > 0 && sendCallback) {
            sendCallback(inputBuffer);

            // Add to local display (legacy - always channel 0)
            addOutgoingMessage(inputBuffer, millis(), 0);

            // Clear input
            inputBuffer[0] = '\0';
            inputPos = 0;

            // Scroll to bottom
            scrollOffset = 0;
            int channelMsgCount = countChannelMessages();
            int maxVisibleLines = 130 / 16;
            if (channelMsgCount > maxVisibleLines) {
                scrollOffset = channelMsgCount - maxVisibleLines;
            }

            show();
        }
    } else if (c == '\b' && inputPos > 0) {
        // Backspace - delete character
        inputBuffer[--inputPos] = '\0';
        show();
    } else if (c >= 32 && c < 127 && inputPos < 126) {
        // Regular character
        inputBuffer[inputPos++] = c;
        inputBuffer[inputPos] = '\0';
        show();
    }

    return false;
}

bool handleTrackball(bool up, bool down, bool left, bool right, bool click) {
    bool needsRefresh = false;

    if (up) {
        // Scroll up (show older messages)
        if (scrollOffset > 0) {
            scrollOffset--;
            needsRefresh = true;
        }
    }
    if (down) {
        // Scroll down (show newer messages)
        int channelMsgCount = countChannelMessages();
        int maxVisibleLines = 130 / 16;
        int maxScroll = (channelMsgCount > maxVisibleLines) ? (channelMsgCount - maxVisibleLines) : 0;
        if (scrollOffset < maxScroll) {
            scrollOffset++;
            needsRefresh = true;
        }
    }

    if (needsRefresh) {
        show();
    }

    return needsRefresh;
}

void addIncomingMessage(const char* sender, const char* text, uint32_t timestamp, uint8_t channelIdx) {
    ChatMessage msg;
    strlcpy(msg.sender, sender, sizeof(msg.sender));
    strlcpy(msg.text, text, sizeof(msg.text));
    msg.timestamp = timestamp;
    msg.channelIdx = channelIdx;
    msg.isOutgoing = false;

    addMessage(msg);
}

void addOutgoingMessage(const char* text, uint32_t timestamp, uint8_t channelIdx) {
    ChatMessage msg;
    strlcpy(msg.sender, "You", sizeof(msg.sender));
    strlcpy(msg.text, text, sizeof(msg.text));
    msg.timestamp = timestamp;
    msg.channelIdx = channelIdx;
    msg.isOutgoing = true;

    addMessage(msg);
}

int getMessageCount() {
    return countChannelMessages();
}

void setSendCallback(SendMessageCallback callback) {
    sendCallback = callback;
}

} // namespace MessagingUI
