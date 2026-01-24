/**
 * MeshCore Adapter for Meshtastic
 *
 * Provides MeshCore integration layer for Meshtastic firmware.
 * This adapter allows MeshCore to handle mesh routing while preserving
 * Meshtastic's UI and peripheral systems.
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include <Arduino.h>
#include <Mesh.h>
#include <helpers/SimpleMeshTables.h>
#include <helpers/StaticPoolPacketManager.h>
#include <helpers/radiolib/CustomSX1262.h>
#include <helpers/radiolib/RadioLibWrappers.h>

// Forward declarations
class MeshCoreAdapter;
class LockingArduinoHal;

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

// Note: ESP32RTCClock is provided by MeshCore's helpers/ESP32Board.h

/**
 * SX1262 wrapper for RadioLib 7.x compatibility
 */
class MeshCoreSX1262Wrapper : public RadioLibWrapper {
public:
    MeshCoreSX1262Wrapper(CustomSX1262& radio, mesh::MainBoard& board)
        : RadioLibWrapper(radio, board) { }

    bool isReceivingPacket() override {
        return ((CustomSX1262 *)_radio)->isReceiving();
    }

    float getCurrentRSSI() override {
        return ((CustomSX1262 *)_radio)->getRSSI(false);
    }

    float getLastRSSI() const override {
        return ((CustomSX1262 *)_radio)->getRSSI();
    }

    float getLastSNR() const override {
        return ((CustomSX1262 *)_radio)->getSNR();
    }

    float packetScore(float snr, int packet_len) override {
        // Default SF=10 for packet scoring
        return packetScoreInt(snr, 10, packet_len);
    }

    void powerOff() override {
        ((CustomSX1262 *)_radio)->sleep(false);
    }
};

/**
 * Node information from MeshCore advertisements
 */
struct MeshCoreNode {
    uint32_t id;
    char name[32];
    uint8_t type;       // 0=unknown, 1=chat, 2=repeater, 3=room, 4=sensor
    int16_t rssi;
    float snr;
    uint32_t lastHeard;
    bool hasLocation;
    float latitude;
    float longitude;
};

/**
 * Message received from MeshCore
 */
struct MeshCoreMessage {
    uint32_t senderId;
    char senderName[32];
    char text[256];
    uint32_t timestamp;
    int channelIdx;     // -1 for broadcast, 0+ for channel
    uint8_t hops;
};

/**
 * Callback types for MeshCore events
 */
typedef void (*MeshCoreMessageCallback)(const MeshCoreMessage& msg);
typedef void (*MeshCoreNodeCallback)(const MeshCoreNode& node);
typedef void (*MeshCoreDeliveryCallback)(uint32_t contactId, bool delivered);

/**
 * MeshCore Adapter - Main interface for MeshCore integration
 *
 * This class manages the MeshCore mesh network and provides callbacks
 * for integration with Meshtastic's UI layer.
 */
class MeshCoreAdapter : public mesh::Mesh {
public:
    MeshCoreAdapter(mesh::Radio& radio, mesh::RNG& rng, mesh::RTCClock& rtc,
                    SimpleMeshTables& tables, mesh::PacketManager& mgr);

    /**
     * Initialize the MeshCore adapter
     * @return true if initialization successful
     */
    bool begin();

    /**
     * Process mesh events (call in main loop)
     */
    void loop();

    /**
     * Send a broadcast message
     */
    bool sendBroadcast(const char* text);

    /**
     * Send a message to a channel
     */
    bool sendToChannel(int channelIdx, const char* text);

    /**
     * Send a direct message
     */
    bool sendDirectMessage(uint32_t nodeId, const char* text);

    /**
     * Send an advertisement
     */
    void sendAdvertisement();

    /**
     * Get/set node name
     */
    const char* getNodeName() const { return _nodeName; }
    void setNodeName(const char* name);

    /**
     * Get local node ID
     */
    uint32_t getLocalNodeId() const;

    /**
     * Check for pending work (for power management)
     */
    bool hasPendingWork() const;

    /**
     * Enable/disable packet forwarding
     */
    void setForwardingEnabled(bool enabled) { _forwardingEnabled = enabled; }
    bool isForwardingEnabled() const { return _forwardingEnabled; }

    // Callbacks
    void setMessageCallback(MeshCoreMessageCallback cb) { _msgCallback = cb; }
    void setNodeCallback(MeshCoreNodeCallback cb) { _nodeCallback = cb; }
    void setDeliveryCallback(MeshCoreDeliveryCallback cb) { _deliveryCallback = cb; }

protected:
    // MeshCore virtual method overrides
    void onAdvertRecv(mesh::Packet* packet, const mesh::Identity& id,
                      uint32_t timestamp, const uint8_t* app_data, size_t app_data_len) override;

    void onAnonDataRecv(mesh::Packet* packet, const uint8_t* secret,
                        const mesh::Identity& sender, uint8_t* data, size_t len) override;

    void onAckRecv(mesh::Packet* packet, uint32_t ack_crc) override;

    int searchChannelsByHash(const uint8_t* hash, mesh::GroupChannel channels[], int max_matches) override;

    void onGroupDataRecv(mesh::Packet* packet, uint8_t type,
                         const mesh::GroupChannel& channel, uint8_t* data, size_t len) override;

    bool allowPacketForward(const mesh::Packet* packet) override;

private:
    char _nodeName[32];
    ArduinoMillisClock _msClock;
    bool _forwardingEnabled;

    // Callbacks
    MeshCoreMessageCallback _msgCallback;
    MeshCoreNodeCallback _nodeCallback;
    MeshCoreDeliveryCallback _deliveryCallback;
};

/**
 * Global MeshCore adapter instance
 * Replaces Meshtastic's 'router' global
 */
extern MeshCoreAdapter* meshCoreAdapter;

/**
 * Initialize MeshCore system
 * Call this instead of creating ReliableRouter
 * @param hal Meshtastic's LockingArduinoHal for SPI mutex protection
 */
bool initMeshCore(LockingArduinoHal* hal);

/**
 * Get the MeshCore adapter
 */
MeshCoreAdapter* getMeshCore();
