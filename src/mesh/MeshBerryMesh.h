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
    bool _timeIsSet;  // true once clock has been synced from GPS, peer, or manual set

public:
    // Minimum valid epoch: Jan 1, 2025 (1735689600)
    static constexpr uint32_t MIN_VALID_EPOCH = 1735689600;

    ESP32RTCClock() : _timeIsSet(false) {
        // Set a reasonable default epoch (Jan 1, 2025 = 1735689600)
        // This ensures timestamps are plausible even before syncing from advertisements
        // The exact time will be synced from received advertisements
        _epoch_offset = MIN_VALID_EPOCH;
    }

    void begin() {
        // Could sync with NTP or GPS here
    }

    uint32_t getCurrentTime() override {
        return (millis() / 1000) + _epoch_offset;
    }

    void setCurrentTime(uint32_t time) override {
        _epoch_offset = time - (millis() / 1000);
        _timeIsSet = true;  // Mark as set
    }

    /**
     * Check if time has been set (from GPS, peer, or manual)
     */
    bool isTimeSet() const { return _timeIsSet; }

    /**
     * Try to sync time from a peer timestamp (only if not already set)
     * @param peerTime Unix timestamp from a peer
     * @return true if time was synced
     */
    bool trySyncFromPeer(uint32_t peerTime) {
        // Only sync if not already set and peer time is valid
        if (!_timeIsSet && peerTime > MIN_VALID_EPOCH) {
            setCurrentTime(peerTime);
            return true;
        }
        return false;
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
 * Node types (from MeshCore AdvertDataHelpers.h)
 */
enum NodeType : uint8_t {
    NODE_TYPE_UNKNOWN = 0,
    NODE_TYPE_CHAT = 1,      // Regular chat client
    NODE_TYPE_REPEATER = 2,  // Repeater/relay node
    NODE_TYPE_ROOM = 3,      // Room server (bulletin board)
    NODE_TYPE_SENSOR = 4     // Sensor node
};

/**
 * Node information structure
 */
struct NodeInfo {
    uint32_t id;
    char name[32];
    NodeType type;           // Node type from advertisement
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
     * Send a text message to a specific channel
     * @param channelIdx Channel index from ChannelSettings
     * @param text Message text
     * @return true if queued for send
     */
    bool sendToChannel(int channelIdx, const char* text);

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
     * Callback type for channel message received
     * @param channelIdx Index of the channel (0-7)
     * @param senderAndText Message text in format "SenderName: message"
     * @param timestamp Unix timestamp of the message
     */
    typedef void (*ChannelMessageCallback)(int channelIdx, const char* senderAndText, uint32_t timestamp);

    /**
     * Set channel message callback
     */
    void setChannelMessageCallback(ChannelMessageCallback cb) { _channelMsgCallback = cb; }

    /**
     * Callback type for node discovered
     */
    typedef void (*NodeCallback)(const NodeInfo& node);

    /**
     * Set node discovered callback
     */
    void setNodeCallback(NodeCallback cb) { _nodeCallback = cb; }

    /**
     * Enable or disable packet forwarding (repeater mode)
     */
    void setForwardingEnabled(bool enabled) { _forwardingEnabled = enabled; }

    /**
     * Check if forwarding is enabled
     */
    bool isForwardingEnabled() const { return _forwardingEnabled; }

    // =========================================================================
    // REPEATER MANAGEMENT
    // =========================================================================

    /**
     * Callback type for login response
     * @param success True if login succeeded
     * @param permissions Permission level (0=guest, 3=admin)
     * @param repeaterName Name of connected repeater
     */
    typedef void (*LoginCallback)(bool success, uint8_t permissions, const char* repeaterName);

    /**
     * Callback type for CLI response
     * @param response The response text from the repeater
     */
    typedef void (*CLIResponseCallback)(const char* response);

    /**
     * Callback type for direct message received
     * @param senderId Node ID of the sender
     * @param senderName Name of the sender (from contacts)
     * @param text Message text
     * @param timestamp Unix timestamp of the message
     */
    typedef void (*DMCallback)(uint32_t senderId, const char* senderName, const char* text, uint32_t timestamp);

    /**
     * Set login callback
     */
    void setLoginCallback(LoginCallback cb) { _loginCallback = cb; }

    /**
     * Set CLI response callback
     */
    void setCLIResponseCallback(CLIResponseCallback cb) { _cliCallback = cb; }

    /**
     * Set direct message callback
     */
    void setDMCallback(DMCallback cb) { _dmCallback = cb; }

    // =========================================================================
    // DIRECT MESSAGING
    // =========================================================================

    /**
     * Send a direct message to a contact
     * @param contactId Node ID of the recipient
     * @param text Message text
     * @return true if packet was queued for send
     */
    bool sendDirectMessage(uint32_t contactId, const char* text);

    /**
     * Send login request to a repeater
     * @param repeaterId Node ID of the repeater
     * @param repeaterPubKey Public key of the repeater (32 bytes)
     * @param password Admin or guest password
     * @return true if packet was queued for send
     */
    bool sendRepeaterLogin(uint32_t repeaterId, const uint8_t* repeaterPubKey, const char* password);

    /**
     * Send CLI command to connected repeater
     * @param command The CLI command to send
     * @return true if packet was queued for send
     */
    bool sendRepeaterCommand(const char* command);

    /**
     * Disconnect from current repeater
     */
    void disconnectRepeater();

    /**
     * Check if connected to a repeater
     */
    bool isRepeaterConnected() const { return _repeaterConnected; }

    /**
     * Get connected repeater name
     */
    const char* getConnectedRepeaterName() const { return _connectedRepeaterName; }

    /**
     * Get connected repeater permissions
     */
    uint8_t getRepeaterPermissions() const { return _repeaterPermissions; }

protected:
    // MeshCore virtual method overrides
    void onAdvertRecv(mesh::Packet* packet, const mesh::Identity& id,
                      uint32_t timestamp, const uint8_t* app_data, size_t app_data_len) override;

    void onAnonDataRecv(mesh::Packet* packet, const uint8_t* secret,
                        const mesh::Identity& sender, uint8_t* data, size_t len) override;

    void onAckRecv(mesh::Packet* packet, uint32_t ack_crc) override;

    // Channel message handling overrides
    int searchChannelsByHash(const uint8_t* hash, mesh::GroupChannel channels[], int max_matches) override;
    void onGroupDataRecv(mesh::Packet* packet, uint8_t type, const mesh::GroupChannel& channel, uint8_t* data, size_t len) override;

    // Enable packet forwarding for mesh relay
    bool allowPacketForward(const mesh::Packet* packet) override;

    // Peer data handling for CLI responses
    int searchPeersByHash(const uint8_t* hash) override;
    void getPeerSharedSecret(uint8_t* dest_secret, int peer_idx) override;
    bool onPeerPathRecv(mesh::Packet* packet, int sender_idx, const uint8_t* secret, uint8_t* path, uint8_t path_len, uint8_t extra_type, uint8_t* extra, uint8_t extra_len) override;
    void onPeerDataRecv(mesh::Packet* packet, uint8_t type, int sender_idx, const uint8_t* secret, uint8_t* data, size_t len) override;

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
    LoginCallback _loginCallback;
    CLIResponseCallback _cliCallback;
    ChannelMessageCallback _channelMsgCallback;
    DMCallback _dmCallback;

    // DM peer tracking (for decrypting incoming DMs)
    struct DMPeer {
        uint32_t contactId;
        mesh::Identity identity;
        uint8_t sharedSecret[32];
        bool isActive;

        // Routing info for direct messaging
        uint8_t outPath[64];      // Learned path to this peer (max 64 hops)
        int8_t outPathLen;        // -1 = unknown, 0+ = valid path length
        uint32_t pathLearnedAt;   // millis() when path was learned

        void clearPath() {
            memset(outPath, 0, sizeof(outPath));
            outPathLen = -1;
            pathLearnedAt = 0;
        }
    };
    static const int MAX_DM_PEERS = 8;
    DMPeer _dmPeers[MAX_DM_PEERS];
    int _lastMatchedDMPeer;  // Index of last matched DM peer (-1 if none/repeater)

    // Forwarding state
    bool _forwardingEnabled;

    // Repeater session state
    uint32_t _connectedRepeaterId;
    char _connectedRepeaterName[32];
    uint8_t _repeaterPermissions;
    uint8_t _repeaterSharedSecret[32];
    mesh::Identity _repeaterIdentity;
    bool _repeaterConnected;
    uint8_t _pendingLoginAttempt;
    uint32_t _loginStartTime;  // For login timeout

    // Add message to history
    void addMessage(const Message& msg);

    // Add or update node
    void updateNode(const NodeInfo& node);

    // Find channel index by hash
    int findChannelByHash(uint8_t hash);

    // DM peer management
    int findOrCreateDMPeer(uint32_t contactId);
    int findDMPeerByHash(const uint8_t* hash);

    // Path management for routing
    static const uint32_t PATH_EXPIRY_MS = 30 * 60 * 1000;  // 30 minutes

    /**
     * Learn/store a path to a contact from received packet
     * @param contactId Node ID to store path for
     * @param path The path array (route the packet took)
     * @param pathLen Length of the path
     */
    void learnPath(uint32_t contactId, const uint8_t* path, uint8_t pathLen);

    /**
     * Check if a stored path is still valid (not expired)
     * @param pathLen Path length (-1 = unknown)
     * @param learnedAt When the path was learned
     * @return true if path is valid and not expired
     */
    bool isPathValid(int8_t pathLen, uint32_t learnedAt) const;

    /**
     * Invalidate (clear) path to a contact
     * @param contactId Node ID to clear path for
     */
    void invalidatePath(uint32_t contactId);
};

#endif // MESHBERRY_MESH_H
