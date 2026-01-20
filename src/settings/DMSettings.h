/**
 * MeshBerry DM Settings
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2026 NodakMesh (nodakmesh.org)
 *
 * This file is part of MeshBerry.
 *
 * Persistent storage for direct message conversations.
 */

#ifndef MESHBERRY_DM_SETTINGS_H
#define MESHBERRY_DM_SETTINGS_H

#include <Arduino.h>
#include "../config.h"

// =============================================================================
// DM MESSAGE
// =============================================================================

struct DMMessage {
    uint32_t timestamp;
    char text[MAX_MESSAGE_LENGTH];
    bool isOutgoing;
    bool delivered;

    void clear() {
        timestamp = 0;
        text[0] = '\0';
        isOutgoing = false;
        delivered = false;
    }
};

// =============================================================================
// DM CONVERSATION
// =============================================================================

struct DMConversation {
    static constexpr int MAX_MESSAGES_PER_CONV = 32;

    uint32_t contactId;
    DMMessage messages[MAX_MESSAGES_PER_CONV];
    int messageCount;
    int messageHead;        // Circular buffer head
    uint32_t lastTimestamp;
    int unreadCount;
    bool isActive;

    void clear() {
        contactId = 0;
        messageCount = 0;
        messageHead = 0;
        lastTimestamp = 0;
        unreadCount = 0;
        isActive = false;
        for (int i = 0; i < MAX_MESSAGES_PER_CONV; i++) {
            messages[i].clear();
        }
    }

    // Add a message to this conversation (circular buffer)
    void addMessage(const char* text, bool outgoing, uint32_t timestamp) {
        if (!text) return;

        // Add to circular buffer
        int idx = messageHead;
        messages[idx].timestamp = timestamp;
        strncpy(messages[idx].text, text, MAX_MESSAGE_LENGTH - 1);
        messages[idx].text[MAX_MESSAGE_LENGTH - 1] = '\0';
        messages[idx].isOutgoing = outgoing;
        messages[idx].delivered = outgoing;  // Outgoing are "delivered" immediately

        // Advance head
        messageHead = (messageHead + 1) % MAX_MESSAGES_PER_CONV;
        if (messageCount < MAX_MESSAGES_PER_CONV) {
            messageCount++;
        }

        // Update metadata
        lastTimestamp = timestamp;
        if (!outgoing) {
            unreadCount++;
        }
    }

    // Get message by index (0 = oldest in buffer)
    const DMMessage* getMessage(int index) const {
        if (index < 0 || index >= messageCount) return nullptr;

        // Calculate actual array index accounting for circular buffer
        int start = (messageHead - messageCount + MAX_MESSAGES_PER_CONV) % MAX_MESSAGES_PER_CONV;
        int actualIdx = (start + index) % MAX_MESSAGES_PER_CONV;
        return &messages[actualIdx];
    }

    // Mark all as read
    void markAsRead() {
        unreadCount = 0;
    }
};

// =============================================================================
// DM SETTINGS
// =============================================================================

struct DMSettings {
    static constexpr int MAX_CONVERSATIONS = 16;
    static constexpr uint32_t DM_MAGIC = 0x4D42444D;  // "MBDM"

    DMConversation conversations[MAX_CONVERSATIONS];
    int numConversations;
    uint32_t magic;

    // Initialize with empty settings
    void setDefaults() {
        for (int i = 0; i < MAX_CONVERSATIONS; i++) {
            conversations[i].clear();
        }
        numConversations = 0;
        magic = DM_MAGIC;
    }

    // Validate settings
    bool isValid() const {
        return magic == DM_MAGIC;
    }

    // Find conversation by contact ID
    int findConversation(uint32_t contactId) const {
        for (int i = 0; i < MAX_CONVERSATIONS; i++) {
            if (conversations[i].isActive && conversations[i].contactId == contactId) {
                return i;
            }
        }
        return -1;
    }

    // Get or create conversation for a contact
    int getOrCreateConversation(uint32_t contactId) {
        // Check if exists
        int idx = findConversation(contactId);
        if (idx >= 0) return idx;

        // Find empty slot
        for (int i = 0; i < MAX_CONVERSATIONS; i++) {
            if (!conversations[i].isActive) {
                conversations[i].clear();
                conversations[i].contactId = contactId;
                conversations[i].isActive = true;
                numConversations++;
                return i;
            }
        }

        // Evict oldest conversation (by lastTimestamp)
        int oldestIdx = 0;
        uint32_t oldestTime = conversations[0].lastTimestamp;
        for (int i = 1; i < MAX_CONVERSATIONS; i++) {
            if (conversations[i].lastTimestamp < oldestTime) {
                oldestTime = conversations[i].lastTimestamp;
                oldestIdx = i;
            }
        }

        conversations[oldestIdx].clear();
        conversations[oldestIdx].contactId = contactId;
        conversations[oldestIdx].isActive = true;
        return oldestIdx;
    }

    // Add message to a contact's conversation
    void addMessage(uint32_t contactId, const char* text, bool outgoing, uint32_t timestamp) {
        int idx = getOrCreateConversation(contactId);
        if (idx >= 0) {
            conversations[idx].addMessage(text, outgoing, timestamp);
        }
    }

    // Get conversation by index
    DMConversation* getConversation(int idx) {
        if (idx < 0 || idx >= MAX_CONVERSATIONS) return nullptr;
        if (!conversations[idx].isActive) return nullptr;
        return &conversations[idx];
    }

    const DMConversation* getConversation(int idx) const {
        if (idx < 0 || idx >= MAX_CONVERSATIONS) return nullptr;
        if (!conversations[idx].isActive) return nullptr;
        return &conversations[idx];
    }

    // Get total unread count across all conversations
    int getTotalUnreadCount() const {
        int total = 0;
        for (int i = 0; i < MAX_CONVERSATIONS; i++) {
            if (conversations[i].isActive) {
                total += conversations[i].unreadCount;
            }
        }
        return total;
    }

    // Get number of active conversations
    int getActiveConversationCount() const {
        int count = 0;
        for (int i = 0; i < MAX_CONVERSATIONS; i++) {
            if (conversations[i].isActive) count++;
        }
        return count;
    }
};

#endif // MESHBERRY_DM_SETTINGS_H
