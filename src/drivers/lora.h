/**
 * MeshBerry LoRa Driver
 *
 * Interface for Semtech SX1262 LoRa transceiver with MeshCore integration
 *
 * Hardware: LILYGO T-Deck SX1262 module
 * Interface: SPI
 * Frequencies: 433/868/915 MHz (region dependent)
 *
 * See: dev-docs/REQUIREMENTS.md for specifications
 */

#ifndef MESHBERRY_LORA_H
#define MESHBERRY_LORA_H

#include <Arduino.h>
#include "../config.h"
#include "../settings/RadioSettings.h"

// =============================================================================
// LORA REGIONS (legacy enum for backwards compatibility)
// =============================================================================

typedef enum {
    LORA_REGION_US915,      // United States (915 MHz)
    LORA_REGION_EU868,      // Europe (868 MHz)
    LORA_REGION_AU915,      // Australia (915 MHz)
    LORA_REGION_AS923,      // Asia (923 MHz)
    LORA_REGION_KR920,      // Korea (920 MHz)
    LORA_REGION_IN865,      // India (865 MHz)
    LORA_REGION_433         // 433 MHz band
} LoRaRegion_t;

// =============================================================================
// PACKET TYPES
// =============================================================================

typedef enum {
    PACKET_TEXT,            // Text message
    PACKET_POSITION,        // GPS position
    PACKET_ACK,             // Acknowledgment
    PACKET_BEACON,          // Repeater beacon
    PACKET_ADMIN            // Admin/control
} PacketType_t;

// =============================================================================
// PACKET STRUCTURE
// =============================================================================

typedef struct {
    uint32_t    senderId;
    uint32_t    destinationId;
    PacketType_t type;
    uint8_t     hopCount;
    uint8_t     dataLength;
    uint8_t     data[MAX_MESSAGE_LENGTH];
    int16_t     rssi;       // Signal strength
    float       snr;        // Signal-to-noise ratio
} LoRaPacket_t;

// =============================================================================
// LORA DRIVER INTERFACE
// =============================================================================

namespace LoRa {

/**
 * Initialize the LoRa hardware
 * @param region Frequency region
 * @return true if successful
 */
bool init(LoRaRegion_t region = LORA_REGION_US915);

/**
 * Set the operating region/frequency
 */
bool setRegion(LoRaRegion_t region);

/**
 * Get current frequency in MHz
 */
float getFrequency();

/**
 * Set transmission power
 * @param dbm Power in dBm (max 22)
 */
void setTxPower(int8_t dbm);

/**
 * Set spreading factor
 * @param sf Spreading factor (7-12)
 */
void setSpreadingFactor(uint8_t sf);

/**
 * Set bandwidth
 * @param bw Bandwidth in kHz (125, 250, 500)
 */
void setBandwidth(float bw);

/**
 * Send a packet
 * @param packet Packet to send
 * @return true if transmission started
 */
bool send(const LoRaPacket_t& packet);

/**
 * Send raw data
 * @param data Data buffer
 * @param length Data length
 * @return true if transmission started
 */
bool sendRaw(const uint8_t* data, size_t length);

/**
 * Check if a packet is available
 */
bool available();

/**
 * Receive a packet
 * @param packet Buffer to store received packet
 * @return true if packet received
 */
bool receive(LoRaPacket_t& packet);

/**
 * Get last RSSI value
 */
int16_t getRSSI();

/**
 * Get last SNR value
 */
float getSNR();

/**
 * Put radio in sleep mode
 */
void sleep();

/**
 * Wake radio from sleep
 */
void wake();

/**
 * Set receive callback
 * @param callback Function to call when packet received
 */
void setReceiveCallback(void (*callback)(LoRaPacket_t& packet));

// =============================================================================
// RUNTIME SETTINGS FUNCTIONS
// =============================================================================

/**
 * Initialize with RadioSettings structure
 * @param settings Settings to apply
 * @return true if successful
 */
bool initWithSettings(const RadioSettings& settings);

/**
 * Apply new RadioSettings at runtime
 * @param settings Settings to apply
 * @return true if successful
 */
bool applySettings(const RadioSettings& settings);

/**
 * Set frequency at runtime
 * @param mhz Frequency in MHz
 * @return true if successful
 */
bool setFrequencyMHz(float mhz);

/**
 * Set coding rate
 * @param cr Coding rate (5-8, maps to 4/5 - 4/8)
 * @return true if successful
 */
bool setCodingRate(uint8_t cr);

/**
 * Get current settings
 */
float getCurrentFrequency();
float getCurrentBandwidth();
uint8_t getCurrentSpreadingFactor();
uint8_t getCurrentCodingRate();
uint8_t getCurrentTxPower();

} // namespace LoRa

#endif // MESHBERRY_LORA_H
