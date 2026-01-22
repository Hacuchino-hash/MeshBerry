/**
 * MeshBerry Message Thread Screen
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2026 NodakMesh (nodakmesh.org)
 *
 * Unified message view for both DMs and Channels
 * Replaces separate ChatScreen and DMChatScreen
 */

#ifndef MESHBERRY_MESSAGETHREAD_SCREEN_H
#define MESHBERRY_MESSAGETHREAD_SCREEN_H

#include "Screen.h"
#include "ConversationsScreen.h"
#include "../settings/MessageArchive.h"
#include <vector>

/**
 * Cached message with pre-wrapped text for efficient rendering
 */
struct CachedMessage {
    char sender[16];
    char text[128];
    uint32_t timestamp;
    bool isOutgoing;
    uint8_t deliveryStatus;  // For DMs: SENDING, SENT, DELIVERED, FAILED
    uint8_t hops;

    // Cached rendering data (Meshtastic-style optimization)
    std::vector<String> wrappedLines;
    int totalHeight;

    void clear() {
        sender[0] = '\0';
        text[0] = '\0';
        timestamp = 0;
        isOutgoing = false;
        deliveryStatus = 0;
        hops = 0;
        wrappedLines.clear();
        totalHeight = 0;
    }
};

/**
 * Message Thread Screen - Unified chat for DMs and Channels
 */
class MessageThreadScreen : public Screen {
private:
    // Conversation context
    ConversationType _type;
    int _channelIdx;
    uint32_t _contactId;
    char _title[32];

    // Message cache (lazy loaded from archive)
    static constexpr int CACHE_SIZE = 100;
    std::vector<CachedMessage> _messageCache;
    int _totalMessages;
    int _cacheStartIdx;
    bool _cacheValid;

    // Rendering cache (pre-wrapped lines)
    bool _renderCacheValid;
    int _contentHeight;

    // Scroll state
    int _scrollY;
    bool _autoScroll;

    // Input state (always-visible input bar)
    char _inputBuffer[128];
    int _inputPos;
    bool _cursorVisible;
    uint32_t _lastCursorBlink;

    // Message loading
    void loadMessages(int startIdx, int count);
    void rebuildRenderCache();
    void wrapMessageText(CachedMessage& msg);

    // Rendering
    void drawMessages();
    void drawInputBar();

    // Input handling
    void sendMessage();
    void handleCharInput(char c);
    void handleBackspace();

public:
    MessageThreadScreen();

    /**
     * Set conversation context (call before navigating to this screen)
     */
    void setConversation(ConversationType type, int channelIdx, uint32_t contactId, const char* title);

    void onEnter() override;
    void onExit() override;
    void configureSoftKeys() override;
    void draw(bool fullRedraw) override;
    bool handleInput(const InputData& input) override;
    void update(uint32_t deltaMs) override;

    /**
     * Append new message to thread (if viewing this conversation)
     */
    void appendMessage(const char* sender, const char* text, uint32_t timestamp, bool isOutgoing, uint8_t hops);

    /**
     * Check if this thread is currently viewing a specific conversation
     */
    bool isViewingConversation(ConversationType type, int channelIdx, uint32_t contactId);

    /**
     * Get singleton instance
     */
    static MessageThreadScreen* getInstance();

private:
    static MessageThreadScreen* _instance;
};

#endif // MESHBERRY_MESSAGETHREAD_SCREEN_H
