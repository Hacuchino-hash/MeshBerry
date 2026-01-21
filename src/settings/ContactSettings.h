/**
 * MeshBerry Contact Settings
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2026 NodakMesh (nodakmesh.org)
 *
 * This file is part of MeshBerry.
 *
 * Persistent storage for discovered contacts/nodes.
 * Separates repeaters from regular chat nodes.
 */

#ifndef MESHBERRY_CONTACT_SETTINGS_H
#define MESHBERRY_CONTACT_SETTINGS_H

#include <Arduino.h>
#include "../mesh/MeshBerryMesh.h"

// =============================================================================
// DM ROUTING MODE
// =============================================================================

enum DMRoutingMode : uint8_t {
    DM_ROUTE_AUTO = 0,      // Auto: use direct if path known, flood otherwise
    DM_ROUTE_FLOOD = 1,     // Always use flood routing
    DM_ROUTE_DIRECT = 2,    // Always use direct routing (fail if no path)
    DM_ROUTE_MANUAL = 3     // Manual path through selected repeaters
};

// =============================================================================
// CONTACT ENTRY
// =============================================================================

struct ContactEntry {
    uint32_t id;            // Node ID (hash of public key)
    char name[32];          // Node name
    NodeType type;          // Node type (CHAT, REPEATER, ROOM, SENSOR)
    int16_t lastRssi;       // Last received signal strength
    float lastSnr;          // Last SNR
    uint32_t lastHeard;     // Timestamp when last heard
    bool isFavorite;        // User marked as favorite
    bool isActive;          // Entry is in use
    uint8_t pubKey[32];     // Full public key (for repeater login)
    char savedPassword[16]; // Saved admin password (empty = not saved)

    // Routing info - learned paths for direct routing
    uint8_t outPath[64];    // Learned path to this contact (max 64 hops)
    int8_t outPathLen;      // -1 = unknown, 0+ = valid path length
    uint32_t pathLearnedAt; // millis() when path was learned

    // DM routing preferences
    DMRoutingMode routingMode;  // How to route DMs to this contact
    uint8_t manualPath[8];      // Manual path via repeater IDs (up to 8 hops)
    uint8_t manualPathLen;      // Length of manual path

    void clear() {
        id = 0;
        name[0] = '\0';
        type = NODE_TYPE_UNKNOWN;
        lastRssi = 0;
        lastSnr = 0;
        lastHeard = 0;
        isFavorite = false;
        isActive = false;
        memset(pubKey, 0, sizeof(pubKey));
        savedPassword[0] = '\0';
        // Clear routing info
        memset(outPath, 0, sizeof(outPath));
        outPathLen = -1;  // Unknown path
        pathLearnedAt = 0;
        // Clear DM routing preferences
        routingMode = DM_ROUTE_AUTO;
        memset(manualPath, 0, sizeof(manualPath));
        manualPathLen = 0;
    }

    bool hasSavedPassword() const {
        return savedPassword[0] != '\0';
    }
};

// =============================================================================
// CONTACT SETTINGS
// =============================================================================

struct ContactSettings {
    static constexpr int MAX_CONTACTS = 32;
    static constexpr uint32_t CONTACT_MAGIC = 0x4D424354;  // "MBCT"

    ContactEntry contacts[MAX_CONTACTS];
    int numContacts;
    uint32_t magic;

    // Initialize with empty contact list
    void setDefaults() {
        for (int i = 0; i < MAX_CONTACTS; i++) {
            contacts[i].clear();
        }
        numContacts = 0;
        magic = CONTACT_MAGIC;
    }

    // Add or update contact from NodeInfo
    int addOrUpdateContact(const NodeInfo& node);

    // Add or update contact with public key
    int addOrUpdateContact(const NodeInfo& node, const uint8_t* pubKey);

    // Save password for a contact
    bool savePassword(int idx, const char* password);

    // Clear saved password for a contact
    bool clearPassword(int idx);

    // Remove contact by index
    bool removeContact(int idx);

    // Find contact by ID
    int findContact(uint32_t id) const;

    // Find contact by full public key (32 bytes) - prevents duplicates
    int findContactByPubKey(const uint8_t* pubKey) const;

    // Find contact by name prefix
    int findContactByName(const char* namePrefix) const;

    // Get contact by index
    ContactEntry* getContact(int idx);
    const ContactEntry* getContact(int idx) const;

    // Validate settings
    bool isValid() const;

    // Count contacts by type
    int countByType(NodeType type) const;

    // Get all contacts of a specific type
    int getContactsByType(NodeType type, int* indices, int maxIndices) const;

    // Count repeaters
    int countRepeaters() const { return countByType(NODE_TYPE_REPEATER); }

    // Count chat nodes (non-repeaters)
    int countChatNodes() const { return countByType(NODE_TYPE_CHAT); }

    // Toggle favorite status
    bool toggleFavorite(int idx);
};

// =============================================================================
// FILTER OPTIONS
// =============================================================================

enum ContactFilter : uint8_t {
    FILTER_ALL = 0,
    FILTER_REPEATERS = 1,
    FILTER_CHAT_NODES = 2,
    FILTER_FAVORITES = 3
};

#endif // MESHBERRY_CONTACT_SETTINGS_H
