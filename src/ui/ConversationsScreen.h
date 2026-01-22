/**
 * MeshBerry Conversations Screen
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2026 NodakMesh (nodakmesh.org)
 *
 * Unified conversation list (DMs + Channels merged)
 * Replaces separate MessagesScreen (channels only) navigation
 */

#ifndef MESHBERRY_CONVERSATIONS_SCREEN_H
#define MESHBERRY_CONVERSATIONS_SCREEN_H

#include "Screen.h"
#include "ListView.h"

/**
 * Type of conversation
 */
enum ConversationType {
    TYPE_CHANNEL = 0,
    TYPE_DM = 1
};

/**
 * Unified conversation entry (DM or Channel)
 */
struct UnifiedConversation {
    ConversationType type;

    // For channels
    int channelIdx;
    char channelName[32];

    // For DMs
    uint32_t contactId;
    char contactName[32];

    // Common fields
    char lastMessagePreview[64];
    uint32_t lastMessageTime;
    uint16_t unreadCount;
    bool isPinned;
    bool isMuted;

    void clear() {
        type = TYPE_CHANNEL;
        channelIdx = -1;
        contactId = 0;
        channelName[0] = '\0';
        contactName[0] = '\0';
        lastMessagePreview[0] = '\0';
        lastMessageTime = 0;
        unreadCount = 0;
        isPinned = false;
        isMuted = false;
    }
};

/**
 * Conversations Screen - Unified list of DMs and Channels
 */
class ConversationsScreen : public Screen {
private:
    static constexpr int MAX_CONVERSATIONS = 32;

    UnifiedConversation _conversations[MAX_CONVERSATIONS];
    int _count;
    int _selectedIdx;
    ListView _listView;

    // Build unified list from channels + DMs
    void buildConversationList();

    // Sort by last message time (most recent first)
    void sortByLastMessage();

    // Handle conversation selection
    void openConversation(int idx);

    // Draw a single conversation item
    void drawConversationItem(int idx, int visibleIdx, bool selected);

public:
    ConversationsScreen();

    void onEnter() override;
    void onExit() override;
    void configureSoftKeys() override;
    void draw(bool fullRedraw) override;
    bool handleInput(const InputData& input) override;

    /**
     * Called when a new channel message arrives
     * Updates the conversation list and unread counts
     */
    static void onChannelMessage(int channelIdx, const char* senderAndText,
                                 uint32_t timestamp, uint8_t hops);

    /**
     * Called when a new DM arrives
     * Updates the conversation list and unread counts
     */
    static void onDMReceived(uint32_t senderId, const char* senderName,
                            const char* text, uint32_t timestamp);

    /**
     * Get singleton instance (for static callbacks)
     */
    static ConversationsScreen* getInstance();

private:
    static ConversationsScreen* _instance;
};

#endif // MESHBERRY_CONVERSATIONS_SCREEN_H
