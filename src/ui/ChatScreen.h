/**
 * MeshBerry Chat Screen
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2026 NodakMesh (nodakmesh.org)
 *
 * Channel conversation view - shows message history and allows composing
 */

#ifndef MESHBERRY_CHATSCREEN_H
#define MESHBERRY_CHATSCREEN_H

#include "Screen.h"
#include "ScreenManager.h"

class ChatScreen : public Screen {
public:
    ChatScreen() = default;
    ~ChatScreen() override = default;

    ScreenId getId() const override { return ScreenId::CHAT; }
    void onEnter() override;
    void onExit() override;
    void draw(bool fullRedraw) override;
    bool handleInput(const InputData& input) override;
    const char* getTitle() const override;
    void configureSoftKeys() override;

    /**
     * Set the channel to display
     * Called before navigating to this screen
     */
    void setChannel(int channelIdx);

    /**
     * Get current channel index
     */
    int getChannelIndex() const { return _channelIdx; }

    /**
     * Add a message to this chat (for live updates)
     * @param sender Sender name
     * @param text Message text
     * @param timestamp Unix timestamp
     * @param isOutgoing True if sent by us
     * @param hops Hop count for incoming messages (0 = unknown)
     */
    void addMessage(const char* sender, const char* text, uint32_t timestamp, bool isOutgoing, uint8_t hops = 0);

    /**
     * Static helper to add message to the currently viewed chat
     * Safe to call even if not on ChatScreen
     * @param channelIdx Channel index
     * @param senderAndText Combined "Sender: text" string
     * @param timestamp Message timestamp
     * @param hops Hop count (0 = unknown)
     */
    static void addToCurrentChat(int channelIdx, const char* senderAndText, uint32_t timestamp, uint8_t hops = 0);

    /**
     * Update repeat count for a message (called when our message is heard repeated)
     * @param channelIdx Channel index
     * @param contentHash Hash of the message content
     * @param repeatCount Number of times heard repeated
     */
    static void updateRepeatCount(int channelIdx, uint32_t contentHash, uint8_t repeatCount);

    /**
     * Parse "SenderName: message" format (public for callback use)
     */
    static void parseSenderAndText(const char* combined, char* sender, size_t senderLen,
                                   char* text, size_t textLen);

private:
    int _channelIdx = 0;
    char _channelName[32];

    // Message history for this channel
    static const int MAX_CHAT_MESSAGES = 32;
    struct ChatMessage {
        char sender[16];
        char text[128];
        uint32_t timestamp;
        uint32_t contentHash;    // For matching repeat callbacks (outgoing only)
        uint8_t repeatCount;     // Times heard repeated (outgoing only)
        uint8_t hops;            // Hop count (incoming only, 0 = unknown)
        bool isOutgoing;
    };
    ChatMessage _messages[MAX_CHAT_MESSAGES];
    int _messageCount = 0;
    int _scrollOffset = 0;  // For scrolling through messages

    // Text input buffer
    char _inputBuffer[128];
    int _inputPos = 0;
    bool _inputMode = false;

    // Drawing helpers
    void drawMessages(bool fullRedraw);
    void drawInputBar();
    int getVisibleMessageCount() const;

    // Message handling
    void sendMessage();
    void clearInput();

    // Compute content hash for repeat tracking
    static uint32_t hashMessage(int channelIdx, const char* text);

    // Singleton instance pointer for static callback
    static ChatScreen* _instance;
};

#endif // MESHBERRY_CHATSCREEN_H
