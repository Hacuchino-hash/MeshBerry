/**
 * MeshBerry Contact Settings Implementation
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2026 NodakMesh (nodakmesh.org)
 *
 * This file is part of MeshBerry.
 */

#include "ContactSettings.h"
#include <string.h>

int ContactSettings::addOrUpdateContact(const NodeInfo& node) {
    // First, check if contact already exists
    int existingIdx = findContact(node.id);

    if (existingIdx >= 0) {
        // Update existing contact - preserve pubKey, favorite, password
        ContactEntry& contact = contacts[existingIdx];
        strncpy(contact.name, node.name, sizeof(contact.name) - 1);
        contact.name[sizeof(contact.name) - 1] = '\0';
        contact.type = node.type;
        contact.lastRssi = node.rssi;
        contact.lastSnr = node.snr;
        contact.lastHeard = node.lastHeard;
        // Keep: favorite status, pubKey, savedPassword (preserved automatically)
        return existingIdx;
    }

    // Add new contact
    if (numContacts >= MAX_CONTACTS) {
        // Find oldest non-favorite contact to replace
        int oldestIdx = -1;
        uint32_t oldestTime = UINT32_MAX;

        for (int i = 0; i < MAX_CONTACTS; i++) {
            if (!contacts[i].isFavorite && contacts[i].lastHeard < oldestTime) {
                oldestTime = contacts[i].lastHeard;
                oldestIdx = i;
            }
        }

        if (oldestIdx < 0) {
            // All contacts are favorites, can't add
            return -1;
        }

        // Replace oldest contact
        ContactEntry& contact = contacts[oldestIdx];
        contact.id = node.id;
        strncpy(contact.name, node.name, sizeof(contact.name) - 1);
        contact.name[sizeof(contact.name) - 1] = '\0';
        contact.type = node.type;
        contact.lastRssi = node.rssi;
        contact.lastSnr = node.snr;
        contact.lastHeard = node.lastHeard;
        contact.isFavorite = false;
        contact.isActive = true;
        memset(contact.pubKey, 0, sizeof(contact.pubKey));  // Clear old pubKey
        contact.savedPassword[0] = '\0';  // Clear old password
        return oldestIdx;
    }

    // Add to end
    ContactEntry& contact = contacts[numContacts];
    contact.id = node.id;
    strncpy(contact.name, node.name, sizeof(contact.name) - 1);
    contact.name[sizeof(contact.name) - 1] = '\0';
    contact.type = node.type;
    contact.lastRssi = node.rssi;
    contact.lastSnr = node.snr;
    contact.lastHeard = node.lastHeard;
    contact.isFavorite = false;
    contact.isActive = true;
    memset(contact.pubKey, 0, sizeof(contact.pubKey));  // Initialize pubKey to zeros
    contact.savedPassword[0] = '\0';  // Initialize password to empty

    return numContacts++;
}

int ContactSettings::addOrUpdateContact(const NodeInfo& node, const uint8_t* pubKey) {
    int idx = addOrUpdateContact(node);
    if (idx >= 0 && pubKey != nullptr) {
        // Debug: log incoming pubKey
        Serial.print("[CONTACT] Incoming pubKey: ");
        for (int i = 0; i < 8; i++) Serial.printf("%02X", pubKey[i]);
        Serial.println("...");

        // Only update pubKey if it's non-zero (valid key from advertisement)
        bool hasValidKey = false;
        for (int i = 0; i < 32; i++) {
            if (pubKey[i] != 0) {
                hasValidKey = true;
                break;
            }
        }
        if (hasValidKey) {
            memcpy(contacts[idx].pubKey, pubKey, 32);

            // Debug: verify what was stored
            Serial.print("[CONTACT] Stored pubKey: ");
            for (int i = 0; i < 8; i++) Serial.printf("%02X", contacts[idx].pubKey[i]);
            Serial.printf("... at idx=%d\n", idx);
        }
    }
    return idx;
}

bool ContactSettings::savePassword(int idx, const char* password) {
    if (idx < 0 || idx >= numContacts || !contacts[idx].isActive) {
        return false;
    }
    if (!password) {
        contacts[idx].savedPassword[0] = '\0';
        return true;
    }
    strncpy(contacts[idx].savedPassword, password, sizeof(contacts[idx].savedPassword) - 1);
    contacts[idx].savedPassword[sizeof(contacts[idx].savedPassword) - 1] = '\0';
    return true;
}

bool ContactSettings::clearPassword(int idx) {
    if (idx < 0 || idx >= numContacts || !contacts[idx].isActive) {
        return false;
    }
    contacts[idx].savedPassword[0] = '\0';
    return true;
}

bool ContactSettings::removeContact(int idx) {
    if (idx < 0 || idx >= numContacts) {
        return false;
    }

    // Shift remaining contacts down
    for (int i = idx; i < numContacts - 1; i++) {
        contacts[i] = contacts[i + 1];
    }

    // Clear last slot
    contacts[numContacts - 1].clear();
    numContacts--;

    return true;
}

int ContactSettings::findContact(uint32_t id) const {
    for (int i = 0; i < numContacts; i++) {
        if (contacts[i].isActive && contacts[i].id == id) {
            return i;
        }
    }
    return -1;
}

int ContactSettings::findContactByName(const char* namePrefix) const {
    if (!namePrefix || namePrefix[0] == '\0') return -1;

    size_t prefixLen = strlen(namePrefix);
    for (int i = 0; i < numContacts; i++) {
        if (contacts[i].isActive && strncasecmp(contacts[i].name, namePrefix, prefixLen) == 0) {
            return i;
        }
    }
    return -1;
}

ContactEntry* ContactSettings::getContact(int idx) {
    if (idx >= 0 && idx < numContacts && contacts[idx].isActive) {
        return &contacts[idx];
    }
    return nullptr;
}

const ContactEntry* ContactSettings::getContact(int idx) const {
    if (idx >= 0 && idx < numContacts && contacts[idx].isActive) {
        return &contacts[idx];
    }
    return nullptr;
}

bool ContactSettings::isValid() const {
    if (magic != CONTACT_MAGIC) return false;
    if (numContacts < 0 || numContacts > MAX_CONTACTS) return false;
    return true;
}

int ContactSettings::countByType(NodeType type) const {
    int count = 0;
    for (int i = 0; i < numContacts; i++) {
        if (contacts[i].isActive && contacts[i].type == type) {
            count++;
        }
    }
    return count;
}

int ContactSettings::getContactsByType(NodeType type, int* indices, int maxIndices) const {
    int count = 0;
    for (int i = 0; i < numContacts && count < maxIndices; i++) {
        if (contacts[i].isActive && contacts[i].type == type) {
            indices[count++] = i;
        }
    }
    return count;
}

bool ContactSettings::toggleFavorite(int idx) {
    if (idx < 0 || idx >= numContacts || !contacts[idx].isActive) {
        return false;
    }
    contacts[idx].isFavorite = !contacts[idx].isFavorite;
    return true;
}
