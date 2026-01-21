/**
 * MeshBerry Message Archive Implementation
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2026 NodakMesh (nodakmesh.org)
 *
 * Persistent storage for channel and DM messages.
 */

#include "MessageArchive.h"
#include "../drivers/storage.h"
#include <string.h>

// Path templates
static const char* CHANNEL_PATH_FMT = "/channels/ch%d.bin";
static const char* DM_PATH_FMT = "/dms/dm_%08X.bin";

// Buffer for path building
static char pathBuffer[48];

namespace MessageArchive {

void init() {
    // Ensure directories exist
    Storage::createDir("/channels");
    Storage::createDir("/dms");
    Serial.println("[ARCHIVE] Message archive initialized");
}

// =========================================================================
// HELPER FUNCTIONS
// =========================================================================

static const char* getChannelPath(int channelIdx) {
    snprintf(pathBuffer, sizeof(pathBuffer), CHANNEL_PATH_FMT, channelIdx);
    return pathBuffer;
}

static const char* getDMPath(uint32_t contactId) {
    snprintf(pathBuffer, sizeof(pathBuffer), DM_PATH_FMT, contactId);
    return pathBuffer;
}

/**
 * Read archive header from file
 * @return true if valid header read, false if file doesn't exist or invalid
 */
static bool readHeader(const char* path, ArchiveHeader& header) {
    if (!Storage::fileExists(path)) {
        return false;
    }

    size_t bytesRead = 0;
    if (!Storage::readFile(path, (uint8_t*)&header, sizeof(ArchiveHeader), &bytesRead)) {
        return false;
    }

    if (bytesRead < sizeof(ArchiveHeader)) {
        return false;
    }

    // Validate magic and version
    if (header.magic != ARCHIVE_MAGIC || header.version != ARCHIVE_VERSION) {
        Serial.printf("[ARCHIVE] Invalid header in %s (magic=0x%08X, ver=%d)\n",
                      path, header.magic, header.version);
        return false;
    }

    return true;
}

/**
 * Write archive header to file
 */
static bool writeHeader(const char* path, const ArchiveHeader& header) {
    return Storage::writeFile(path, (const uint8_t*)&header, sizeof(ArchiveHeader));
}

/**
 * Create new archive file with header
 */
static bool createArchive(const char* path) {
    ArchiveHeader header;
    header.magic = ARCHIVE_MAGIC;
    header.version = ARCHIVE_VERSION;
    header.messageCount = 0;
    header.reserved = 0;

    return writeHeader(path, header);
}

/**
 * Append a message to an archive file, handling rotation if needed
 */
static bool appendMessage(const char* path, const ArchivedMessage& msg) {
    ArchiveHeader header;

    // Check if file exists
    if (!readHeader(path, header)) {
        // Create new archive
        if (!createArchive(path)) {
            Serial.printf("[ARCHIVE] Failed to create %s\n", path);
            return false;
        }
        header.magic = ARCHIVE_MAGIC;
        header.version = ARCHIVE_VERSION;
        header.messageCount = 0;
        header.reserved = 0;
    }

    // Check if we need to rotate (remove oldest messages)
    if (header.messageCount >= MAX_ARCHIVED_MESSAGES) {
        // Read all messages, drop oldest, rewrite file
        ArchivedMessage* buffer = new ArchivedMessage[MAX_ARCHIVED_MESSAGES];
        if (!buffer) {
            Serial.println("[ARCHIVE] Out of memory for rotation");
            return false;
        }

        // Read existing messages
        size_t bytesRead = 0;
        size_t dataSize = header.messageCount * sizeof(ArchivedMessage);

        // Open file and seek past header
        // Since Storage doesn't have seek, we need to read the whole file
        size_t totalSize = sizeof(ArchiveHeader) + dataSize;
        uint8_t* fileBuffer = new uint8_t[totalSize];
        if (!fileBuffer) {
            delete[] buffer;
            Serial.println("[ARCHIVE] Out of memory for file buffer");
            return false;
        }

        if (!Storage::readFile(path, fileBuffer, totalSize, &bytesRead)) {
            delete[] buffer;
            delete[] fileBuffer;
            return false;
        }

        // Copy messages (skip header)
        memcpy(buffer, fileBuffer + sizeof(ArchiveHeader), dataSize);
        delete[] fileBuffer;

        // Keep last (MAX_ARCHIVED_MESSAGES - 10) messages to make room
        int keepCount = MAX_ARCHIVED_MESSAGES - 10;
        int skipCount = header.messageCount - keepCount;

        // Create new file with rotated messages
        header.messageCount = keepCount;
        if (!writeHeader(path, header)) {
            delete[] buffer;
            return false;
        }

        // Append kept messages
        if (!Storage::appendFile(path, (const uint8_t*)&buffer[skipCount],
                                  keepCount * sizeof(ArchivedMessage))) {
            delete[] buffer;
            return false;
        }

        delete[] buffer;
    }

    // Append new message
    if (!Storage::appendFile(path, (const uint8_t*)&msg, sizeof(ArchivedMessage))) {
        Serial.printf("[ARCHIVE] Failed to append to %s\n", path);
        return false;
    }

    // Update header with new count
    header.messageCount++;

    // Rewrite header (need to read whole file, update header, write back)
    // For efficiency, we'll read the file, update header in memory, write back
    size_t fileSize = Storage::getFileSize(path);
    uint8_t* fileData = new uint8_t[fileSize];
    if (!fileData) {
        Serial.println("[ARCHIVE] Out of memory for header update");
        return false;
    }

    size_t bytesRead = 0;
    if (!Storage::readFile(path, fileData, fileSize, &bytesRead)) {
        delete[] fileData;
        return false;
    }

    // Update header in buffer
    memcpy(fileData, &header, sizeof(ArchiveHeader));

    // Write back
    bool success = Storage::writeFile(path, fileData, fileSize);
    delete[] fileData;

    return success;
}

/**
 * Load messages from archive file
 * @return Number of messages loaded
 */
static int loadMessages(const char* path, ArchivedMessage* buffer, int maxCount) {
    ArchiveHeader header;
    if (!readHeader(path, header)) {
        return 0;
    }

    int loadCount = (header.messageCount < (uint32_t)maxCount) ?
                    header.messageCount : maxCount;

    if (loadCount == 0) {
        return 0;
    }

    // Calculate which messages to load (newest ones)
    int skipCount = 0;
    if (header.messageCount > (uint32_t)maxCount) {
        skipCount = header.messageCount - maxCount;
    }

    // Read entire file
    size_t fileSize = Storage::getFileSize(path);
    uint8_t* fileData = new uint8_t[fileSize];
    if (!fileData) {
        Serial.println("[ARCHIVE] Out of memory for load");
        return 0;
    }

    size_t bytesRead = 0;
    if (!Storage::readFile(path, fileData, fileSize, &bytesRead)) {
        delete[] fileData;
        return 0;
    }

    // Copy messages (skip header and older messages)
    size_t offset = sizeof(ArchiveHeader) + (skipCount * sizeof(ArchivedMessage));
    memcpy(buffer, fileData + offset, loadCount * sizeof(ArchivedMessage));

    delete[] fileData;

    Serial.printf("[ARCHIVE] Loaded %d messages from %s\n", loadCount, path);
    return loadCount;
}

/**
 * Get message count from archive
 */
static int getMessageCount(const char* path) {
    ArchiveHeader header;
    if (!readHeader(path, header)) {
        return 0;
    }
    return header.messageCount;
}

/**
 * Clear archive file
 */
static bool clearArchive(const char* path) {
    if (!Storage::fileExists(path)) {
        return true;  // Nothing to clear
    }
    return Storage::deleteFile(path);
}

// =========================================================================
// CHANNEL MESSAGES
// =========================================================================

bool saveChannelMessage(int channelIdx, const ArchivedMessage& msg) {
    if (channelIdx < 0 || channelIdx > 7) {
        return false;
    }
    return appendMessage(getChannelPath(channelIdx), msg);
}

int loadChannelMessages(int channelIdx, ArchivedMessage* buffer, int maxCount) {
    if (channelIdx < 0 || channelIdx > 7 || !buffer || maxCount <= 0) {
        return 0;
    }
    return loadMessages(getChannelPath(channelIdx), buffer, maxCount);
}

int getChannelMessageCount(int channelIdx) {
    if (channelIdx < 0 || channelIdx > 7) {
        return 0;
    }
    return getMessageCount(getChannelPath(channelIdx));
}

bool clearChannelMessages(int channelIdx) {
    if (channelIdx < 0 || channelIdx > 7) {
        return false;
    }
    return clearArchive(getChannelPath(channelIdx));
}

// =========================================================================
// DM MESSAGES
// =========================================================================

bool saveDMMessage(uint32_t contactId, const ArchivedMessage& msg) {
    if (contactId == 0) {
        return false;
    }
    return appendMessage(getDMPath(contactId), msg);
}

int loadDMMessages(uint32_t contactId, ArchivedMessage* buffer, int maxCount) {
    if (contactId == 0 || !buffer || maxCount <= 0) {
        return 0;
    }
    return loadMessages(getDMPath(contactId), buffer, maxCount);
}

int getDMMessageCount(uint32_t contactId) {
    if (contactId == 0) {
        return 0;
    }
    return getMessageCount(getDMPath(contactId));
}

bool clearDMMessages(uint32_t contactId) {
    if (contactId == 0) {
        return false;
    }
    return clearArchive(getDMPath(contactId));
}

// =========================================================================
// UTILITIES
// =========================================================================

int getTotalMessageCount() {
    int total = 0;

    // Count channel messages
    for (int i = 0; i < 8; i++) {
        total += getChannelMessageCount(i);
    }

    // DM messages would require directory enumeration
    // For now, just return channel count
    // TODO: Enumerate DM files if needed

    return total;
}

size_t getStorageUsed() {
    size_t total = 0;

    // Sum up channel file sizes
    for (int i = 0; i < 8; i++) {
        const char* path = getChannelPath(i);
        if (Storage::fileExists(path)) {
            total += Storage::getFileSize(path);
        }
    }

    // DM files would require directory enumeration
    // TODO: Enumerate DM files if needed

    return total;
}

} // namespace MessageArchive
