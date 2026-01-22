/**
 * MeshBerry Messages Screen
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2026 NodakMesh (nodakmesh.org)
 *
 * Conversation list showing channels as group chats
 */

#ifndef MESHBERRY_MESSAGESSCREEN_H
#define MESHBERRY_MESSAGESSCREEN_H

#include "Screen.h"
#include "ScreenManager.h"
#include "ListView.h"

/**
 * Messages Screen - Channel conversation list
 *
 * Shows each channel as a conversation item with last message preview.
 * Selecting a channel opens the ChatScreen for that channel.
 * Last item is "Manage Channels" to open ChannelsScreen.
 */
class MessagesScreen : public Screen {
public:
    MessagesScreen();
    ~MessagesScreen() override = default;

    // Screen interface
    ScreenId getId() const override { return ScreenId::MESSAGES; }
    void onEnter() override;
    void onExit() override {}
    void draw(bool fullRedraw) override;
    bool handleInput(const InputData& input) override;
    const char* getTitle() const override { return "Messages"; }
    void configureSoftKeys() override;

    /**
     * Update a channel's conversation with new message
     * @param channelIdx Channel index (0-7)
     * @param senderAndText Message in "Sender: text" format
     * @param timestamp Message timestamp
     * @param hops Hop count (0 = direct/unknown)
     */
    static void onChannelMessage(int channelIdx, const char* senderAndText, uint32_t timestamp, uint8_t hops = 0);

    /**
     * Get total unread count across all channels
     */
    static int getUnreadCount();

private:
    // Build conversation list from channel settings
    void buildConversationList();

    // Handle conversation selection
    void onConversationSelected(int index);

    // Draw a single conversation item with modern card styling
    void drawConversationItem(int idx, int visibleIdx, bool selected);

    // List view
    ListView _listView;

    // Conversation data (one per channel + "Manage Channels" item)
    static const int MAX_CONVERSATIONS = 9;  // 8 channels + 1 management item
    struct Conversation {
        char name[32];          // Channel name
        char lastMessage[64];   // Last message preview
        uint32_t lastTimestamp; // Time of last message
        int channelIdx;         // -1 for management item, 0-7 for channel
        int unreadCount;        // Unread messages in this channel
    };
    static Conversation _conversations[MAX_CONVERSATIONS];
    static int _conversationCount;

    // List items for display
    ListItem _listItems[MAX_CONVERSATIONS];
    char _primaryStrings[MAX_CONVERSATIONS][32];
    char _secondaryStrings[MAX_CONVERSATIONS][72];  // More space for message preview

    // Format time ago string
    void formatTimeAgo(uint32_t timestamp, char* buf, size_t bufSize);
};

#endif // MESHBERRY_MESSAGESSCREEN_H
