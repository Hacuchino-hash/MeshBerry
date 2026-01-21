/**
 * MeshBerry Storage Driver
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2026 NodakMesh (nodakmesh.org)
 *
 * Unified storage abstraction with SD card primary and SPIFFS fallback.
 */

#ifndef MESHBERRY_STORAGE_H
#define MESHBERRY_STORAGE_H

#include <Arduino.h>

namespace Storage {

/**
 * Initialize storage system
 * Attempts to mount SD card first, falls back to SPIFFS if unavailable
 * @return true if at least one storage medium is available
 */
bool init();

/**
 * Check if SD card is currently available and mounted
 */
bool isSDAvailable();

/**
 * Check if any storage is available (SD or SPIFFS)
 */
bool isAvailable();

/**
 * Get storage type string for display
 */
const char* getStorageType();

/**
 * Write data to file
 * @param path File path (will be prefixed with appropriate root)
 * @param data Data buffer to write
 * @param len Length of data
 * @return true on success
 */
bool writeFile(const char* path, const uint8_t* data, size_t len);

/**
 * Append data to file
 * @param path File path
 * @param data Data buffer to append
 * @param len Length of data
 * @return true on success
 */
bool appendFile(const char* path, const uint8_t* data, size_t len);

/**
 * Read file contents
 * @param path File path
 * @param buffer Buffer to read into
 * @param maxLen Maximum bytes to read
 * @param bytesRead Output: actual bytes read
 * @return true on success
 */
bool readFile(const char* path, uint8_t* buffer, size_t maxLen, size_t* bytesRead);

/**
 * Check if file exists
 */
bool fileExists(const char* path);

/**
 * Delete a file
 */
bool deleteFile(const char* path);

/**
 * Get file size
 * @return File size in bytes, or 0 if file doesn't exist
 */
size_t getFileSize(const char* path);

/**
 * Create directory (and parent directories if needed)
 */
bool createDir(const char* path);

/**
 * Get available space in bytes
 */
size_t getAvailableSpace();

/**
 * Get total space in bytes
 */
size_t getTotalSpace();

} // namespace Storage

#endif // MESHBERRY_STORAGE_H
