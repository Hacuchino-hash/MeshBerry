/**
 * MeshCore Adapter Implementation
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "MeshCoreAdapter.h"
#include "RadioLibInterface.h"  // For LockingArduinoHal (handles SPI mutex internally)
#include "configuration.h"
#include <SPI.h>
#include <esp_task_wdt.h>       // For watchdog feeding

// Use Meshtastic's variant pin definitions
#include "variant.h"

// Define MeshCore-expected pin names from Meshtastic's variant
#ifndef P_LORA_SCLK
#define P_LORA_SCLK LORA_SCK
#endif
#ifndef P_LORA_MISO
#define P_LORA_MISO LORA_MISO
#endif
#ifndef P_LORA_MOSI
#define P_LORA_MOSI LORA_MOSI
#endif
#ifndef P_LORA_NSS
#define P_LORA_NSS LORA_CS
#endif
#ifndef P_LORA_DIO_1
#define P_LORA_DIO_1 LORA_DIO1
#endif
#ifndef P_LORA_RESET
#define P_LORA_RESET LORA_RESET
#endif
#ifndef P_LORA_BUSY
#define P_LORA_BUSY LORA_BUSY
#endif

// Define MeshCore-expected radio parameters (US region defaults)
#ifndef LORA_FREQ
#define LORA_FREQ 910.525
#endif
#ifndef LORA_BW
#define LORA_BW 62.5
#endif
#ifndef LORA_SF
#define LORA_SF 10
#endif
#ifndef LORA_TX_POWER
#define LORA_TX_POWER 22
#endif

#include <helpers/ESP32Board.h>

// Global adapter instance
MeshCoreAdapter* meshCoreAdapter = nullptr;

// MeshCore infrastructure
static CustomSX1262* radio = nullptr;
static MeshCoreSX1262Wrapper* radioWrapper = nullptr;
static ESP32RNG rng;
static ESP32RTCClock rtcClock;
static SimpleMeshTables meshTables;
static ESP32Board board;
static StaticPoolPacketManager packetMgr(10);  // 10 packet pool

// ============================================================================
// MeshCoreAdapter Implementation
// ============================================================================

MeshCoreAdapter::MeshCoreAdapter(mesh::Radio& radio, mesh::RNG& rng, mesh::RTCClock& rtc,
                                   SimpleMeshTables& tables, mesh::PacketManager& mgr)
    : mesh::Mesh(radio, _msClock, rng, rtc, mgr, tables)
    , _forwardingEnabled(true)
    , _msgCallback(nullptr)
    , _nodeCallback(nullptr)
    , _deliveryCallback(nullptr)
{
    _nodeName[0] = '\0';
}

bool MeshCoreAdapter::begin() {
    // Generate or load identity
    // For now, generate a new identity each boot (settings integration TBD)
    mesh::LocalIdentity identity(&rng);

    // Set default node name if not set
    if (_nodeName[0] == '\0') {
        snprintf(_nodeName, sizeof(_nodeName), "Node_%04X",
                 (uint16_t)(identity.pub_key[0] << 8 | identity.pub_key[1]));
    }

    // Set our identity
    self_id = identity;

    // Start the mesh
    Mesh::begin();

    LOG_INFO("MeshCore: Started with node name: %s\n", _nodeName);
    return true;
}

void MeshCoreAdapter::loop() {
    Mesh::loop();
}

bool MeshCoreAdapter::sendBroadcast(const char* text) {
    if (!text || strlen(text) == 0) return false;

    // Create anonymous broadcast packet
    size_t len = strlen(text);
    if (len > 250) len = 250;

    // Create an identity for anonymous sender (could reuse our identity)
    mesh::Identity anonDest;  // Empty/broadcast destination
    uint8_t secret[32] = {0}; // No encryption for anonymous broadcast

    mesh::Packet* pkt = createAnonDatagram(0x01, self_id, anonDest, secret, (const uint8_t*)text, len);
    if (pkt) {
        sendFlood(pkt);
        return true;
    }
    return false;
}

bool MeshCoreAdapter::sendToChannel(int channelIdx, const char* text) {
    LOG_WARN("MeshCore: Channel messaging not yet implemented\n");
    return false;
}

bool MeshCoreAdapter::sendDirectMessage(uint32_t nodeId, const char* text) {
    LOG_WARN("MeshCore: Direct messaging not yet implemented\n");
    return false;
}

void MeshCoreAdapter::sendAdvertisement() {
    // Create advertisement with node name
    uint8_t appData[64];
    size_t appDataLen = 0;

    // Pack node name into app data
    size_t nameLen = strlen(_nodeName);
    if (nameLen > 31) nameLen = 31;
    appData[appDataLen++] = 0x01;  // Type: chat node
    appData[appDataLen++] = nameLen;
    memcpy(&appData[appDataLen], _nodeName, nameLen);
    appDataLen += nameLen;

    mesh::Packet* pkt = createAdvert(self_id, appData, appDataLen);
    if (pkt) {
        sendFlood(pkt);
    }
}

void MeshCoreAdapter::setNodeName(const char* name) {
    if (name) {
        strncpy(_nodeName, name, sizeof(_nodeName) - 1);
        _nodeName[sizeof(_nodeName) - 1] = '\0';
    }
}

uint32_t MeshCoreAdapter::getLocalNodeId() const {
    return (self_id.pub_key[0] << 24) | (self_id.pub_key[1] << 16) |
           (self_id.pub_key[2] << 8) | self_id.pub_key[3];
}

bool MeshCoreAdapter::hasPendingWork() const {
    return false;  // TBD: Check packet queue
}

// ============================================================================
// MeshCore Callbacks
// ============================================================================

void MeshCoreAdapter::onAdvertRecv(mesh::Packet* packet, const mesh::Identity& id,
                                    uint32_t timestamp, const uint8_t* app_data, size_t app_data_len) {
    if (!_nodeCallback) return;

    MeshCoreNode node;
    memset(&node, 0, sizeof(node));

    // Extract node ID from public key
    node.id = (id.pub_key[0] << 24) | (id.pub_key[1] << 16) |
              (id.pub_key[2] << 8) | id.pub_key[3];

    // Parse app data for node name and type
    if (app_data_len > 2) {
        node.type = app_data[0];
        uint8_t nameLen = app_data[1];
        if (nameLen > 31) nameLen = 31;
        if (nameLen + 2 <= app_data_len) {
            memcpy(node.name, &app_data[2], nameLen);
            node.name[nameLen] = '\0';
        }
    }

    // Packet class has protected _rssi and _snr members - check accessor methods
    // For now use defaults
    node.rssi = -100;
    node.snr = 0;
    node.lastHeard = millis();

    _nodeCallback(node);
}

void MeshCoreAdapter::onAnonDataRecv(mesh::Packet* packet, const uint8_t* secret,
                                      const mesh::Identity& sender, uint8_t* data, size_t len) {
    if (!_msgCallback) return;

    MeshCoreMessage msg;
    memset(&msg, 0, sizeof(msg));

    // Extract sender ID
    msg.senderId = (sender.pub_key[0] << 24) | (sender.pub_key[1] << 16) |
                   (sender.pub_key[2] << 8) | sender.pub_key[3];

    strncpy(msg.senderName, "Unknown", sizeof(msg.senderName));

    // Copy message text
    if (len > sizeof(msg.text) - 1) len = sizeof(msg.text) - 1;
    memcpy(msg.text, data, len);
    msg.text[len] = '\0';

    msg.timestamp = rtcClock.getCurrentTime();
    msg.channelIdx = -1;
    msg.hops = 0;

    _msgCallback(msg);
}

void MeshCoreAdapter::onAckRecv(mesh::Packet* packet, uint32_t ack_crc) {
    // TBD: Track delivery status
}

int MeshCoreAdapter::searchChannelsByHash(const uint8_t* hash, mesh::GroupChannel channels[], int max_matches) {
    return 0;  // TBD
}

void MeshCoreAdapter::onGroupDataRecv(mesh::Packet* packet, uint8_t type,
                                       const mesh::GroupChannel& channel, uint8_t* data, size_t len) {
    // TBD
}

bool MeshCoreAdapter::allowPacketForward(const mesh::Packet* packet) {
    return _forwardingEnabled;
}

// ============================================================================
// MeshCore FreeRTOS Task
// ============================================================================

/**
 * MeshCore task handler - runs MeshCore's polling loop with spiLock protection.
 *
 * This matches Meshtastic's tft_task_handler pattern (see src/graphics/tftSetup.cpp:27-35).
 * Both tasks acquire spiLock before SPI operations, preventing bus contention.
 *
 * MeshCore uses polling-based receive (RadioLibWrapper::recvRaw calls startReceive),
 * which requires coordination with the display task that also uses SPI.
 */
// Counter for debug logging (every ~10 seconds)
static uint32_t meshCoreLoopCount = 0;

static void meshCoreTaskHandler(void* param) {
    // Add this task to watchdog monitoring (timeout is configured globally)
    esp_task_wdt_add(NULL);

    while (true) {
        // Feed the watchdog to prevent timeout resets
        esp_task_wdt_reset();

        // Debug: log every ~200 iterations (~10 seconds at 50ms interval)
        meshCoreLoopCount++;
        if (meshCoreLoopCount % 200 == 0) {
            LOG_DEBUG("MeshCore: loop count %u", meshCoreLoopCount);
        }

        // NOTE: Do NOT acquire spiLock here!
        // LockingArduinoHal::spiBeginTransaction() already acquires spiLock
        // for each SPI transaction. Acquiring it here would cause deadlock
        // since concurrency::Lock uses a non-recursive binary semaphore.

        // Run MeshCore's mesh loop (handles RX polling, TX, routing)
        if (meshCoreAdapter) {
            meshCoreAdapter->loop();
        }

        // Yield to other tasks - give display more time with the SPI bus
        // 50ms gives ~20 loop iterations per second, still adequate for RX polling
        vTaskDelay(pdMS_TO_TICKS(50));
    }
}

// ============================================================================
// Global Initialization
// ============================================================================

bool initMeshCore(LockingArduinoHal* hal) {
    LOG_INFO("MeshCore: Initializing...\n");

    // Use Meshtastic's LockingArduinoHal for SPI mutex protection
    // DO NOT call SPI.begin() - Meshtastic already initialized SPI
    // The LockingArduinoHal wraps SPI with spiLock mutex for bus sharing with display

    // Create RadioLib module using the LockingArduinoHal
    Module* mod = new Module(hal, LORA_CS, LORA_DIO1, LORA_RESET, SX126X_BUSY);
    radio = new CustomSX1262(mod);

    // Get TCXO voltage from variant.h (T-Deck uses 1.8V TCXO)
#ifdef SX126X_DIO3_TCXO_VOLTAGE
    float tcxoVoltage = SX126X_DIO3_TCXO_VOLTAGE;
#else
    float tcxoVoltage = 0.0f;
#endif

    LOG_INFO("MeshCore: Using TCXO voltage: %.1fV\n", tcxoVoltage);

    // Initialize radio with parameters
    int16_t result = radio->begin(
        LORA_FREQ,      // Frequency MHz
        LORA_BW,        // Bandwidth kHz
        LORA_SF,        // Spreading factor
        5,              // Coding rate
        RADIOLIB_SX126X_SYNC_WORD_PRIVATE,
        LORA_TX_POWER,  // TX power dBm
        16,             // Preamble length
        tcxoVoltage     // TCXO voltage from variant
    );

    if (result != RADIOLIB_ERR_NONE) {
        LOG_ERROR("MeshCore: Radio init failed: %d\n", result);
        return false;
    }

    // Configure radio hardware
    radio->setDio2AsRfSwitch(true);
    radio->setCurrentLimit(140.0);
    radio->setRxBoostedGainMode(true);

    // Initialize board
    board.begin();

    // Create MeshCore radio wrapper
    radioWrapper = new MeshCoreSX1262Wrapper(*radio, board);
    radioWrapper->begin();

    // Create the MeshCore adapter
    meshCoreAdapter = new MeshCoreAdapter(*radioWrapper, rng, rtcClock, meshTables, packetMgr);

    // Initialize the adapter
    if (!meshCoreAdapter->begin()) {
        LOG_ERROR("MeshCore: Adapter init failed\n");
        return false;
    }

    // Start MeshCore polling task with spiLock protection
    // Matches tftSetup.cpp pattern: xTaskCreatePinnedToCore(tft_task_handler, "tft", 10240, NULL, 1, NULL, 0);
    // Note: Using priority 0 (lower than tft_task's 1) to let display have priority
    // Stack increased to 8192 to prevent stack overflow crashes
    xTaskCreatePinnedToCore(
        meshCoreTaskHandler,
        "MeshCore",         // Task name
        8192,               // Stack size (increased from 4096 to prevent overflow)
        nullptr,            // Parameter
        0,                  // Priority LOWER than tft_task (0 vs 1) - display gets priority
        nullptr,            // Task handle (not needed)
        1                   // Core 1 (different from tft_task to reduce contention)
    );

    LOG_INFO("MeshCore: Task started on Core 1 (priority 0)\n");
    LOG_INFO("MeshCore: Initialization complete\n");
    return true;
}

MeshCoreAdapter* getMeshCore() {
    return meshCoreAdapter;
}
