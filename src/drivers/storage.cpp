/**
 * MeshBerry Storage Driver Implementation
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2026 NodakMesh (nodakmesh.org)
 */

#include "storage.h"
#include "../config.h"
#include <SD.h>
#include <SPIFFS.h>
#include <SPI.h>

// Storage state
static bool sdAvailable = false;
static bool spiffsAvailable = false;
static bool initialized = false;

// Use the same SPI bus as display (HSPI)
static SPIClass* sdSPI = nullptr;

namespace Storage {

bool init() {
    if (initialized) {
        return isAvailable();
    }

    Serial.println("[STORAGE] Initializing storage...");

    // First try SD card
    Serial.println("[STORAGE] Attempting SD card mount...");

    // Initialize SPI for SD card (shared bus with display)
    sdSPI = new SPIClass(HSPI);
    sdSPI->begin(PIN_SD_SCK, PIN_SD_MISO, PIN_SD_MOSI, PIN_SD_CS);

    // Try to mount SD card
    if (SD.begin(PIN_SD_CS, *sdSPI, 4000000)) {  // 4MHz for compatibility
        sdAvailable = true;
        uint64_t cardSize = SD.cardSize() / (1024 * 1024);
        Serial.printf("[STORAGE] SD card mounted: %lluMB\n", cardSize);

        // Create base directory for MeshBerry
        if (!SD.exists("/meshberry")) {
            SD.mkdir("/meshberry");
        }
        if (!SD.exists("/meshberry/channels")) {
            SD.mkdir("/meshberry/channels");
        }
        if (!SD.exists("/meshberry/dms")) {
            SD.mkdir("/meshberry/dms");
        }
    } else {
        Serial.println("[STORAGE] SD card not available");
        sdAvailable = false;
    }

    // SPIFFS should already be initialized by SettingsManager
    // But check if it's available
    if (SPIFFS.begin(false)) {  // false = don't format if mount fails
        spiffsAvailable = true;
        Serial.printf("[STORAGE] SPIFFS available: %d bytes free\n",
                      SPIFFS.totalBytes() - SPIFFS.usedBytes());
    } else {
        Serial.println("[STORAGE] SPIFFS not available");
        spiffsAvailable = false;
    }

    initialized = true;

    if (sdAvailable) {
        Serial.println("[STORAGE] Using SD card for message storage");
    } else if (spiffsAvailable) {
        Serial.println("[STORAGE] Using SPIFFS fallback for message storage");
    } else {
        Serial.println("[STORAGE] WARNING: No storage available!");
    }

    return isAvailable();
}

bool isSDAvailable() {
    return sdAvailable;
}

bool isAvailable() {
    return sdAvailable || spiffsAvailable;
}

const char* getStorageType() {
    if (sdAvailable) return "SD";
    if (spiffsAvailable) return "SPIFFS";
    return "None";
}

// Helper to get the appropriate filesystem
static fs::FS& getFS() {
    if (sdAvailable) {
        return SD;
    }
    return SPIFFS;
}

// Helper to build full path with appropriate prefix
static String buildPath(const char* path) {
    // For SD card, files go under /meshberry
    // For SPIFFS, files go in root (limited space)
    if (sdAvailable) {
        if (path[0] == '/') {
            return String("/meshberry") + path;
        } else {
            return String("/meshberry/") + path;
        }
    } else {
        // SPIFFS - use path as-is but ensure it starts with /
        if (path[0] == '/') {
            return String(path);
        } else {
            return String("/") + path;
        }
    }
}

bool writeFile(const char* path, const uint8_t* data, size_t len) {
    if (!isAvailable()) return false;

    String fullPath = buildPath(path);
    File file = getFS().open(fullPath.c_str(), FILE_WRITE);
    if (!file) {
        Serial.printf("[STORAGE] Failed to open %s for writing\n", fullPath.c_str());
        return false;
    }

    size_t written = file.write(data, len);
    file.close();

    if (written != len) {
        Serial.printf("[STORAGE] Write incomplete: %d/%d bytes\n", written, len);
        return false;
    }

    return true;
}

bool appendFile(const char* path, const uint8_t* data, size_t len) {
    if (!isAvailable()) return false;

    String fullPath = buildPath(path);
    File file = getFS().open(fullPath.c_str(), FILE_APPEND);
    if (!file) {
        // File might not exist, try creating it
        file = getFS().open(fullPath.c_str(), FILE_WRITE);
        if (!file) {
            Serial.printf("[STORAGE] Failed to open %s for append\n", fullPath.c_str());
            return false;
        }
    }

    size_t written = file.write(data, len);
    file.close();

    if (written != len) {
        Serial.printf("[STORAGE] Append incomplete: %d/%d bytes\n", written, len);
        return false;
    }

    return true;
}

bool readFile(const char* path, uint8_t* buffer, size_t maxLen, size_t* bytesRead) {
    if (!isAvailable()) return false;

    String fullPath = buildPath(path);
    File file = getFS().open(fullPath.c_str(), FILE_READ);
    if (!file) {
        return false;
    }

    size_t read = file.read(buffer, maxLen);
    file.close();

    if (bytesRead) {
        *bytesRead = read;
    }

    return true;
}

bool fileExists(const char* path) {
    if (!isAvailable()) return false;

    String fullPath = buildPath(path);
    return getFS().exists(fullPath.c_str());
}

bool deleteFile(const char* path) {
    if (!isAvailable()) return false;

    String fullPath = buildPath(path);
    return getFS().remove(fullPath.c_str());
}

size_t getFileSize(const char* path) {
    if (!isAvailable()) return 0;

    String fullPath = buildPath(path);
    File file = getFS().open(fullPath.c_str(), FILE_READ);
    if (!file) {
        return 0;
    }

    size_t size = file.size();
    file.close();
    return size;
}

bool createDir(const char* path) {
    if (!isAvailable()) return false;

    String fullPath = buildPath(path);

    // SPIFFS doesn't support directories, just return true
    if (!sdAvailable) {
        return true;
    }

    return SD.mkdir(fullPath.c_str());
}

size_t getAvailableSpace() {
    if (sdAvailable) {
        return SD.totalBytes() - SD.usedBytes();
    } else if (spiffsAvailable) {
        return SPIFFS.totalBytes() - SPIFFS.usedBytes();
    }
    return 0;
}

size_t getTotalSpace() {
    if (sdAvailable) {
        return SD.totalBytes();
    } else if (spiffsAvailable) {
        return SPIFFS.totalBytes();
    }
    return 0;
}

} // namespace Storage
