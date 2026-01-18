/**
 * MeshBerry Mesh Application
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
 *
 * MeshCore integration for MeshBerry firmware
 */

#ifndef MESHBERRY_MESH_H
#define MESHBERRY_MESH_H

#include <Arduino.h>
#include <Mesh.h>
#include <helpers/SimpleMeshTables.h>
#include <helpers/StaticPoolPacketManager.h>
#include "../config.h"
#include "../board/TDeckBoard.h"

// Forward declarations
class MeshBerryRadio;

/**
 * Arduino-compatible millisecond clock for MeshCore
 */
class ArduinoMillisClock : public mesh::MillisecondClock {
public:
    unsigned long getMillis() override {
        return millis();
    }
};

/**
 * ESP32 RTC clock implementation for MeshCore
 */
class ESP32RTCClock : public mesh::RTCClock {
private:
    uint32_t _epoch_offset;

public:
    ESP32RTCClock() : _epoch_offset(0) {}

    void begin() {
        // Could sync with NTP or GPS here
    }

    uint32_t getCurrentTime() override {
        return (millis() / 1000) + _epoch_offset;
    }

    void setCurrentTime(uint32_t time) override {
        _epoch_offset = time - (millis() / 1000);
    }
};

/**
 * Simple RNG using ESP32 hardware random
 */
class ESP32RNG : public mesh::RNG {
public:
    void random(uint8_t* dest, size_t sz) override {
        for (size_t i = 0; i < sz; i++) {
            dest[i] = esp_random() & 0xFF;
        }
    }
};

/**
 * Node information structure
 */
struct NodeInfo {
    uint32_t id;
    char name[32];
    int16_t rssi;
    float snr;
    uint32_t lastHeard;
    bool hasLocation;
    float latitude;
    float longitude;
};

/**
 * Message structure
 */
struct Message {
    uint32_t senderId;
    uint32_t timestamp;
    char text[MAX_MESSAGE_LENGTH];
    bool isOutgoing;
    bool delivered;
};

/**
 * MeshBerry Mesh Application
 * Simplified mesh layer for standalone T-Deck operation
 */
class MeshBerryMesh : public mesh::Mesh {
public:
    MeshBerryMesh(mesh::Radio& radio, mesh::RNG& rng, mesh::RTCClock& rtc,
                  SimpleMeshTables& tables, StaticPoolPacketManager& mgr);

    /**
     * Initialize the mesh network
     */
    bool begin();

    /**
     * Process mesh events (call in main loop)
     */
    void loop();

    /**
     * Send a text message to broadcast
     * @param text Message text
     * @return true if queued for send
     */
    bool sendBroadcast(const char* text);

    /**
     * Send advertisement packet
     */
    void sendAdvertisement();

    /**
     * Get node name
     */
    const char* getNodeName() const { return _nodeName; }

    /**
     * Set node name
     */
    void setNodeName(const char* name);

    /**
     * Get number of known nodes
     */
    int getNodeCount() const { return _nodeCount; }

    /**
     * Get node info by index
     */
    bool getNodeInfo(int index, NodeInfo& info) const;

    /**
     * Get number of messages
     */
    int getMessageCount() const { return _messageCount; }

    /**
     * Get message by index
     */
    bool getMessage(int index, Message& msg) const;

    /**
     * Callback type for message received
     */
    typedef void (*MessageCallback)(const Message& msg);

    /**
     * Set message received callback
     */
    void setMessageCallback(MessageCallback cb) { _msgCallback = cb; }

    /**
     * Callback type for node discovered
     */
    typedef void (*NodeCallback)(const NodeInfo& node);

    /**
     * Set node discovered callback
     */
    void setNodeCallback(NodeCallback cb) { _nodeCallback = cb; }

protected:
    // MeshCore virtual method overrides
    void onAdvertRecv(mesh::Packet* packet, const mesh::Identity& id,
                      uint32_t timestamp, const uint8_t* app_data, size_t app_data_len) override;

    void onAnonDataRecv(mesh::Packet* packet, const uint8_t* secret,
                        const mesh::Identity& sender, uint8_t* data, size_t len) override;

    void onAckRecv(mesh::Packet* packet, uint32_t ack_crc) override;

private:
    char _nodeName[32];
    ArduinoMillisClock _msClock;

    // Node tracking
    static const int MAX_TRACKED_NODES = MAX_NODES;
    NodeInfo _nodes[MAX_TRACKED_NODES];
    int _nodeCount;

    // Message history
    static const int MAX_MESSAGES = MESSAGE_HISTORY;
    Message _messages[MAX_MESSAGES];
    int _messageCount;
    int _messageHead;

    // Callbacks
    MessageCallback _msgCallback;
    NodeCallback _nodeCallback;

    // Add message to history
    void addMessage(const Message& msg);

    // Add or update node
    void updateNode(const NodeInfo& node);
};

#endif // MESHBERRY_MESH_H
