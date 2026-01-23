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
#include <SD.h>
#include <SPIFFS.h>

// Path templates
static const char* CHANNEL_PATH_FMT = "/channels/ch%d.bin";
static const char* DM_PATH_FMT = "/dms/dm_%08X.bin";

// Buffer for path building
static char pathBuffer[48];

// Helper to get the appropriate filesystem (SD or SPIFFS)
static fs::FS& getFS() {
    if (Storage::isSDAvailable()) {
        return SD;
    }
    return SPIFFS;
}

// Helper to build full path with appropriate prefix
static const char* buildFullPath(const char* path, char* buffer, size_t bufferSize) {
    if (Storage::isSDAvailable()) {
        // SD card: prefix with /meshberry
        if (path[0] == '/') {
            snprintf(buffer, bufferSize, "/meshberry%s", path);
        } else {
            snprintf(buffer, bufferSize, "/meshberry/%s", path);
        }
    } else {
        // SPIFFS: use path as-is but ensure it starts with /
        if (path[0] == '/') {
            snprintf(buffer, bufferSize, "%s", path);
        } else {
            snprintf(buffer, bufferSize, "/%s", path);
        }
    }
    return buffer;
}

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
 * OPTIMIZED: Uses seek operations to avoid heap allocations
 */
static bool appendMessage(const char* path, const ArchivedMessage& msg) {
    char fullPath[256];
    buildFullPath(path, fullPath, sizeof(fullPath));

    ArchiveHeader header;

    // Check if file exists and read header
    File file = getFS().open(fullPath, "r+");  // Read-write mode
    if (!file) {
        // File doesn't exist, create new archive
        if (!createArchive(path)) {
            Serial.printf("[ARCHIVE] Failed to create %s\n", path);
            return false;
        }
        // Open newly created file
        file = getFS().open(fullPath, "r+");
        if (!file) {
            return false;
        }
        header.magic = ARCHIVE_MAGIC;
        header.version = ARCHIVE_VERSION;
        header.messageCount = 0;
        header.reserved = 0;
    } else {
        // Read existing header
        file.read((uint8_t*)&header, sizeof(ArchiveHeader));

        if (header.magic != ARCHIVE_MAGIC) {
            file.close();
            Serial.printf("[ARCHIVE] Invalid header in %s\n", path);
            return false;
        }
    }

    // Check if we need to rotate (remove oldest messages)
    if (header.messageCount >= MAX_ARCHIVED_MESSAGES) {
        file.close();  // Close before rotation

        // Rotation still needs heap allocation, but happens much less frequently (every 100 messages)
        // TODO: Optimize rotation in future if needed
        ArchivedMessage* buffer = new ArchivedMessage[MAX_ARCHIVED_MESSAGES];
        if (!buffer) {
            Serial.println("[ARCHIVE] Out of memory for rotation");
            return false;
        }

        // Reopen and read messages
        file = getFS().open(fullPath, "r");
        if (!file) {
            delete[] buffer;
            return false;
        }

        file.seek(sizeof(ArchiveHeader));  // Skip header
        size_t messagesRead = file.read((uint8_t*)buffer, header.messageCount * sizeof(ArchivedMessage));
        file.close();

        if (messagesRead != header.messageCount * sizeof(ArchivedMessage)) {
            delete[] buffer;
            Serial.println("[ARCHIVE] Failed to read messages for rotation");
            return false;
        }

        // Keep last (MAX_ARCHIVED_MESSAGES - 10) messages to make room
        int keepCount = MAX_ARCHIVED_MESSAGES - 10;
        int skipCount = header.messageCount - keepCount;

        // Rewrite file with rotated messages
        header.messageCount = keepCount;
        header.reserved = 0;

        file = getFS().open(fullPath, "w");  // Overwrite mode
        if (!file) {
            delete[] buffer;
            return false;
        }

        // Write header
        file.write((uint8_t*)&header, sizeof(ArchiveHeader));
        // Write kept messages
        file.write((uint8_t*)&buffer[skipCount], keepCount * sizeof(ArchivedMessage));
        file.close();

        delete[] buffer;

        // Reopen for appending new message
        file = getFS().open(fullPath, "r+");
        if (!file) {
            return false;
        }
        file.read((uint8_t*)&header, sizeof(ArchiveHeader));
    }

    // Append new message to end of file
    file.seek(0, SeekEnd);
    size_t written = file.write((uint8_t*)&msg, sizeof(ArchivedMessage));

    if (written != sizeof(ArchivedMessage)) {
        file.close();
        Serial.printf("[ARCHIVE] Failed to append to %s\n", path);
        return false;
    }

    // Update header count (in-place, NO heap allocation)
    header.messageCount++;
    file.seek(0, SeekSet);  // Go to start of file
    file.write((uint8_t*)&header, sizeof(ArchiveHeader));

    file.close();
    return true;
}

/**
 * Load messages from archive file
 * OPTIMIZED: Streams messages directly from file, NO heap allocation
 * @return Number of messages loaded
 */
static int loadMessages(const char* path, ArchivedMessage* buffer, int maxCount) {
    char fullPath[256];
    buildFullPath(path, fullPath, sizeof(fullPath));

    // Open file for reading
    File file = getFS().open(fullPath, "r");
    if (!file) {
        return 0;
    }

    // Read header
    ArchiveHeader header;
    size_t headerRead = file.read((uint8_t*)&header, sizeof(ArchiveHeader));
    if (headerRead != sizeof(ArchiveHeader) || header.magic != ARCHIVE_MAGIC) {
        file.close();
        return 0;
    }

    int loadCount = (header.messageCount < (uint32_t)maxCount) ?
                    header.messageCount : maxCount;

    if (loadCount == 0) {
        file.close();
        return 0;
    }

    // Calculate which messages to load (newest ones)
    int skipCount = 0;
    if (header.messageCount > (uint32_t)maxCount) {
        skipCount = header.messageCount - maxCount;
    }

    // Seek to first message to load (skip header and older messages)
    size_t offset = sizeof(ArchiveHeader) + (skipCount * sizeof(ArchivedMessage));
    file.seek(offset, SeekSet);

    // Stream messages directly into output buffer (NO heap allocation)
    size_t bytesToRead = loadCount * sizeof(ArchivedMessage);
    size_t bytesRead = file.read((uint8_t*)buffer, bytesToRead);

    file.close();

    if (bytesRead != bytesToRead) {
        Serial.printf("[ARCHIVE] Incomplete read from %s: %d/%d bytes\n",
                      path, bytesRead, bytesToRead);
        return bytesRead / sizeof(ArchivedMessage);  // Return partial load
    }

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
