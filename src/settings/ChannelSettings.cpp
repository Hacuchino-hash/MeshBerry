/**
 * MeshBerry Channel Settings Implementation
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2026 NodakMesh (nodakmesh.org)
 *
 * This file is part of MeshBerry.
 */

#include "ChannelSettings.h"
#include "../crypto/ChannelCrypto.h"
#include <string.h>

void ChannelSettings::setDefaults() {
    // Clear all channels
    for (int i = 0; i < MAX_CHANNELS; i++) {
        channels[i].clear();
    }

    numChannels = 0;
    activeChannel = 0;

    // Add default Public channel
    addChannel("Public", PUBLIC_CHANNEL_PSK);

    magic = CHANNEL_MAGIC;
}

int ChannelSettings::addChannel(const char* name, const char* psk_base64) {
    if (numChannels >= MAX_CHANNELS) {
        return -1;
    }

    ChannelEntry& ch = channels[numChannels];
    ch.clear();

    // Copy name
    strncpy(ch.name, name, sizeof(ch.name) - 1);
    ch.name[sizeof(ch.name) - 1] = '\0';

    // Decode PSK
    int keyLen = ChannelCrypto::decodePSK(psk_base64, ch.secret, sizeof(ch.secret));
    if (keyLen == 0) {
        return -1;  // Invalid PSK
    }
    ch.secretLen = keyLen;

    // Derive hash
    ChannelCrypto::deriveHash(ch.secret, ch.secretLen, &ch.hash);

    ch.isHashtag = false;
    ch.isActive = true;

    return numChannels++;
}

int ChannelSettings::addHashtagChannel(const char* hashtag) {
    if (numChannels >= MAX_CHANNELS) {
        return -1;
    }

    // Ensure it starts with #
    if (!hashtag || hashtag[0] != '#') {
        return -1;
    }

    ChannelEntry& ch = channels[numChannels];
    ch.clear();

    // Copy name
    strncpy(ch.name, hashtag, sizeof(ch.name) - 1);
    ch.name[sizeof(ch.name) - 1] = '\0';

    // Derive key from name using SHA256 (first 16 bytes - matches MeshCore)
    ChannelCrypto::deriveKeyFromName(hashtag, ch.secret);
    ch.secretLen = 16;  // MeshCore hashtag channels use 16-byte keys

    // Derive hash
    ChannelCrypto::deriveHash(ch.secret, ch.secretLen, &ch.hash);

    // Generate base64 PSK for sharing with other MeshCore devices
    char pskBase64[32];
    ChannelCrypto::encodePSK(ch.secret, 16, pskBase64);
    Serial.printf("[CHANNEL] Hashtag '%s' created (hash=0x%02X)\n", hashtag, ch.hash);
    Serial.printf("[CHANNEL] PSK: %s\n", pskBase64);
    Serial.print("[CHANNEL] Key: ");
    for (int i = 0; i < 16; i++) Serial.printf("%02X", ch.secret[i]);
    Serial.println();

    ch.isHashtag = true;
    ch.isActive = true;

    return numChannels++;
}

bool ChannelSettings::removeChannel(int idx) {
    // Don't allow removing the Public channel (index 0)
    if (idx <= 0 || idx >= numChannels) {
        return false;
    }

    // Shift remaining channels down
    for (int i = idx; i < numChannels - 1; i++) {
        channels[i] = channels[i + 1];
    }

    // Clear last slot
    channels[numChannels - 1].clear();
    numChannels--;

    // Adjust active channel if needed
    if (activeChannel >= numChannels) {
        activeChannel = numChannels - 1;
    }
    if (activeChannel == idx) {
        activeChannel = 0;  // Fall back to Public
    } else if (activeChannel > idx) {
        activeChannel--;
    }

    return true;
}

int ChannelSettings::findChannel(const char* name) const {
    for (int i = 0; i < numChannels; i++) {
        if (strcmp(channels[i].name, name) == 0) {
            return i;
        }
    }
    return -1;
}

ChannelEntry* ChannelSettings::getActiveChannel() {
    if (activeChannel < numChannels) {
        return &channels[activeChannel];
    }
    return nullptr;
}

const ChannelEntry* ChannelSettings::getActiveChannel() const {
    if (activeChannel < numChannels) {
        return &channels[activeChannel];
    }
    return nullptr;
}

bool ChannelSettings::setActiveChannel(int idx) {
    if (idx >= 0 && idx < numChannels && channels[idx].isActive) {
        activeChannel = idx;
        return true;
    }
    return false;
}

bool ChannelSettings::isValid() const {
    if (magic != CHANNEL_MAGIC) return false;
    if (numChannels > MAX_CHANNELS) return false;
    if (activeChannel >= numChannels && numChannels > 0) return false;

    // Verify at least Public channel exists
    if (numChannels == 0) return false;

    // Verify each active channel has valid key
    for (int i = 0; i < numChannels; i++) {
        if (channels[i].isActive) {
            if (channels[i].secretLen != 16 && channels[i].secretLen != 32) {
                return false;
            }
        }
    }

    return true;
}
