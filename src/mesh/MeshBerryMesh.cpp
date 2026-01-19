/**
 * MeshBerry Mesh Application Implementation
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2026 NodakMesh (nodakmesh.org)
 *
 * This file is part of MeshBerry.
 *
 * MeshBerry is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * MeshBerry is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with MeshBerry. If not, see <https://www.gnu.org/licenses/>.
 */

#include "MeshBerryMesh.h"
#include "../settings/SettingsManager.h"
#include <Utils.h>
#include <helpers/AdvertDataHelpers.h>
#include <helpers/TxtDataHelpers.h>
#include <Packet.h>
#include <string.h>

MeshBerryMesh::MeshBerryMesh(mesh::Radio& radio, mesh::RNG& rng, mesh::RTCClock& rtc,
                             SimpleMeshTables& tables, StaticPoolPacketManager& mgr)
    : mesh::Mesh(radio, _msClock, rng, rtc, mgr, tables)
    , _nodeCount(0)
    , _messageCount(0)
    , _messageHead(0)
    , _msgCallback(nullptr)
    , _nodeCallback(nullptr)
    , _loginCallback(nullptr)
    , _cliCallback(nullptr)
    , _forwardingEnabled(true)
    , _connectedRepeaterId(0)
    , _repeaterPermissions(0)
    , _repeaterConnected(false)
    , _pendingLoginAttempt(0)
    , _loginStartTime(0)
{
    strcpy(_nodeName, "MeshBerry");
    memset(_nodes, 0, sizeof(_nodes));
    memset(_messages, 0, sizeof(_messages));
    memset(_connectedRepeaterName, 0, sizeof(_connectedRepeaterName));
    memset(_repeaterSharedSecret, 0, sizeof(_repeaterSharedSecret));
}

bool MeshBerryMesh::begin() {
    Serial.println("[MESH] Starting mesh network...");

    // Initialize the mesh base class
    mesh::Mesh::begin();

    // Try to load persisted identity from SPIFFS
    if (SettingsManager::loadIdentity(self_id)) {
        Serial.println("[MESH] Loaded existing identity");
    } else {
        // Generate new identity on first boot
        Serial.println("[MESH] Generating new node identity...");
        self_id = mesh::LocalIdentity(getRNG());

        // Persist for future boots
        if (SettingsManager::saveIdentity(self_id)) {
            Serial.println("[MESH] Identity saved to SPIFFS");
        } else {
            Serial.println("[MESH] WARNING: Failed to save identity!");
        }
    }

    // Log full identity for debugging (critical for response matching)
    Serial.print("[MESH] self_id.pub_key: ");
    for (int i = 0; i < 32; i++) Serial.printf("%02X", self_id.pub_key[i]);
    Serial.println();

    // Log identity hash for debugging (first 4 bytes of public key hash)
    uint8_t hash[8];
    self_id.copyHashTo(hash);
    Serial.printf("[MESH] Node ID: %02X%02X%02X%02X (hash[0]=0x%02X for response matching)\n",
                  hash[0], hash[1], hash[2], hash[3], hash[0]);

    Serial.println("[MESH] Mesh network started");
    return true;
}

void MeshBerryMesh::loop() {
    // Process mesh events
    mesh::Mesh::loop();

    // Check for login timeout (10 seconds)
    if (_pendingLoginAttempt > 0 && (millis() - _loginStartTime > 10000)) {
        Serial.println("[MESH] Login timeout - no response from repeater");
        _pendingLoginAttempt = 0;
        if (_loginCallback) {
            _loginCallback(false, 0, "Timeout");
        }
    }
}

void MeshBerryMesh::setNodeName(const char* name) {
    strncpy(_nodeName, name, sizeof(_nodeName) - 1);
    _nodeName[sizeof(_nodeName) - 1] = '\0';
}

bool MeshBerryMesh::sendBroadcast(const char* text) {
    if (!text || strlen(text) == 0) return false;

    size_t len = strlen(text);
    if (len > MAX_MESSAGE_LENGTH - 1) {
        len = MAX_MESSAGE_LENGTH - 1;
    }

    // Create broadcast packet
    // For simplicity, use raw data packet type
    mesh::Packet* pkt = createRawData((const uint8_t*)text, len);
    if (!pkt) {
        Serial.println("[MESH] Failed to create packet");
        return false;
    }

    // Send with flood routing
    sendFlood(pkt);

    // Add to our message history
    Message msg;
    memset(&msg, 0, sizeof(msg));
    msg.senderId = 0;  // Self
    msg.timestamp = getRTCClock()->getCurrentTime();
    strncpy(msg.text, text, sizeof(msg.text) - 1);
    msg.isOutgoing = true;
    msg.delivered = false;
    addMessage(msg);

    Serial.printf("[MESH] Broadcast sent: %s\n", text);
    return true;
}

bool MeshBerryMesh::sendToChannel(int channelIdx, const char* text) {
    if (!text || strlen(text) == 0) return false;

    // Get channel settings
    ChannelSettings& chSettings = SettingsManager::getChannelSettings();
    if (channelIdx < 0 || channelIdx >= chSettings.numChannels) {
        Serial.println("[MESH] Invalid channel index");
        return false;
    }

    const ChannelEntry& entry = chSettings.channels[channelIdx];

    // Debug: Print channel info for troubleshooting
    Serial.printf("[MESH] Sending on channel '%s' (idx=%d, keyLen=%d, storedHash=0x%02X)\n",
                  entry.name, channelIdx, entry.secretLen, entry.hash);
    Serial.print("[MESH] Secret (hex): ");
    for (int i = 0; i < entry.secretLen; i++) {
        Serial.printf("%02X", entry.secret[i]);
    }
    Serial.println();

    // Verify hash matches MeshCore calculation
    uint8_t verifyHash;
    mesh::Utils::sha256(&verifyHash, 1, entry.secret, entry.secretLen);
    Serial.printf("[MESH] MeshCore-calculated hash: 0x%02X (match=%s)\n",
                  verifyHash, (verifyHash == entry.hash) ? "YES" : "NO");

    // Build MeshCore GroupChannel from our ChannelEntry
    mesh::GroupChannel channel;
    channel.hash[0] = verifyHash;  // Use MeshCore-verified hash
    memcpy(channel.secret, entry.secret, sizeof(channel.secret));

    // Build MeshCore-compatible message format:
    // [4 bytes timestamp][1 byte flags][sender: message]
    uint8_t payload[5 + 32 + MAX_MESSAGE_LENGTH];

    // Timestamp (4 bytes)
    uint32_t timestamp = getRTCClock()->getCurrentTime();
    memcpy(payload, &timestamp, 4);

    // Flags (1 byte) - 0 = plain text
    payload[4] = 0;

    // Format "NodeName: message"
    int prefixLen = snprintf((char*)&payload[5], 32, "%s: ", _nodeName);

    size_t textLen = strlen(text);
    size_t maxText = MAX_MESSAGE_LENGTH - prefixLen - 1;
    if (textLen > maxText) {
        textLen = maxText;
    }

    memcpy(&payload[5 + prefixLen], text, textLen);
    payload[5 + prefixLen + textLen] = '\0';

    size_t totalLen = 5 + prefixLen + textLen;

    // Create encrypted group datagram using MeshCore's API
    // PAYLOAD_TYPE_GRP_TXT = 0x05 for group text messages
    mesh::Packet* pkt = createGroupDatagram(0x05, channel, payload, totalLen);
    if (!pkt) {
        Serial.println("[MESH] Failed to create group datagram");
        return false;
    }

    // Send with flood routing
    sendFlood(pkt);

    Serial.printf("[MESH] Sent to channel %s: %s\n", entry.name, text);
    return true;
}

void MeshBerryMesh::sendAdvertisement() {
    // Build advertisement using MeshCore's AdvertDataBuilder
    uint8_t app_data[MAX_ADVERT_DATA_SIZE];
    AdvertDataBuilder builder(ADV_TYPE_CHAT, _nodeName);
    uint8_t app_data_len = builder.encodeTo(app_data);

    // Create and send advertisement packet
    mesh::Packet* pkt = createAdvert(self_id, app_data, app_data_len);
    if (pkt) {
        sendFlood(pkt);
        Serial.printf("[MESH] Advertisement sent: %s (type=CHAT)\n", _nodeName);
    }
}

bool MeshBerryMesh::getNodeInfo(int index, NodeInfo& info) const {
    if (index < 0 || index >= _nodeCount) return false;
    info = _nodes[index];
    return true;
}

bool MeshBerryMesh::getMessage(int index, Message& msg) const {
    if (index < 0 || index >= _messageCount) return false;

    // Calculate actual index in circular buffer
    int actualIndex = (_messageHead - _messageCount + index + MAX_MESSAGES) % MAX_MESSAGES;
    msg = _messages[actualIndex];
    return true;
}

void MeshBerryMesh::addMessage(const Message& msg) {
    _messages[_messageHead] = msg;
    _messageHead = (_messageHead + 1) % MAX_MESSAGES;

    if (_messageCount < MAX_MESSAGES) {
        _messageCount++;
    }

    if (_msgCallback) {
        _msgCallback(msg);
    }
}

void MeshBerryMesh::updateNode(const NodeInfo& node) {
    // Check if node already exists
    for (int i = 0; i < _nodeCount; i++) {
        if (_nodes[i].id == node.id) {
            _nodes[i] = node;
            if (_nodeCallback) {
                _nodeCallback(node);
            }
            return;
        }
    }

    // Add new node
    if (_nodeCount < MAX_TRACKED_NODES) {
        _nodes[_nodeCount++] = node;
        if (_nodeCallback) {
            _nodeCallback(node);
        }
    }
}

void MeshBerryMesh::onAdvertRecv(mesh::Packet* packet, const mesh::Identity& id,
                                  uint32_t timestamp, const uint8_t* app_data, size_t app_data_len) {
    Serial.println("[MESH] Advertisement received");

    // Sync our RTC clock from advertisement timestamp if significantly behind
    // This ensures our timestamps are valid for login requests
    uint32_t currentTime = getRTCClock()->getCurrentTime();
    if (timestamp > currentTime + 60) {  // Their time is >60s ahead of ours
        Serial.printf("[MESH] Syncing RTC from advertisement: %u -> %u\n", currentTime, timestamp);
        getRTCClock()->setCurrentTime(timestamp);
    }

    // Debug: Print raw app_data
    Serial.printf("[MESH] app_data_len=%d, raw bytes: ", app_data_len);
    for (size_t i = 0; i < app_data_len && i < 16; i++) {
        Serial.printf("%02X ", app_data[i]);
    }
    Serial.println();

    NodeInfo node;
    memset(&node, 0, sizeof(node));

    // Extract node ID from identity hash
    uint8_t hash[MAX_HASH_SIZE];
    id.copyHashTo(hash);
    memcpy(&node.id, hash, sizeof(node.id));

    // Parse advertisement data using MeshCore's AdvertDataParser
    if (app_data && app_data_len > 0) {
        AdvertDataParser parser(app_data, app_data_len);

        Serial.printf("[MESH] Parser: isValid=%d, type=%d, hasName=%d\n",
                      parser.isValid(), parser.getType(), parser.hasName());

        if (parser.isValid()) {
            // Extract node type (lower 4 bits)
            node.type = (NodeType)parser.getType();

            // Extract name if present
            if (parser.hasName()) {
                strncpy(node.name, parser.getName(), sizeof(node.name) - 1);
                node.name[sizeof(node.name) - 1] = '\0';
            } else {
                strcpy(node.name, "Unknown");
            }

            // Extract GPS location if present
            if (parser.hasLatLon()) {
                node.hasLocation = true;
                node.latitude = parser.getLat();
                node.longitude = parser.getLon();
            }
        } else {
            // Fallback: Extract type directly from first byte (lower 4 bits)
            // even if parser validation fails
            node.type = (NodeType)(app_data[0] & 0x0F);

            // Try to extract name from remaining bytes (skip first byte)
            if (app_data_len > 1) {
                size_t nameLen = app_data_len - 1;
                if (nameLen > sizeof(node.name) - 1) {
                    nameLen = sizeof(node.name) - 1;
                }
                memcpy(node.name, app_data + 1, nameLen);
                node.name[nameLen] = '\0';
            } else {
                strcpy(node.name, "Unknown");
            }

            Serial.printf("[MESH] Fallback parsing: type=%d, name='%s'\n", node.type, node.name);
        }
    } else {
        strcpy(node.name, "Unknown");
        node.type = NODE_TYPE_UNKNOWN;
    }

    node.rssi = (int16_t)packet->_snr;
    node.snr = packet->getSNR();
    node.lastHeard = timestamp;

    updateNode(node);

    // Copy pubKey to local buffer BEFORE any other processing
    // This ensures we have a stable copy that won't be affected by any callbacks
    uint8_t pubKeyCopy[32];
    memcpy(pubKeyCopy, id.pub_key, 32);

    // Debug: trace source pubKey from MeshCore Identity BEFORE any processing
    Serial.print("[MESH] Source id.pub_key: ");
    for (int i = 0; i < 32; i++) Serial.printf("%02X", pubKeyCopy[i]);
    Serial.println();

    // Only add contact if we have a valid public key (required for repeater login)
    bool hasPubKey = false;
    for (int i = 0; i < 32; i++) {
        if (pubKeyCopy[i] != 0) {
            hasPubKey = true;
            break;
        }
    }

    if (hasPubKey) {
        ContactSettings& contacts = SettingsManager::getContactSettings();
        int idx = contacts.addOrUpdateContact(node, pubKeyCopy);
        SettingsManager::saveContacts();

        // Debug: verify pubKey was saved correctly
        if (idx >= 0) {
            const ContactEntry* c = contacts.getContact(idx);
            if (c) {
                // Compare stored vs original
                bool match = (memcmp(c->pubKey, pubKeyCopy, 32) == 0);
                Serial.printf("[MESH] Saved pubKey for %s: %02X%02X%02X%02X... match=%d\n",
                              c->name, c->pubKey[0], c->pubKey[1], c->pubKey[2], c->pubKey[3], match);
                if (!match) {
                    Serial.println("[MESH] ERROR: pubKey mismatch after save!");
                    Serial.print("[MESH] Expected: ");
                    for (int i = 0; i < 8; i++) Serial.printf("%02X", pubKeyCopy[i]);
                    Serial.println();
                    Serial.print("[MESH] Got:      ");
                    for (int i = 0; i < 8; i++) Serial.printf("%02X", c->pubKey[i]);
                    Serial.println();
                }
            }
        }
    } else {
        Serial.printf("[MESH] Skipping contact %s - no pubKey in advertisement\n", node.name);
    }

    // Log with type indicator
    const char* typeStr = "?";
    switch (node.type) {
        case NODE_TYPE_CHAT: typeStr = "C"; break;
        case NODE_TYPE_REPEATER: typeStr = "R"; break;
        case NODE_TYPE_ROOM: typeStr = "S"; break;
        case NODE_TYPE_SENSOR: typeStr = "X"; break;
        default: typeStr = "?"; break;
    }
    Serial.printf("[MESH] Node [%s]: %s (RSSI: %d, SNR: %.1f)\n",
                  typeStr, node.name, node.rssi, node.snr);
}

void MeshBerryMesh::onAnonDataRecv(mesh::Packet* packet, const uint8_t* secret,
                                    const mesh::Identity& sender, uint8_t* data, size_t len) {
    Serial.printf("[MESH] >>> onAnonDataRecv ENTRY: payloadType=%02X, len=%d\n",
                  packet->getPayloadType(), len);
    Serial.print("[MESH] Data (first 16): ");
    for (size_t i = 0; i < 16 && i < len; i++) Serial.printf("%02X", data[i]);
    Serial.println();
    Serial.print("[MESH] Sender pubKey: ");
    for (int i = 0; i < 8; i++) Serial.printf("%02X", sender.pub_key[i]);
    Serial.println("...");
    Serial.printf("[MESH] Anonymous data received (type=%02X, len=%d)\n",
                  packet->getPayloadType(), len);

    // Check if this is a login response (PAYLOAD_TYPE_RESPONSE = 0x01)
    if (packet->getPayloadType() == PAYLOAD_TYPE_RESPONSE && _pendingLoginAttempt > 0 && len >= 8) {
        // Response format: [4-byte timestamp][1-byte type][1-byte keep-alive][1-byte isAdmin][1-byte permissions]
        uint8_t respType = data[4];

        if (respType == 0) {  // RESP_SERVER_LOGIN_OK
            _repeaterConnected = true;
            _repeaterPermissions = data[7];
            _pendingLoginAttempt = 0;

            // Find repeater name from contacts
            ContactSettings& contacts = SettingsManager::getContactSettings();
            int idx = contacts.findContact(_connectedRepeaterId);
            if (idx >= 0) {
                const ContactEntry* c = contacts.getContact(idx);
                if (c) {
                    strncpy(_connectedRepeaterName, c->name, sizeof(_connectedRepeaterName) - 1);
                }
            } else {
                snprintf(_connectedRepeaterName, sizeof(_connectedRepeaterName), "Repeater%04X",
                         (uint16_t)(_connectedRepeaterId & 0xFFFF));
            }

            bool isAdmin = data[6] != 0;
            Serial.printf("[MESH] Login successful! Connected to %s (admin=%d, perms=%d)\n",
                          _connectedRepeaterName, isAdmin, _repeaterPermissions);

            if (_loginCallback) {
                _loginCallback(true, _repeaterPermissions, _connectedRepeaterName);
            }
            return;
        } else {
            Serial.printf("[MESH] Login failed: response type %d\n", respType);
            _pendingLoginAttempt = 0;

            if (_loginCallback) {
                _loginCallback(false, 0, "");
            }
            return;
        }
    }

    // Handle other anonymous data as text message
    Message msg;
    memset(&msg, 0, sizeof(msg));

    uint8_t hash[MAX_HASH_SIZE];
    sender.copyHashTo(hash);
    memcpy(&msg.senderId, hash, sizeof(msg.senderId));

    msg.timestamp = getRTCClock()->getCurrentTime();

    if (len > 0 && len < sizeof(msg.text)) {
        memcpy(msg.text, data, len);
        msg.text[len] = '\0';
    }

    msg.isOutgoing = false;
    msg.delivered = true;

    addMessage(msg);

    Serial.printf("[MESH] Message from %08X: %s\n", msg.senderId, msg.text);
}

void MeshBerryMesh::onAckRecv(mesh::Packet* packet, uint32_t ack_crc) {
    Serial.printf("[MESH] ACK received: %08X\n", ack_crc);

    // Mark corresponding message as delivered
    for (int i = 0; i < _messageCount; i++) {
        int idx = (_messageHead - 1 - i + MAX_MESSAGES) % MAX_MESSAGES;
        if (_messages[idx].isOutgoing && !_messages[idx].delivered) {
            _messages[idx].delivered = true;
            break;
        }
    }
}

int MeshBerryMesh::searchChannelsByHash(const uint8_t* hash, mesh::GroupChannel channels[], int max_matches) {
    ChannelSettings& chSettings = SettingsManager::getChannelSettings();
    int count = 0;

    for (int i = 0; i < chSettings.numChannels && count < max_matches; i++) {
        const ChannelEntry& entry = chSettings.channels[i];
        if (entry.isActive && entry.hash == hash[0]) {
            channels[count].hash[0] = entry.hash;
            memcpy(channels[count].secret, entry.secret, sizeof(channels[count].secret));
            count++;
        }
    }

    return count;
}

void MeshBerryMesh::onGroupDataRecv(mesh::Packet* packet, uint8_t type, const mesh::GroupChannel& channel, uint8_t* data, size_t len) {
    Serial.printf("[MESH] Channel message received (type=%02X, len=%d)\n", type, len);

    // PAYLOAD_TYPE_GRP_TXT with MeshCore format: [4-byte timestamp][1-byte flags][text]
    if (type == 0x05 && len > 5 && (data[4] >> 2) == 0) {
        Message msg;
        memset(&msg, 0, sizeof(msg));

        // Extract timestamp from offset 0-3
        memcpy(&msg.timestamp, data, 4);

        msg.senderId = 0;  // Unknown sender for channel messages

        // Message text is at offset 5 (includes "SenderName: " prefix)
        size_t textLen = len - 5;
        if (textLen > sizeof(msg.text) - 1) {
            textLen = sizeof(msg.text) - 1;
        }
        memcpy(msg.text, &data[5], textLen);
        msg.text[textLen] = '\0';

        msg.isOutgoing = false;
        msg.delivered = true;

        addMessage(msg);

        Serial.printf("[MESH] Channel text: %s\n", msg.text);
    }
}

bool MeshBerryMesh::allowPacketForward(const mesh::Packet* packet) {
    // Check if forwarding is enabled (can be toggled via CLI)
    return _forwardingEnabled;
}

// =============================================================================
// REPEATER MANAGEMENT
// =============================================================================

bool MeshBerryMesh::sendRepeaterLogin(uint32_t repeaterId, const uint8_t* repeaterPubKey, const char* password) {
    if (!password) return false;

    // Validate pubKey before proceeding
    bool hasValidPubKey = false;
    for (int i = 0; i < 32; i++) {
        if (repeaterPubKey[i] != 0) {
            hasValidPubKey = true;
            break;
        }
    }
    if (!hasValidPubKey) {
        Serial.println("[MESH] ERROR: Empty pubKey - need fresh advertisement");
        if (_loginCallback) {
            _loginCallback(false, 0, "No pubKey");
        }
        return false;
    }

    // Store repeater info for later
    _connectedRepeaterId = repeaterId;
    _repeaterIdentity = mesh::Identity(repeaterPubKey);

    // DEBUG: Log the pubKey being used
    Serial.printf("[MESH] Login pubKey: %02X%02X%02X%02X%02X%02X%02X%02X...\n",
                  repeaterPubKey[0], repeaterPubKey[1], repeaterPubKey[2], repeaterPubKey[3],
                  repeaterPubKey[4], repeaterPubKey[5], repeaterPubKey[6], repeaterPubKey[7]);

    // DEBUG: Log the hash that will be matched in responses
    uint8_t expectedHash[8];
    _repeaterIdentity.copyHashTo(expectedHash);
    Serial.printf("[MESH] Expected response from hash: %02X\n", expectedHash[0]);

    // Calculate shared secret using ECDH
    self_id.calcSharedSecret(_repeaterSharedSecret, _repeaterIdentity);

    // DEBUG: Log our own public key (what we send in the packet)
    Serial.print("[MESH] Our pubKey (sent in packet): ");
    for (int i = 0; i < 32; i++) Serial.printf("%02X", self_id.pub_key[i]);
    Serial.println();

    // DEBUG: Log shared secret
    Serial.printf("[MESH] Shared secret: %02X%02X%02X%02X...\n",
                  _repeaterSharedSecret[0], _repeaterSharedSecret[1],
                  _repeaterSharedSecret[2], _repeaterSharedSecret[3]);

    Serial.printf("[MESH] Sending login to repeater ID %08X...\n", repeaterId);

    // Build login packet: [4-byte timestamp][password string] - NO null terminator
    // MeshCore repeater adds its own null terminator when receiving
    uint8_t payload[24];
    uint32_t timestamp = getRTCClock()->getCurrentTimeUnique();
    memcpy(payload, &timestamp, 4);

    size_t pwdLen = strlen(password);
    if (pwdLen > 15) pwdLen = 15;  // Max 15 chars
    memcpy(&payload[4], password, pwdLen);
    // NO null terminator - repeater handles string termination

    // Enhanced debug output for troubleshooting
    Serial.println("[MESH] === LOGIN DEBUG ===");
    Serial.print("[MESH] Full pubKey: ");
    for (int i = 0; i < 32; i++) Serial.printf("%02X", repeaterPubKey[i]);
    Serial.println();
    Serial.print("[MESH] Full secret: ");
    for (int i = 0; i < 32; i++) Serial.printf("%02X", _repeaterSharedSecret[i]);
    Serial.println();
    Serial.printf("[MESH] Password='%s' len=%zu\n", password, pwdLen);
    Serial.print("[MESH] Payload hex: ");
    for (size_t i = 0; i < 4 + pwdLen; i++) Serial.printf("%02X", payload[i]);
    Serial.println();
    Serial.println("[MESH] === END DEBUG ===");

    // Create anonymous datagram (login uses PAYLOAD_TYPE_ANON_REQ)
    Serial.printf("[MESH] Creating anon datagram with type=0x%02X (ANON_REQ should be 0x07)\n", PAYLOAD_TYPE_ANON_REQ);
    mesh::Packet* pkt = createAnonDatagram(PAYLOAD_TYPE_ANON_REQ, self_id, _repeaterIdentity,
                                            _repeaterSharedSecret, payload, 4 + pwdLen);
    if (!pkt) {
        Serial.println("[MESH] Failed to create login packet");
        return false;
    }

    // Debug: show packet details
    Serial.printf("[MESH] Packet created: header=%02X, payload_len=%d\n", pkt->header, pkt->payload_len);
    Serial.print("[MESH] Packet payload (first 16): ");
    for (int i = 0; i < 16 && i < pkt->payload_len; i++) Serial.printf("%02X", pkt->payload[i]);
    Serial.println();

    // Send with flood routing
    sendFlood(pkt);
    _pendingLoginAttempt = 1;
    _loginStartTime = millis();

    Serial.println("[MESH] Login request sent via flood");
    return true;
}

bool MeshBerryMesh::sendRepeaterCommand(const char* command) {
    if (!_repeaterConnected) {
        Serial.println("[MESH] Not connected to a repeater");
        return false;
    }

    if (!command || strlen(command) == 0) return false;

    Serial.printf("[MESH] Sending command to %s: %s\n", _connectedRepeaterName, command);

    // Build command packet: [4-byte timestamp][1-byte flags][command string]
    uint8_t payload[MAX_MESSAGE_LENGTH + 6];
    uint32_t timestamp = getRTCClock()->getCurrentTimeUnique();
    memcpy(payload, &timestamp, 4);
    payload[4] = (TXT_TYPE_CLI_DATA << 2);  // CLI command flag

    size_t cmdLen = strlen(command);
    if (cmdLen > MAX_MESSAGE_LENGTH - 1) cmdLen = MAX_MESSAGE_LENGTH - 1;
    memcpy(&payload[5], command, cmdLen);
    payload[5 + cmdLen] = '\0';

    // Create encrypted datagram to connected repeater
    mesh::Packet* pkt = createDatagram(PAYLOAD_TYPE_TXT_MSG, _repeaterIdentity,
                                        _repeaterSharedSecret, payload, 5 + cmdLen + 1);
    if (!pkt) {
        Serial.println("[MESH] Failed to create command packet");
        return false;
    }

    // Send with flood routing (TODO: use direct routing if we have a path)
    sendFlood(pkt);

    Serial.println("[MESH] Command sent");
    return true;
}

void MeshBerryMesh::disconnectRepeater() {
    _repeaterConnected = false;
    _connectedRepeaterId = 0;
    _repeaterPermissions = 0;
    memset(_connectedRepeaterName, 0, sizeof(_connectedRepeaterName));
    memset(_repeaterSharedSecret, 0, sizeof(_repeaterSharedSecret));
    _pendingLoginAttempt = 0;

    Serial.println("[MESH] Disconnected from repeater");
}

int MeshBerryMesh::searchPeersByHash(const uint8_t* hash) {
    // DEBUG: Log the search
    Serial.printf("[MESH] searchPeersByHash: looking for hash=%02X, pending=%d, connected=%d\n",
                  hash[0], _pendingLoginAttempt, _repeaterConnected);

    // Check if hash matches our connected repeater OR pending login
    // We need to match during login attempt so we can decrypt the response
    if (_repeaterConnected || _pendingLoginAttempt > 0) {
        uint8_t expectedHash[8];
        _repeaterIdentity.copyHashTo(expectedHash);
        bool matches = _repeaterIdentity.isHashMatch(hash);
        Serial.printf("[MESH] searchPeersByHash: repeater hash=%02X, match=%d\n",
                      expectedHash[0], matches);

        if (matches) {
            Serial.println("[MESH] searchPeersByHash: FOUND repeater match!");
            return 1;  // Found one matching peer (the repeater)
        }
    }

    Serial.println("[MESH] searchPeersByHash: no match");
    return 0;
}

void MeshBerryMesh::getPeerSharedSecret(uint8_t* dest_secret, int peer_idx) {
    Serial.printf("[MESH] getPeerSharedSecret: peer_idx=%d, pending=%d, connected=%d\n",
                  peer_idx, _pendingLoginAttempt, _repeaterConnected);

    // Return shared secret for connected repeater OR pending login
    if (peer_idx == 0 && (_repeaterConnected || _pendingLoginAttempt > 0)) {
        memcpy(dest_secret, _repeaterSharedSecret, PUB_KEY_SIZE);
        Serial.printf("[MESH] getPeerSharedSecret: returning secret %02X%02X%02X%02X...\n",
                      _repeaterSharedSecret[0], _repeaterSharedSecret[1],
                      _repeaterSharedSecret[2], _repeaterSharedSecret[3]);
    } else {
        Serial.println("[MESH] getPeerSharedSecret: NOT returning secret (conditions not met)");
    }
}

bool MeshBerryMesh::onPeerPathRecv(mesh::Packet* packet, int sender_idx, const uint8_t* secret,
                                    uint8_t* path, uint8_t path_len, uint8_t extra_type,
                                    uint8_t* extra, uint8_t extra_len) {
    Serial.printf("[MESH] >>> onPeerPathRecv ENTRY: sender_idx=%d, path_len=%d, extra_type=%02X, extra_len=%d\n",
                  sender_idx, path_len, extra_type, extra_len);

    // Check if this is a login response embedded in PATH packet
    if (extra_type == PAYLOAD_TYPE_RESPONSE && _pendingLoginAttempt > 0 && extra_len >= 8) {
        Serial.println("[MESH] Found login response embedded in PATH packet!");

        // Response format: [4-byte timestamp][1-byte type][1-byte keep-alive][1-byte isAdmin][1-byte permissions]
        uint8_t respType = extra[4];

        Serial.printf("[MESH] Login response: type=%d, data[5]=%d, data[6]=%d, data[7]=%d\n",
                      respType, extra[5], extra[6], extra[7]);

        if (respType == 0) {  // RESP_SERVER_LOGIN_OK
            _repeaterConnected = true;
            _repeaterPermissions = extra[7];
            _pendingLoginAttempt = 0;

            // Find repeater name from contacts
            ContactSettings& contacts = SettingsManager::getContactSettings();
            int idx = contacts.findContact(_connectedRepeaterId);
            if (idx >= 0) {
                const ContactEntry* c = contacts.getContact(idx);
                if (c) {
                    strncpy(_connectedRepeaterName, c->name, sizeof(_connectedRepeaterName) - 1);
                }
            } else {
                snprintf(_connectedRepeaterName, sizeof(_connectedRepeaterName), "Repeater%04X",
                         (uint16_t)(_connectedRepeaterId & 0xFFFF));
            }

            bool isAdmin = extra[6] != 0;
            Serial.printf("[MESH] Login successful! Connected to %s (admin=%d, perms=%d)\n",
                          _connectedRepeaterName, isAdmin, _repeaterPermissions);

            if (_loginCallback) {
                _loginCallback(true, _repeaterPermissions, _connectedRepeaterName);
            }
            return true;
        } else {
            Serial.printf("[MESH] Login failed: response type %d\n", respType);
            _pendingLoginAttempt = 0;

            if (_loginCallback) {
                _loginCallback(false, 0, "");
            }
            return true;
        }
    }

    // For other PATH packets, just log and continue
    return true;  // Return true to send reciprocal path if needed
}

void MeshBerryMesh::onPeerDataRecv(mesh::Packet* packet, uint8_t type, int sender_idx,
                                    const uint8_t* secret, uint8_t* data, size_t len) {
    Serial.printf("[MESH] >>> onPeerDataRecv ENTRY: type=%02X, sender_idx=%d, len=%d\n", type, sender_idx, len);
    Serial.print("[MESH] Data (first 16): ");
    for (size_t i = 0; i < 16 && i < len; i++) Serial.printf("%02X", data[i]);
    Serial.println();
    Serial.printf("[MESH] Peer data received (type=%02X, len=%d)\n", type, len);

    // Handle login response (PAYLOAD_TYPE_RESPONSE = 0x01)
    if (type == PAYLOAD_TYPE_RESPONSE && _pendingLoginAttempt > 0 && len >= 8) {
        // Response format: [4-byte timestamp][1-byte type][1-byte keep-alive][1-byte isAdmin][1-byte permissions]
        uint8_t respType = data[4];

        Serial.printf("[MESH] Login response: type=%d, data[5]=%d, data[6]=%d, data[7]=%d\n",
                      respType, data[5], data[6], data[7]);

        if (respType == 0) {  // RESP_SERVER_LOGIN_OK
            _repeaterConnected = true;
            _repeaterPermissions = data[7];
            _pendingLoginAttempt = 0;

            // Find repeater name from contacts
            ContactSettings& contacts = SettingsManager::getContactSettings();
            int idx = contacts.findContact(_connectedRepeaterId);
            if (idx >= 0) {
                const ContactEntry* c = contacts.getContact(idx);
                if (c) {
                    strncpy(_connectedRepeaterName, c->name, sizeof(_connectedRepeaterName) - 1);
                }
            } else {
                snprintf(_connectedRepeaterName, sizeof(_connectedRepeaterName), "Repeater%04X",
                         (uint16_t)(_connectedRepeaterId & 0xFFFF));
            }

            bool isAdmin = data[6] != 0;
            Serial.printf("[MESH] Login successful! Connected to %s (admin=%d, perms=%d)\n",
                          _connectedRepeaterName, isAdmin, _repeaterPermissions);

            if (_loginCallback) {
                _loginCallback(true, _repeaterPermissions, _connectedRepeaterName);
            }
            return;
        } else {
            Serial.printf("[MESH] Login failed: response type %d\n", respType);
            _pendingLoginAttempt = 0;

            if (_loginCallback) {
                _loginCallback(false, 0, "");
            }
            return;
        }
    }

    // Handle text messages from connected repeater (CLI responses)
    if (type == PAYLOAD_TYPE_TXT_MSG && len > 5 && _repeaterConnected) {
        uint8_t flags = data[4] >> 2;

        if (flags == TXT_TYPE_CLI_DATA || flags == TXT_TYPE_PLAIN) {
            // Extract response text (skip timestamp and flags)
            data[len] = '\0';  // Ensure null termination
            char* responseText = (char*)&data[5];

            Serial.printf("[MESH] CLI response from %s: %s\n", _connectedRepeaterName, responseText);

            if (_cliCallback) {
                _cliCallback(responseText);
            }
        }
    }
}
