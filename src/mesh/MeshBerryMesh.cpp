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
    , _channelMsgCallback(nullptr)
    , _dmCallback(nullptr)
    , _deliveryCallback(nullptr)
    , _repeatCallback(nullptr)
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
    memset(_dmPeers, 0, sizeof(_dmPeers));
    memset(_pendingDMs, 0, sizeof(_pendingDMs));
    memset(_channelStats, 0, sizeof(_channelStats));
    _lastMatchedDMPeer = -1;
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

    // Check for DM delivery timeouts
    checkPendingTimeouts();
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

    // Track this message for repeat counting (use text without sender prefix)
    trackSentChannelMessage(channelIdx, text);

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

    // Auto-sync RTC from advertisement timestamp - only if not already set
    // Cast to ESP32RTCClock to access trySyncFromPeer()
    ESP32RTCClock* rtc = static_cast<ESP32RTCClock*>(getRTCClock());
    if (rtc->trySyncFromPeer(timestamp)) {
        Serial.printf("[MESH] Auto-synced RTC from peer advert: %u\n", timestamp);
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
        // DEBUG: Show Identity hash calculation for this advertisement
        mesh::Identity tempIdentity(pubKeyCopy);
        uint8_t identityHash[8];
        tempIdentity.copyHashTo(identityHash);

        Serial.printf("[MESH] Advertisement Identity hash: %02X%02X%02X%02X%02X%02X%02X%02X\n",
                      identityHash[0], identityHash[1], identityHash[2], identityHash[3],
                      identityHash[4], identityHash[5], identityHash[6], identityHash[7]);
        Serial.printf("[MESH] Advertisement pubKey[0..7]:  %02X%02X%02X%02X%02X%02X%02X%02X\n",
                      pubKeyCopy[0], pubKeyCopy[1], pubKeyCopy[2], pubKeyCopy[3],
                      pubKeyCopy[4], pubKeyCopy[5], pubKeyCopy[6], pubKeyCopy[7]);

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

mesh::DispatcherAction MeshBerryMesh::onRecvPacket(mesh::Packet* pkt) {
    // INTERCEPT: Handle DIRECT-routed ACKs that MeshCore would skip
    // MeshCore's Mesh.cpp (lines 80-90) returns ACTION_RELEASE without calling onAckRecv()
    // for DIRECT ACKs, so we extract the CRC and call it manually

    if (pkt->isRouteDirect() &&
        pkt->path_len >= PATH_HASH_SIZE &&
        self_id.isHashMatch(pkt->path)) {

        // Handle regular DIRECT ACK packets
        if (pkt->getPayloadType() == PAYLOAD_TYPE_ACK) {
            if (pkt->payload_len >= 4) {
                uint32_t ack_crc;
                memcpy(&ack_crc, &pkt->payload, 4);

                // Call onAckRecv() manually (MeshCore won't call it)
                onAckRecv(pkt, ack_crc);

                Serial.printf("[MESH] DIRECT ACK intercepted: %08X\n", ack_crc);
            }
        }

        // Handle MULTIPART ACK packets
        else if (pkt->getPayloadType() == PAYLOAD_TYPE_MULTIPART) {
            if (pkt->payload_len >= 5) {
                uint8_t remaining = pkt->payload[0] >> 4;
                uint8_t type = pkt->payload[0] & 0x0F;

                if (type == PAYLOAD_TYPE_ACK) {
                    uint32_t ack_crc;
                    memcpy(&ack_crc, &pkt->payload[1], 4);

                    onAckRecv(pkt, ack_crc);

                    Serial.printf("[MESH] DIRECT MULTIPART ACK intercepted: %08X (remaining=%d)\n",
                                 ack_crc, remaining);
                }
            }
        }
    }

    // Call parent implementation for normal processing (forwarding, etc.)
    return mesh::Mesh::onRecvPacket(pkt);
}

void MeshBerryMesh::onAckRecv(mesh::Packet* packet, uint32_t ack_crc) {
    // Show routing type to confirm if ACKs come via FLOOD or DIRECT
    const char* routingType = packet->isRouteDirect() ? "DIRECT" : "FLOOD";
    Serial.printf("[MESH] ACK received: %08X (routing=%s, path_len=%d)\n",
                  ack_crc, routingType, packet->path_len);

    // Check if this ACK matches a pending DM
    for (int i = 0; i < MAX_PENDING_DMS; i++) {
        if (_pendingDMs[i].active && _pendingDMs[i].ack_crc == ack_crc) {
            Serial.printf("[DM] Delivery confirmed for contact %08X (attempt %d)\n",
                          _pendingDMs[i].contactId, _pendingDMs[i].attempts);

            // Learn path from any ACK that has path info (flood or direct)
            if (packet->path_len > 0) {
                learnPath(_pendingDMs[i].contactId, packet->path, packet->path_len);
                Serial.printf("[ROUTE] Learned path from %s ACK: %d hops\n",
                              packet->isRouteFlood() ? "FLOOD" : "DIRECT",
                              packet->path_len);
            }

            // Mark as inactive and notify UI
            uint32_t contactId = _pendingDMs[i].contactId;
            uint8_t attempts = _pendingDMs[i].attempts;
            _pendingDMs[i].active = false;

            if (_deliveryCallback) {
                _deliveryCallback(contactId, ack_crc, true, attempts);
            }
            return;
        }
    }

    // Fallback: Mark corresponding message as delivered (legacy behavior)
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

bool MeshBerryMesh::filterRecvFloodPacket(mesh::Packet* packet) {
    // This is called BEFORE the duplicate filter (hasSeen), allowing us to
    // detect our own repeated messages before they get filtered out

    // Only check group text messages (channel messages)
    if (packet->getPayloadType() != 0x05) {
        return false;  // Not a channel message, don't filter
    }

    // Try to decrypt with each of our channels to see if it's ours
    uint8_t channel_hash = packet->payload[0];
    ChannelSettings& chSettings = SettingsManager::getChannelSettings();

    for (int ch = 0; ch < chSettings.numChannels; ch++) {
        if (!chSettings.channels[ch].isActive) continue;
        if (chSettings.channels[ch].hash != channel_hash) continue;

        // Build GroupChannel for decryption
        mesh::GroupChannel channel;
        channel.hash[0] = channel_hash;
        memcpy(channel.secret, chSettings.channels[ch].secret, sizeof(channel.secret));

        // Try to decrypt
        uint8_t data[MAX_PACKET_PAYLOAD];
        int len = mesh::Utils::MACThenDecrypt(
            channel.secret,
            data,
            &packet->payload[1],  // Skip channel hash byte
            packet->payload_len - 1
        );

        if (len > 5) {  // Valid decryption: 4-byte timestamp + 1-byte flags + text
            // Extract text (skip timestamp and flags)
            char textBuf[MAX_MESSAGE_LENGTH];
            size_t textLen = len - 5;
            if (textLen > sizeof(textBuf) - 1) textLen = sizeof(textBuf) - 1;
            memcpy(textBuf, &data[5], textLen);
            textBuf[textLen] = '\0';

            // Parse sender name from "SenderName: message"
            char senderName[32] = "";
            const char* msgText = textBuf;
            const char* colonPos = strstr(textBuf, ": ");
            if (colonPos && (colonPos - textBuf) < 31) {
                size_t nameLen = colonPos - textBuf;
                memcpy(senderName, textBuf, nameLen);
                senderName[nameLen] = '\0';
                msgText = colonPos + 2;
            }

            // Check if this is our own message being repeated
            if (strcmp(senderName, _nodeName) == 0) {
                // This is our message! Check against tracked messages
                uint32_t receivedHash = hashChannelMessage(ch, msgText);
                uint32_t now = millis();

                for (int i = 0; i < MAX_CHANNEL_STATS; i++) {
                    if (_channelStats[i].active &&
                        _channelStats[i].channelIdx == ch &&
                        _channelStats[i].contentHash == receivedHash) {

                        if ((now - _channelStats[i].sentAt) <= CHANNEL_STATS_EXPIRY_MS) {
                            _channelStats[i].repeatCount++;
                            Serial.printf("[REPEAT] Heard our message repeated! ch=%d, hash=%08X, count=%d\n",
                                          ch, receivedHash, _channelStats[i].repeatCount);

                            if (_repeatCallback) {
                                _repeatCallback(ch, receivedHash, _channelStats[i].repeatCount);
                            }
                        }
                        break;
                    }
                }
            }
            break;  // Found matching channel, stop searching
        }
    }

    return false;  // Never filter - let hasSeen() handle duplicate filtering
}

void MeshBerryMesh::onGroupDataRecv(mesh::Packet* packet, uint8_t type, const mesh::GroupChannel& channel, uint8_t* data, size_t len) {
    Serial.printf("[MESH] Channel message received (type=%02X, len=%d)\n", type, len);

    // PAYLOAD_TYPE_GRP_TXT with MeshCore format: [4-byte timestamp][1-byte flags][text]
    if (type == 0x05 && len > 5 && (data[4] >> 2) == 0) {
        // Extract timestamp from offset 0-3
        uint32_t timestamp;
        memcpy(&timestamp, data, 4);

        // Auto-sync RTC from message timestamp - only if not already set
        ESP32RTCClock* rtc = static_cast<ESP32RTCClock*>(getRTCClock());
        if (rtc->trySyncFromPeer(timestamp)) {
            Serial.printf("[MESH] Auto-synced RTC from channel message: %u\n", timestamp);
        }

        // Message text is at offset 5 (includes "SenderName: " prefix)
        size_t textLen = len - 5;
        char textBuf[MAX_MESSAGE_LENGTH];
        if (textLen > sizeof(textBuf) - 1) {
            textLen = sizeof(textBuf) - 1;
        }
        memcpy(textBuf, &data[5], textLen);
        textBuf[textLen] = '\0';

        // Find which channel this is for
        int channelIdx = findChannelByHash(channel.hash[0]);

        Serial.printf("[MESH] Channel text (ch=%d): %s\n", channelIdx, textBuf);

        // Extract sender name and message text (before and after ": ")
        char senderName[32] = "";
        const char* msgText = textBuf;
        const char* colonPos = strstr(textBuf, ": ");
        if (colonPos && (colonPos - textBuf) < 31) {
            size_t nameLen = colonPos - textBuf;
            memcpy(senderName, textBuf, nameLen);
            senderName[nameLen] = '\0';
            msgText = colonPos + 2;  // Skip ": " to get just the message
        }

        // Note: Repeat detection now happens in filterRecvFloodPacket() BEFORE
        // the duplicate filter, so we don't call checkChannelRepeat() here anymore

        // Call channel message callback for UI notification
        if (_channelMsgCallback && channelIdx >= 0) {
            // Get hop count from packet path_len (only for flood-routed packets)
            uint8_t hops = packet->isRouteFlood() ? packet->path_len : 0;
            _channelMsgCallback(channelIdx, textBuf, timestamp, hops);
        }

        // Also add to general message history
        Message msg;
        memset(&msg, 0, sizeof(msg));
        msg.timestamp = timestamp;
        msg.senderId = 0;  // Unknown sender for channel messages
        strncpy(msg.text, textBuf, sizeof(msg.text) - 1);
        msg.isOutgoing = false;
        msg.delivered = true;
        addMessage(msg);
    }
}

int MeshBerryMesh::findChannelByHash(uint8_t hash) {
    ChannelSettings& chSettings = SettingsManager::getChannelSettings();
    for (int i = 0; i < chSettings.numChannels; i++) {
        if (chSettings.channels[i].isActive && chSettings.channels[i].hash == hash) {
            return i;
        }
    }
    return -1;
}

bool MeshBerryMesh::allowPacketForward(const mesh::Packet* packet) {
    // Always forward DIRECT-routed packets (they have explicit paths)
    // This is safe because DIRECT packets only go to nodes in the path
    if (packet->isRouteDirect()) {
        return true;
    }

    // For FLOOD packets, respect the forwarding setting
    return _forwardingEnabled;
}

bool MeshBerryMesh::hasPendingWork() const {
    // Check if there are outbound packets waiting to be sent
    // Uses 0xFFFFFFFF as "now" to get count regardless of timing
    return _mgr->getOutboundCount(0xFFFFFFFF) > 0;
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

    Serial.printf("[MESH] Sending CLI command to %s: %s\n", _connectedRepeaterName, command);

    // Build command packet: [4-byte timestamp][1-byte flags][command string]
    // MeshCore CLI format: timestamp(4) + flags(1) + text
    // flags byte: bits[7:2] = TXT_TYPE (CLI_DATA=1), bits[1:0] = attempt counter
    // So (TXT_TYPE_CLI_DATA << 2) = (1 << 2) = 0x04
    uint8_t payload[MAX_MESSAGE_LENGTH + 6];
    uint32_t timestamp = getRTCClock()->getCurrentTimeUnique();
    memcpy(payload, &timestamp, 4);
    payload[4] = (TXT_TYPE_CLI_DATA << 2);  // CLI command flag = 0x04

    size_t cmdLen = strlen(command);
    if (cmdLen > MAX_MESSAGE_LENGTH - 1) cmdLen = MAX_MESSAGE_LENGTH - 1;
    memcpy(&payload[5], command, cmdLen);
    payload[5 + cmdLen] = '\0';  // Null terminator in buffer but not counted in length

    // Debug: show payload being sent
    Serial.printf("[MESH] CLI payload: timestamp=%u, flags=0x%02X, cmd='%s', len=%d\n",
                  timestamp, payload[4], command, (int)(5 + cmdLen));
    Serial.print("[MESH] Payload hex: ");
    for (size_t i = 0; i < 5 + cmdLen && i < 24; i++) {
        Serial.printf("%02X", payload[i]);
    }
    Serial.println();

    // Debug: show repeater identity info
    uint8_t repHash[8];
    _repeaterIdentity.copyHashTo(repHash);
    Serial.printf("[MESH] Target repeater hash: %02X%02X%02X%02X\n",
                  repHash[0], repHash[1], repHash[2], repHash[3]);
    Serial.printf("[MESH] Shared secret: %02X%02X%02X%02X...\n",
                  _repeaterSharedSecret[0], _repeaterSharedSecret[1],
                  _repeaterSharedSecret[2], _repeaterSharedSecret[3]);

    // Create encrypted datagram to connected repeater
    // Uses PAYLOAD_TYPE_TXT_MSG (0x02) which repeater expects for CLI commands
    // Note: packet length is 5 + cmdLen (NOT +1), matching MeshCore's sendCommandData()
    mesh::Packet* pkt = createDatagram(PAYLOAD_TYPE_TXT_MSG, _repeaterIdentity,
                                        _repeaterSharedSecret, payload, 5 + cmdLen);
    if (!pkt) {
        Serial.println("[MESH] Failed to create command packet");
        return false;
    }

    // Debug: show packet info
    Serial.printf("[MESH] Packet: type=0x%02X, header=0x%02X, payload_len=%d\n",
                  pkt->getPayloadType(), pkt->header, pkt->payload_len);

    // Send with flood routing
    sendFlood(pkt);

    Serial.println("[MESH] CLI command sent via flood");
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
    // DEBUG: Log the search (show FULL 8-byte hash)
    Serial.printf("[MESH] searchPeersByHash: looking for hash %02X%02X%02X%02X%02X%02X%02X%02X\n",
                  hash[0], hash[1], hash[2], hash[3],
                  hash[4], hash[5], hash[6], hash[7]);

    // Debug: Show all active DM peers (with FULL 8-byte hash)
    Serial.println("[MESH] Active DM peers:");
    for (int i = 0; i < MAX_DM_PEERS; i++) {
        if (_dmPeers[i].isActive) {
            uint8_t peerHash[8];
            _dmPeers[i].identity.copyHashTo(peerHash);
            Serial.printf("[MESH]   DM peer[%d]: contactId=%08X, hash=%02X%02X%02X%02X%02X%02X%02X%02X\n",
                          i, _dmPeers[i].contactId,
                          peerHash[0], peerHash[1], peerHash[2], peerHash[3],
                          peerHash[4], peerHash[5], peerHash[6], peerHash[7]);
        }
    }

    // Reset last matched DM peer
    _lastMatchedDMPeer = -1;

    // Check if hash matches our connected repeater OR pending login
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

    // Check DM peers
    for (int i = 0; i < MAX_DM_PEERS; i++) {
        if (_dmPeers[i].isActive && _dmPeers[i].identity.isHashMatch(hash)) {
            Serial.printf("[MESH] searchPeersByHash: FOUND DM peer match (slot %d)!\n", i);
            _lastMatchedDMPeer = i;  // Remember which DM peer matched
            return 1;
        }
    }

    Serial.println("[MESH] searchPeersByHash: no match");

    // NEW: Try to auto-create DM peer from contacts if hash matches
    Serial.printf("[MESH] No DM peers matched hash %02X%02X%02X%02X - searching contacts\n",
                  hash[0], hash[1], hash[2], hash[3]);

    ContactSettings& contacts = SettingsManager::getContactSettings();
    for (int i = 0; i < contacts.numContacts && i < ContactSettings::MAX_CONTACTS; i++) {
        const ContactEntry* c = contacts.getContact(i);
        if (!c) continue;

        // Check if this contact's pubKey hash matches
        mesh::Identity contactIdentity(c->pubKey);
        if (contactIdentity.isHashMatch(hash)) {
            Serial.printf("[MESH] Found matching contact: %s (id=%08X)\n", c->name, c->id);

            // Auto-create DM peer for this contact
            int slot = findOrCreateDMPeer(c->id);
            if (slot >= 0) {
                Serial.printf("[MESH] Auto-created DM peer in slot %d\n", slot);
                _lastMatchedDMPeer = slot;  // Remember which peer we just created
                return 1;  // Found one matching peer
            } else {
                Serial.printf("[MESH] Failed to create DM peer for %s\n", c->name);
            }
        }
    }

    Serial.println("[MESH] No matching contact found in database");
    return 0;
}

void MeshBerryMesh::getPeerSharedSecret(uint8_t* dest_secret, int peer_idx) {
    Serial.printf("[MESH] getPeerSharedSecret: peer_idx=%d, pending=%d, connected=%d, lastDM=%d\n",
                  peer_idx, _pendingLoginAttempt, _repeaterConnected, _lastMatchedDMPeer);

    // Check if this was a DM peer match (from searchPeersByHash)
    if (_lastMatchedDMPeer >= 0 && _lastMatchedDMPeer < MAX_DM_PEERS) {
        memcpy(dest_secret, _dmPeers[_lastMatchedDMPeer].sharedSecret, PUB_KEY_SIZE);
        Serial.printf("[MESH] getPeerSharedSecret: returning DM peer secret (slot %d)\n", _lastMatchedDMPeer);
        return;
    }

    // Return shared secret for connected repeater OR pending login
    if (peer_idx == 0 && (_repeaterConnected || _pendingLoginAttempt > 0)) {
        memcpy(dest_secret, _repeaterSharedSecret, PUB_KEY_SIZE);
        Serial.printf("[MESH] getPeerSharedSecret: returning repeater secret %02X%02X%02X%02X...\n",
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

    // Check if this PATH_RETURN contains an embedded ACK
    // Official firmware sends ACK this way for FLOOD-routed DMs
    if (extra_type == PAYLOAD_TYPE_ACK && extra_len >= 4) {
        Serial.println("[MESH] ACK embedded in PATH_RETURN (FLOOD routing)");

        // Extract the 4-byte ACK CRC from extra data (little-endian - ESP32 native order)
        uint32_t ack_crc;
        memcpy(&ack_crc, extra, 4);  // Direct copy preserves platform byte order

        Serial.printf("[MESH] Extracted ACK CRC from PATH: %08X\n", ack_crc);

        // Process the ACK using existing onAckRecv handler
        // This will match against pending DMs and mark as delivered
        onAckRecv(packet, ack_crc);

        // Don't return yet - continue to learn path below
    }

    // Learn the return path from the PATH_RETURN packet
    // The 'path' parameter contains the route TO the sender (from our perspective)
    if (path_len > 0) {
        // Try to identify who sent this PATH_RETURN and learn their path
        // Check DM peers first
        if (_lastMatchedDMPeer >= 0 && _lastMatchedDMPeer < MAX_DM_PEERS) {
            uint32_t contactId = _dmPeers[_lastMatchedDMPeer].contactId;
            learnPath(contactId, path, path_len);
            Serial.printf("[ROUTE] Learned path from PATH_RETURN to peer %08X (%d hops)\n",
                          contactId, path_len);
        }
        // Also could be from a repeater
        else if (_repeaterConnected || _pendingLoginAttempt > 0) {
            // Store path to repeater - could be used for direct repeater comms
            Serial.printf("[ROUTE] Received path from repeater (%d hops) - not storing yet\n", path_len);
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
    if (type == PAYLOAD_TYPE_TXT_MSG && len > 5 && _repeaterConnected && _lastMatchedDMPeer < 0) {
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
        return;
    }

    // Handle direct messages from DM peers
    if (type == PAYLOAD_TYPE_TXT_MSG && len > 5 && _lastMatchedDMPeer >= 0) {
        int dmIdx = _lastMatchedDMPeer;
        if (dmIdx >= 0 && dmIdx < MAX_DM_PEERS && _dmPeers[dmIdx].isActive) {
            // Parse DM: [4-byte timestamp][1-byte flags][text]
            uint32_t timestamp;
            memcpy(&timestamp, data, 4);
            uint8_t flags = data[4] >> 2;  // Extract message type flags

            // Only process plain text messages (TXT_TYPE_PLAIN = 0)
            if (flags != 0) {
                Serial.printf("[DM] Ignoring non-plain message (flags=%d)\n", flags);
                return;
            }

            // Extract text (ensure null termination)
            data[len] = '\0';
            const char* text = (const char*)&data[5];  // Text at offset 5
            size_t textLen = strlen(text);

            // Get sender name from contacts
            char senderName[32] = "Unknown";
            ContactSettings& contacts = SettingsManager::getContactSettings();
            int contactIdx = contacts.findContact(_dmPeers[dmIdx].contactId);
            if (contactIdx >= 0) {
                const ContactEntry* c = contacts.getContact(contactIdx);
                if (c) strncpy(senderName, c->name, sizeof(senderName) - 1);
            }

            Serial.printf("[DM] Received from %s (ID=%08X): \"%s\" (ts=%u)\n",
                          senderName, _dmPeers[dmIdx].contactId, text, timestamp);

            // === LEARN PATH FROM FLOOD PACKET ===
            // If the packet came via flood, the packet->path contains the route it took to reach us
            // We can use this as the return path (it will be reversed when sending back)
            if (packet->isRouteFlood() && packet->path_len > 0) {
                learnPath(_dmPeers[dmIdx].contactId, packet->path, packet->path_len);
                Serial.printf("[ROUTE] Learned path from flood DM: %d hops to %s\n",
                              packet->path_len, senderName);
            }

            if (_dmCallback) {
                _dmCallback(_dmPeers[dmIdx].contactId, senderName, text, timestamp);
            }

            // === SEND ACK ===
            // Calculate ACK hash matching meshcore-open format:
            // Hash input: [timestamp(4)][attempt(1)][text][sender_pubkey(32)]
            uint8_t ackHashInput[4 + 1 + 249 + 32];
            size_t ackHashLen = 0;

            // Timestamp (4 bytes, already little-endian)
            memcpy(ackHashInput + ackHashLen, data, 4);
            ackHashLen += 4;

            // Attempt byte (data[4])
            ackHashInput[ackHashLen++] = data[4] & 0x03;

            // Text
            memcpy(ackHashInput + ackHashLen, data + 5, textLen);
            ackHashLen += textLen;

            // Sender's public key (32 bytes)
            const uint8_t* senderPubKey = _dmPeers[dmIdx].identity.pub_key;
            memcpy(ackHashInput + ackHashLen, senderPubKey, PUB_KEY_SIZE);
            ackHashLen += PUB_KEY_SIZE;

            // Calculate ACK hash from complete buffer
            uint32_t ack_hash;
            mesh::Utils::sha256((uint8_t*)&ack_hash, 4,
                                ackHashInput, ackHashLen);

            Serial.printf("[DM] Sending ACK (hash=%08X, input %d bytes)\n",
                          ack_hash, (int)ackHashLen);

            // Send path return with ACK embedded (provides sender with return path)
            if (packet->isRouteFlood()) {
                mesh::Packet* pathAck = createPathReturn(
                    _dmPeers[dmIdx].identity,
                    _dmPeers[dmIdx].sharedSecret,
                    packet->path, packet->path_len,
                    PAYLOAD_TYPE_ACK,
                    (uint8_t*)&ack_hash, 4
                );
                if (pathAck) {
                    sendFlood(pathAck, 200);  // TXT_ACK_DELAY = 200ms
                    Serial.println("[DM] Path+ACK sent via flood");
                }
            } else {
                // No flood route - send simple ACK
                mesh::Packet* ack = createAck(ack_hash);
                if (ack) {
                    sendFlood(ack, 200);
                    Serial.println("[DM] ACK sent via flood");
                }
            }
        }
    }
}

// =============================================================================
// DIRECT MESSAGING
// =============================================================================

int MeshBerryMesh::findOrCreateDMPeer(uint32_t contactId) {
    // Check if peer already exists
    for (int i = 0; i < MAX_DM_PEERS; i++) {
        if (_dmPeers[i].isActive && _dmPeers[i].contactId == contactId) {
            return i;
        }
    }

    // Look up contact to get public key
    ContactSettings& contacts = SettingsManager::getContactSettings();
    int contactIdx = contacts.findContact(contactId);
    if (contactIdx < 0) {
        Serial.printf("[DM] Contact %08X not found\n", contactId);
        return -1;
    }

    const ContactEntry* c = contacts.getContact(contactIdx);
    if (!c) return -1;

    // Check if public key is valid (not all zeros)
    bool hasValidKey = false;
    for (int i = 0; i < 32; i++) {
        if (c->pubKey[i] != 0) { hasValidKey = true; break; }
    }
    if (!hasValidKey) {
        Serial.printf("[DM] ERROR: Contact %s (id=%08X) has no valid public key\n", c->name, contactId);
        Serial.print("[DM]   pubKey: ");
        for (int i = 0; i < 32; i++) {
            Serial.printf("%02X", c->pubKey[i]);
            if (i == 15) Serial.print("\n[DM]            ");
        }
        Serial.println();
        return -1;
    }

    // Find empty slot
    int slot = -1;
    for (int i = 0; i < MAX_DM_PEERS; i++) {
        if (!_dmPeers[i].isActive) { slot = i; break; }
    }
    if (slot < 0) {
        // Evict oldest (slot 0) if full
        memmove(&_dmPeers[0], &_dmPeers[1], sizeof(DMPeer) * (MAX_DM_PEERS - 1));
        slot = MAX_DM_PEERS - 1;
        _dmPeers[slot].isActive = false;
    }

    // DEBUG: Show the pubKey BEFORE creating Identity
    Serial.printf("[DM] About to create Identity for contact %s (id=%08X)\n", c->name, contactId);
    Serial.printf("[DM]   Contact pubKey (full 32 bytes):\n[DM]   ");
    for (int i = 0; i < 32; i++) {
        Serial.printf("%02X", c->pubKey[i]);
        if (i == 15) Serial.print("\n[DM]   ");
    }
    Serial.println();

    // Set up identity and compute shared secret via ECDH
    _dmPeers[slot].contactId = contactId;
    _dmPeers[slot].identity = mesh::Identity(c->pubKey);
    self_id.calcSharedSecret(_dmPeers[slot].sharedSecret, c->pubKey);
    _dmPeers[slot].isActive = true;

    // DEBUG: Show the full Identity hash (8 bytes, not just 4) and verify against pub_key
    uint8_t fullHash[8];
    _dmPeers[slot].identity.copyHashTo(fullHash);
    Serial.printf("[DM] Created DM peer[%d] for contactId=%08X\n", slot, contactId);
    Serial.printf("[DM]   Identity hash: %02X%02X%02X%02X%02X%02X%02X%02X\n",
                  fullHash[0], fullHash[1], fullHash[2], fullHash[3],
                  fullHash[4], fullHash[5], fullHash[6], fullHash[7]);
    Serial.printf("[DM]   Identity.pub_key first 8: %02X%02X%02X%02X%02X%02X%02X%02X\n",
                  _dmPeers[slot].identity.pub_key[0], _dmPeers[slot].identity.pub_key[1],
                  _dmPeers[slot].identity.pub_key[2], _dmPeers[slot].identity.pub_key[3],
                  _dmPeers[slot].identity.pub_key[4], _dmPeers[slot].identity.pub_key[5],
                  _dmPeers[slot].identity.pub_key[6], _dmPeers[slot].identity.pub_key[7]);

    // Initialize path from contact if available
    if (c->outPathLen >= 0 && isPathValid(c->outPathLen, c->pathLearnedAt)) {
        memcpy(_dmPeers[slot].outPath, c->outPath, c->outPathLen);
        _dmPeers[slot].outPathLen = c->outPathLen;
        _dmPeers[slot].pathLearnedAt = c->pathLearnedAt;
        Serial.printf("[DM] Created peer entry for %s (slot %d) with existing path (%d hops)\n",
                      c->name, slot, c->outPathLen);
    } else {
        _dmPeers[slot].clearPath();
        Serial.printf("[DM] Created peer entry for %s (slot %d) - no path yet\n", c->name, slot);
    }
    return slot;
}

int MeshBerryMesh::findDMPeerByHash(const uint8_t* hash) {
    for (int i = 0; i < MAX_DM_PEERS; i++) {
        if (_dmPeers[i].isActive && _dmPeers[i].identity.isHashMatch(hash)) {
            return i;
        }
    }
    return -1;
}

bool MeshBerryMesh::sendDirectMessage(uint32_t contactId, const char* text, uint32_t* out_ack_crc) {
    if (!text || strlen(text) == 0) return false;

    // Find or create DM peer entry
    Serial.printf("[DM] sendDirectMessage: contactId=%08X\n", contactId);

    int peerIdx = findOrCreateDMPeer(contactId);
    if (peerIdx < 0) {
        Serial.println("[DM] Failed to create peer entry");
        return false;
    }

    DMPeer& peer = _dmPeers[peerIdx];

    // Debug: Show the DM peer Identity hash (FULL 8 bytes)
    uint8_t peerHash[8];
    peer.identity.copyHashTo(peerHash);
    Serial.printf("[DM] Using DM peer[%d]: contactId=%08X\n", peerIdx, peer.contactId);
    Serial.printf("[DM]   Identity hash: %02X%02X%02X%02X%02X%02X%02X%02X\n",
                  peerHash[0], peerHash[1], peerHash[2], peerHash[3],
                  peerHash[4], peerHash[5], peerHash[6], peerHash[7]);

    // Build payload: [4-byte timestamp][1-byte flags][text]
    // MeshCore format: flags byte contains (TXT_TYPE << 2) | attempt
    // TXT_TYPE_PLAIN = 0, so flags = 0x00
    uint32_t ts = getRTCClock()->getCurrentTime();
    size_t textLen = strlen(text);
    if (textLen > 249) textLen = 249;  // Limit to fit with header (5 bytes overhead)

    uint8_t payload[260];
    // Build payload for transmission: [timestamp(4)][attempt(1)][text]
    memcpy(payload, &ts, 4);           // Timestamp at offset 0-3
    payload[4] = 0x00;                 // Attempt byte (0 for first send)
    memcpy(payload + 5, text, textLen);// Text at offset 5+

    size_t payloadLen = 5 + textLen;

    Serial.printf("[DM] Sending to %08X: \"%s\" (ts=%u, len=%d)\n",
                  contactId, text, ts, (int)textLen);

    // Calculate expected ACK CRC matching meshcore-open format:
    // Hash input: [timestamp(4)][attempt(1)][text][sender_pubkey(32)]
    uint8_t hashInput[4 + 1 + 249 + 32];  // Max size
    size_t hashLen = 0;

    // Timestamp (little-endian, already in correct format)
    memcpy(hashInput + hashLen, &ts, 4);
    hashLen += 4;

    // Attempt (lower 2 bits only, matching meshcore-open)
    hashInput[hashLen++] = 0x00 & 0x03;  // First send is attempt 0

    // Text
    memcpy(hashInput + hashLen, text, textLen);
    hashLen += textLen;

    // Sender public key (32 bytes)
    memcpy(hashInput + hashLen, self_id.pub_key, PUB_KEY_SIZE);
    hashLen += PUB_KEY_SIZE;

    // Calculate ACK: SHA256 of entire buffer, truncated to 4 bytes
    uint32_t expected_ack;
    mesh::Utils::sha256((uint8_t*)&expected_ack, 4,
                        hashInput, hashLen);

    Serial.printf("[DM] Expected ACK CRC: %08X (hash input %d bytes)\n",
                  expected_ack, (int)hashLen);

    // Return ACK CRC to caller if requested
    if (out_ack_crc) {
        *out_ack_crc = expected_ack;
    }

    // Find a pending slot for tracking delivery
    int pendingSlot = findFreePendingSlot();

    // Create encrypted datagram using PAYLOAD_TYPE_TXT_MSG (0x02)
    mesh::Packet* pkt = createDatagram(PAYLOAD_TYPE_TXT_MSG, peer.identity,
                                        peer.sharedSecret, payload, payloadLen);
    if (!pkt) {
        Serial.println("[DM] Failed to create packet");
        return false;
    }

    // Check if we have a valid path to this peer
    bool useDirect = isPathValid(peer.outPathLen, peer.pathLearnedAt);

    // Debug output for routing decision
    if (useDirect) {
        if (peer.outPathLen == 0) {
            Serial.printf("[DM] Using DIRECT route (direct neighbor, learned %lums ago)\n",
                          millis() - peer.pathLearnedAt);
        } else {
            Serial.printf("[DM] Using DIRECT route (%d hops, learned %lums ago)\n",
                          peer.outPathLen, millis() - peer.pathLearnedAt);
        }
        // Use direct routing with learned path
        sendDirect(pkt, peer.outPath, peer.outPathLen);
    } else {
        if (peer.outPathLen < 0) {
            Serial.printf("[DM] Using FLOOD route (no path learned)\n");
        } else {
            Serial.printf("[DM] Using FLOOD route (path expired)\n");
        }
        // No valid path - use flood routing
        sendFlood(pkt);
    }

    // Track pending DM for delivery status
    if (pendingSlot >= 0) {
        _pendingDMs[pendingSlot].ack_crc = expected_ack;
        _pendingDMs[pendingSlot].contactId = contactId;
        _pendingDMs[pendingSlot].sentAt = millis();
        // DIRECT timeout: 20s, FLOOD timeout: 20s
        _pendingDMs[pendingSlot].timeout = millis() + 20000;
        memcpy(_pendingDMs[pendingSlot].payload, payload, payloadLen);
        _pendingDMs[pendingSlot].payloadLen = payloadLen;
        _pendingDMs[pendingSlot].attempts = 1;
        _pendingDMs[pendingSlot].pathLen = useDirect ? peer.outPathLen : 0;
        _pendingDMs[pendingSlot].isFlood = !useDirect;
        _pendingDMs[pendingSlot].active = true;
        Serial.printf("[DM] Tracking delivery in slot %d (timeout in 20000ms)\n", pendingSlot);
    }

    return true;
}

// =============================================================================
// PATH MANAGEMENT FOR ROUTING
// =============================================================================

void MeshBerryMesh::learnPath(uint32_t contactId, const uint8_t* path, uint8_t pathLen) {
    if (!path || pathLen == 0 || pathLen > 64) return;

    Serial.printf("[ROUTE] Learning path to %08X (%d hops)\n", contactId, pathLen);

    // Update DMPeer if it exists
    for (int i = 0; i < MAX_DM_PEERS; i++) {
        if (_dmPeers[i].isActive && _dmPeers[i].contactId == contactId) {
            memcpy(_dmPeers[i].outPath, path, pathLen);
            _dmPeers[i].outPathLen = pathLen;
            _dmPeers[i].pathLearnedAt = millis();
            Serial.printf("[ROUTE] Updated DMPeer slot %d with path\n", i);
            break;
        }
    }

    // Also update ContactEntry for persistence
    ContactSettings& contacts = SettingsManager::getContactSettings();
    int idx = contacts.findContact(contactId);
    if (idx >= 0) {
        ContactEntry* c = contacts.getContact(idx);
        if (c) {
            memcpy(c->outPath, path, pathLen);
            c->outPathLen = pathLen;
            c->pathLearnedAt = millis();
            Serial.printf("[ROUTE] Updated contact '%s' with path\n", c->name);
        }
    }
}

bool MeshBerryMesh::isPathValid(int8_t pathLen, uint32_t learnedAt) const {
    if (pathLen < 0) return false;  // Unknown path (-1 = never learned)

    // pathLen == 0 can mean two things:
    // 1. Never learned a path (learnedAt == 0) → FLOOD
    // 2. Confirmed direct neighbor (learnedAt > 0) → DIRECT
    if (pathLen == 0) {
        if (learnedAt == 0) {
            return false;  // No path learned - must FLOOD
        }
        // Direct neighbor confirmed by PATH_RETURN
        return true;
    }

    // Multi-hop path - check for expiration
    uint32_t age = millis() - learnedAt;
    if (age > PATH_EXPIRY_MS) {
        return false;  // Path expired
    }
    return true;
}

void MeshBerryMesh::invalidatePath(uint32_t contactId) {
    Serial.printf("[ROUTE] Invalidating path to %08X\n", contactId);

    // Clear from DMPeer
    for (int i = 0; i < MAX_DM_PEERS; i++) {
        if (_dmPeers[i].isActive && _dmPeers[i].contactId == contactId) {
            _dmPeers[i].clearPath();
            break;
        }
    }

    // Clear from ContactEntry
    ContactSettings& contacts = SettingsManager::getContactSettings();
    int idx = contacts.findContact(contactId);
    if (idx >= 0) {
        ContactEntry* c = contacts.getContact(idx);
        if (c) {
            memset(c->outPath, 0, sizeof(c->outPath));
            c->outPathLen = -1;
            c->pathLearnedAt = 0;
        }
    }
}

// =============================================================================
// PENDING DM MANAGEMENT FOR DELIVERY STATUS
// =============================================================================

int MeshBerryMesh::findFreePendingSlot() {
    for (int i = 0; i < MAX_PENDING_DMS; i++) {
        if (!_pendingDMs[i].active) {
            return i;
        }
    }
    // Evict oldest (by sentAt time)
    int oldest = 0;
    for (int i = 1; i < MAX_PENDING_DMS; i++) {
        if (_pendingDMs[i].sentAt < _pendingDMs[oldest].sentAt) {
            oldest = i;
        }
    }
    // Mark evicted as failed
    if (_deliveryCallback) {
        _deliveryCallback(_pendingDMs[oldest].contactId, _pendingDMs[oldest].ack_crc,
                          false, _pendingDMs[oldest].attempts);
    }
    _pendingDMs[oldest].active = false;
    return oldest;
}

void MeshBerryMesh::retryDMWithFlood(int pendingIdx) {
    if (pendingIdx < 0 || pendingIdx >= MAX_PENDING_DMS) return;
    PendingDM& pending = _pendingDMs[pendingIdx];
    if (!pending.active) return;

    Serial.printf("[DM] Retrying message to %08X via FLOOD (attempt %d)\n",
                  pending.contactId, pending.attempts + 1);

    // Find the DM peer for this contact
    int peerIdx = -1;
    for (int i = 0; i < MAX_DM_PEERS; i++) {
        if (_dmPeers[i].isActive && _dmPeers[i].contactId == pending.contactId) {
            peerIdx = i;
            break;
        }
    }
    if (peerIdx < 0) {
        Serial.println("[DM] Peer not found for retry - marking failed");
        pending.active = false;
        if (_deliveryCallback) {
            _deliveryCallback(pending.contactId, pending.ack_crc, false, pending.attempts);
        }
        return;
    }

    DMPeer& peer = _dmPeers[peerIdx];

    // Create new packet with same payload
    mesh::Packet* pkt = createDatagram(PAYLOAD_TYPE_TXT_MSG, peer.identity,
                                        peer.sharedSecret, pending.payload, pending.payloadLen);
    if (!pkt) {
        Serial.println("[DM] Failed to create retry packet");
        pending.active = false;
        if (_deliveryCallback) {
            _deliveryCallback(pending.contactId, pending.ack_crc, false, pending.attempts);
        }
        return;
    }

    // Send via flood
    sendFlood(pkt);

    // Update tracking
    pending.attempts++;
    pending.isFlood = true;
    pending.timeout = millis() + 20000;  // 20s timeout for flood
    pending.sentAt = millis();

    Serial.printf("[DM] Retry sent via FLOOD (timeout in 20s)\n");
}

int MeshBerryMesh::calculateMaxRetries(bool isFlood, uint8_t pathLen) {
    if (isFlood) {
        // Flood routing: conservative retry count since it broadcasts
        return 3;  // 4 total attempts (1 initial + 3 retries)
    } else {
        // Direct routing: retries based on hop count
        // More hops = more potential points of failure
        // Formula: base 2 retries + 1 per hop (capped at 8)
        int retries = 2 + pathLen;
        return (retries > 8) ? 8 : retries;
        // Examples:
        //   0 hops (direct neighbor): 2 retries (3 total attempts)
        //   1 hop: 3 retries (4 total attempts)
        //   2 hops: 4 retries (5 total attempts)
        //   5 hops: 7 retries (8 total attempts)
        //   10+ hops: 8 retries (9 total attempts, capped)
    }
}

void MeshBerryMesh::retryDMWithDirect(int pendingIdx) {
    if (pendingIdx < 0 || pendingIdx >= MAX_PENDING_DMS) return;
    PendingDM& pending = _pendingDMs[pendingIdx];
    if (!pending.active) return;

    Serial.printf("[DM] Retrying message to %08X via DIRECT (attempt %d)\n",
                  pending.contactId, pending.attempts + 1);

    // Find the DM peer for this contact
    int peerIdx = -1;
    for (int i = 0; i < MAX_DM_PEERS; i++) {
        if (_dmPeers[i].isActive && _dmPeers[i].contactId == pending.contactId) {
            peerIdx = i;
            break;
        }
    }
    if (peerIdx < 0) {
        Serial.println("[DM] Peer not found for retry - marking failed");
        pending.active = false;
        if (_deliveryCallback) {
            _deliveryCallback(pending.contactId, pending.ack_crc, false, pending.attempts);
        }
        return;
    }

    DMPeer& peer = _dmPeers[peerIdx];

    // Verify path is still valid
    if (!isPathValid(peer.outPathLen, peer.pathLearnedAt)) {
        Serial.println("[DM] Path expired during retry - switching to flood");
        retryDMWithFlood(pendingIdx);
        return;
    }

    // Create new packet with same payload
    mesh::Packet* pkt = createDatagram(PAYLOAD_TYPE_TXT_MSG, peer.identity,
                                        peer.sharedSecret, pending.payload, pending.payloadLen);
    if (!pkt) {
        Serial.println("[DM] Failed to create retry packet");
        pending.active = false;
        if (_deliveryCallback) {
            _deliveryCallback(pending.contactId, pending.ack_crc, false, pending.attempts);
        }
        return;
    }

    // Send via direct route with same path
    sendDirect(pkt, peer.outPath, peer.outPathLen);

    // Update tracking
    pending.attempts++;
    pending.isFlood = false;
    pending.timeout = millis() + 10000;  // 10s timeout for direct
    pending.sentAt = millis();

    Serial.printf("[DM] Retry sent via DIRECT route (%d hops)\n", peer.outPathLen);
}

void MeshBerryMesh::checkPendingTimeouts() {
    uint32_t now = millis();

    for (int i = 0; i < MAX_PENDING_DMS; i++) {
        if (!_pendingDMs[i].active) continue;

        if (now >= _pendingDMs[i].timeout) {
            // Calculate max retries based on routing method and path length
            int maxRetries = calculateMaxRetries(_pendingDMs[i].isFlood,
                                                  _pendingDMs[i].pathLen);

            if (_pendingDMs[i].attempts > maxRetries) {
                // Max retries reached - mark as failed
                Serial.printf("[DM] Delivery FAILED for contact %08X after %d attempts (max %d retries)\n",
                              _pendingDMs[i].contactId, _pendingDMs[i].attempts, maxRetries);

                uint32_t contactId = _pendingDMs[i].contactId;
                uint32_t ack_crc = _pendingDMs[i].ack_crc;
                uint8_t attempts = _pendingDMs[i].attempts;
                _pendingDMs[i].active = false;

                // Invalidate path since delivery failed
                invalidatePath(contactId);

                if (_deliveryCallback) {
                    _deliveryCallback(contactId, ack_crc, false, attempts);
                }
            } else {
                // Retry - fallback to FLOOD if DIRECT fails (recipient might not have return path)
                if (_pendingDMs[i].isFlood) {
                    Serial.printf("[DM] Flood timed out for %08X - retry %d/%d\n",
                                  _pendingDMs[i].contactId, _pendingDMs[i].attempts, maxRetries + 1);
                    retryDMWithFlood(i);
                } else {
                    // DIRECT failed - fallback to FLOOD for next attempt
                    Serial.printf("[DM] Direct timed out for %08X - falling back to FLOOD for retry %d/%d\n",
                                  _pendingDMs[i].contactId, _pendingDMs[i].attempts, maxRetries + 1);
                    _pendingDMs[i].isFlood = true;  // Switch to FLOOD for retries
                    retryDMWithFlood(i);
                }
            }
        }
    }
}

// =============================================================================
// CHANNEL REPEAT TRACKING
// =============================================================================

uint32_t MeshBerryMesh::hashChannelMessage(int channelIdx, const char* text) {
    // Create a simple hash from channel index and text content
    // This hash is independent of timestamp, so RTC sync won't affect matching
    uint32_t hash = 0x811c9dc5;  // FNV-1a offset basis
    const uint32_t prime = 0x01000193;  // FNV-1a prime

    // Mix in channel index
    hash ^= (uint32_t)channelIdx;
    hash *= prime;

    // Mix in text content
    if (text) {
        while (*text) {
            hash ^= (uint8_t)*text++;
            hash *= prime;
        }
    }

    return hash;
}

void MeshBerryMesh::trackSentChannelMessage(int channelIdx, const char* text) {
    // Clean up expired entries first
    uint32_t now = millis();
    for (int i = 0; i < MAX_CHANNEL_STATS; i++) {
        if (_channelStats[i].active &&
            (now - _channelStats[i].sentAt) > CHANNEL_STATS_EXPIRY_MS) {
            _channelStats[i].active = false;
        }
    }

    // Find empty slot or oldest
    int slot = -1;
    int oldestSlot = 0;
    uint32_t oldestTime = UINT32_MAX;

    for (int i = 0; i < MAX_CHANNEL_STATS; i++) {
        if (!_channelStats[i].active) {
            slot = i;
            break;
        }
        if (_channelStats[i].sentAt < oldestTime) {
            oldestTime = _channelStats[i].sentAt;
            oldestSlot = i;
        }
    }

    if (slot < 0) slot = oldestSlot;

    uint32_t contentHash = hashChannelMessage(channelIdx, text);
    _channelStats[slot].contentHash = contentHash;
    _channelStats[slot].sentAt = now;
    _channelStats[slot].channelIdx = channelIdx;
    _channelStats[slot].repeatCount = 0;
    _channelStats[slot].active = true;

    Serial.printf("[CHANNEL] Tracking sent message on ch=%d, hash=%08X\n", channelIdx, contentHash);
}

void MeshBerryMesh::checkChannelRepeat(int channelIdx, const char* text, const char* senderName) {
    // Debug: Show what we're checking
    Serial.printf("[REPEAT CHECK] sender='%s' nodeName='%s' match=%d\n",
                  senderName, _nodeName, strcmp(senderName, _nodeName) == 0);

    // Check if sender matches our node name
    if (strcmp(senderName, _nodeName) != 0) {
        return;  // Not our message
    }

    // Calculate hash of received message
    uint32_t receivedHash = hashChannelMessage(channelIdx, text);
    uint32_t now = millis();
    Serial.printf("[REPEAT CHECK] Looking for hash=%08X on ch=%d\n", receivedHash, channelIdx);

    // Find matching tracked message by content hash
    for (int i = 0; i < MAX_CHANNEL_STATS; i++) {
        if (_channelStats[i].active &&
            _channelStats[i].channelIdx == channelIdx &&
            _channelStats[i].contentHash == receivedHash) {

            // Check if not expired
            if ((now - _channelStats[i].sentAt) > CHANNEL_STATS_EXPIRY_MS) {
                _channelStats[i].active = false;
                continue;
            }

            _channelStats[i].repeatCount++;
            Serial.printf("[CHANNEL] Heard repeat of our message (ch=%d, hash=%08X, count=%d)\n",
                          channelIdx, receivedHash, _channelStats[i].repeatCount);

            if (_repeatCallback) {
                _repeatCallback(channelIdx, receivedHash, _channelStats[i].repeatCount);
            }
            return;
        }
    }
}
