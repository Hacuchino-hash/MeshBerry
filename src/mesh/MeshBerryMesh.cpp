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
#include <string.h>

MeshBerryMesh::MeshBerryMesh(mesh::Radio& radio, mesh::RNG& rng, mesh::RTCClock& rtc,
                             SimpleMeshTables& tables, StaticPoolPacketManager& mgr)
    : mesh::Mesh(radio, _msClock, rng, rtc, mgr, tables)
    , _nodeCount(0)
    , _messageCount(0)
    , _messageHead(0)
    , _msgCallback(nullptr)
    , _nodeCallback(nullptr)
{
    strcpy(_nodeName, "MeshBerry");
    memset(_nodes, 0, sizeof(_nodes));
    memset(_messages, 0, sizeof(_messages));
}

bool MeshBerryMesh::begin() {
    Serial.println("[MESH] Starting mesh network...");

    // Initialize the mesh base class
    mesh::Mesh::begin();

    // Generate identity
    // LocalIdentity default constructor creates null identity
    // Create new identity using RNG
    Serial.println("[MESH] Generating new node identity...");
    self_id = mesh::LocalIdentity(getRNG());

    Serial.println("[MESH] Mesh network started");
    return true;
}

void MeshBerryMesh::loop() {
    // Process mesh events
    mesh::Mesh::loop();
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

void MeshBerryMesh::sendAdvertisement() {
    // Create advertisement with node name as app data
    mesh::Packet* pkt = createAdvert(self_id, (const uint8_t*)_nodeName, strlen(_nodeName));
    if (pkt) {
        sendFlood(pkt);
        Serial.println("[MESH] Advertisement sent");
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

    NodeInfo node;
    memset(&node, 0, sizeof(node));

    // Extract node ID from identity hash
    uint8_t hash[MAX_HASH_SIZE];
    id.copyHashTo(hash);
    memcpy(&node.id, hash, sizeof(node.id));

    // Extract name from app data
    if (app_data && app_data_len > 0) {
        size_t nameLen = app_data_len;
        if (nameLen > sizeof(node.name) - 1) {
            nameLen = sizeof(node.name) - 1;
        }
        memcpy(node.name, app_data, nameLen);
        node.name[nameLen] = '\0';
    } else {
        strcpy(node.name, "Unknown");
    }

    node.rssi = (int16_t)packet->_snr;  // SNR stored in packet
    node.snr = packet->getSNR();
    node.lastHeard = timestamp;
    node.hasLocation = false;

    updateNode(node);

    Serial.printf("[MESH] Node: %s (RSSI: %d, SNR: %.1f)\n",
                  node.name, node.rssi, node.snr);
}

void MeshBerryMesh::onAnonDataRecv(mesh::Packet* packet, const uint8_t* secret,
                                    const mesh::Identity& sender, uint8_t* data, size_t len) {
    Serial.println("[MESH] Anonymous data received");

    // For now, treat as text message
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
