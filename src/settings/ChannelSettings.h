/**
 * MeshBerry Channel Settings
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2026 NodakMesh (nodakmesh.org)
 *
 * This file is part of MeshBerry.
 *
 * Runtime-configurable channel settings with encryption support.
 * Compatible with MeshCore channel protocol.
 */

#ifndef MESHBERRY_CHANNEL_SETTINGS_H
#define MESHBERRY_CHANNEL_SETTINGS_H

#include <Arduino.h>

// Maximum number of channels
#define MAX_CHANNELS 8

// Public channel PSK (MeshCore default)
#define PUBLIC_CHANNEL_PSK "izOH6cXN6mrJ5e26oRXNcg=="

// =============================================================================
// CHANNEL ENTRY
// =============================================================================

struct ChannelEntry {
    char name[32];           // Channel name (e.g., "Public", "#local")
    uint8_t secret[32];      // Encryption key (raw bytes, up to 32)
    uint8_t secretLen;       // Actual key length (16 or 32)
    uint8_t hash;            // 1-byte channel hash for packet matching
    bool isHashtag;          // True if key was derived from name
    bool isActive;           // Channel is in use

    void clear() {
        memset(name, 0, sizeof(name));
        memset(secret, 0, sizeof(secret));
        secretLen = 0;
        hash = 0;
        isHashtag = false;
        isActive = false;
    }
};

// =============================================================================
// CHANNEL SETTINGS
// =============================================================================

struct ChannelSettings {
    ChannelEntry channels[MAX_CHANNELS];
    uint8_t numChannels;
    uint32_t magic;

    static constexpr uint32_t CHANNEL_MAGIC = 0x4D424348;  // "MBCH"

    /**
     * Initialize with defaults (Public channel only)
     */
    void setDefaults();

    /**
     * Add a channel with base64 PSK
     * @param name Channel name
     * @param psk_base64 Base64-encoded pre-shared key
     * @return Index of new channel, or -1 on error
     */
    int addChannel(const char* name, const char* psk_base64);

    /**
     * Add a #hashtag channel (key derived from name)
     * @param hashtag Channel name starting with # (e.g., "#local")
     * @return Index of new channel, or -1 on error
     */
    int addHashtagChannel(const char* hashtag);

    /**
     * Remove a channel by index
     * @param idx Channel index
     * @return true if removed
     */
    bool removeChannel(int idx);

    /**
     * Find channel by name
     * @param name Channel name to find
     * @return Index of channel, or -1 if not found
     */
    int findChannel(const char* name) const;

    /**
     * Validate settings
     * @return true if valid
     */
    bool isValid() const;
};

#endif // MESHBERRY_CHANNEL_SETTINGS_H
