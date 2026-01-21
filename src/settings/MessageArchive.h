/**
 * MeshBerry Message Archive
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2026 NodakMesh (nodakmesh.org)
 *
 * Persistent storage for channel and DM messages.
 */

#ifndef MESHBERRY_MESSAGE_ARCHIVE_H
#define MESHBERRY_MESSAGE_ARCHIVE_H

#include <Arduino.h>

// Archive constants
#define ARCHIVE_MAGIC           0x4D424D53  // "MBMS" - MeshBerry Message Store
#define ARCHIVE_VERSION         1
#define MAX_ARCHIVED_MESSAGES   100         // Max messages per channel/DM
#define ARCHIVE_SENDER_LEN      16
#define ARCHIVE_TEXT_LEN        200

/**
 * Archived message structure
 * Fixed size for easy binary storage
 */
struct ArchivedMessage {
    uint32_t timestamp;                     // Unix timestamp
    char sender[ARCHIVE_SENDER_LEN];        // Sender name
    char text[ARCHIVE_TEXT_LEN];            // Message text
    uint8_t isOutgoing;                     // 1 = outgoing, 0 = incoming
    uint8_t reserved[3];                    // Padding for alignment

    void clear() {
        timestamp = 0;
        sender[0] = '\0';
        text[0] = '\0';
        isOutgoing = 0;
        memset(reserved, 0, sizeof(reserved));
    }
};

// Size: 4 + 16 + 200 + 1 + 3 = 224 bytes per message

/**
 * Archive file header
 */
struct ArchiveHeader {
    uint32_t magic;
    uint32_t version;
    uint32_t messageCount;
    uint32_t reserved;
};

// Size: 16 bytes

namespace MessageArchive {

/**
 * Initialize the message archive system
 */
void init();

// =========================================================================
// CHANNEL MESSAGES
// =========================================================================

/**
 * Save a message to channel archive
 * @param channelIdx Channel index (0-7)
 * @param msg Message to save
 * @return true on success
 */
bool saveChannelMessage(int channelIdx, const ArchivedMessage& msg);

/**
 * Load messages from channel archive
 * @param channelIdx Channel index
 * @param buffer Buffer to load messages into
 * @param maxCount Maximum messages to load
 * @return Number of messages loaded
 */
int loadChannelMessages(int channelIdx, ArchivedMessage* buffer, int maxCount);

/**
 * Get count of messages in channel archive
 */
int getChannelMessageCount(int channelIdx);

/**
 * Clear all messages in a channel
 */
bool clearChannelMessages(int channelIdx);

// =========================================================================
// DM MESSAGES
// =========================================================================

/**
 * Save a message to DM archive
 * @param contactId Contact's unique ID
 * @param msg Message to save
 * @return true on success
 */
bool saveDMMessage(uint32_t contactId, const ArchivedMessage& msg);

/**
 * Load messages from DM archive
 * @param contactId Contact's unique ID
 * @param buffer Buffer to load messages into
 * @param maxCount Maximum messages to load
 * @return Number of messages loaded
 */
int loadDMMessages(uint32_t contactId, ArchivedMessage* buffer, int maxCount);

/**
 * Get count of messages in DM archive
 */
int getDMMessageCount(uint32_t contactId);

/**
 * Clear all messages for a contact
 */
bool clearDMMessages(uint32_t contactId);

// =========================================================================
// UTILITIES
// =========================================================================

/**
 * Get total archived message count (all channels + DMs)
 */
int getTotalMessageCount();

/**
 * Get storage space used by archives in bytes
 */
size_t getStorageUsed();

} // namespace MessageArchive

#endif // MESHBERRY_MESSAGE_ARCHIVE_H
