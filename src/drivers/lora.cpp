/**
 * MeshBerry LoRa Driver Implementation
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
 * Uses RadioLib (MIT) and MeshCore (MIT) for SX1262 radio control
 */

#include "lora.h"
#include <SPI.h>
#include <RadioLib.h>

// Static SPI instance for LoRa
static SPIClass loraSPI(HSPI);

// RadioLib SX1262 module instance
static Module* radioModule = nullptr;
static SX1262* radio = nullptr;

// Current radio state
static LoRaRegion_t currentRegion = LORA_REGION_US915;
static bool radioInitialized = false;
static void (*rxCallback)(LoRaPacket_t& packet) = nullptr;

// Current settings (runtime modifiable)
static float currentFrequency = 915.0f;
static float currentBandwidth = 250.0f;
static uint8_t currentSF = 10;
static uint8_t currentCR = 5;
static uint8_t currentTxPower = 22;

// IRQ flag for async receive
static volatile bool rxFlag = false;

// ISR for DIO1 interrupt
static void IRAM_ATTR onDio1Rise() {
    rxFlag = true;
}

namespace LoRa {

bool init(LoRaRegion_t region) {
    if (radioInitialized) {
        return true;
    }

    Serial.println("[LORA] Initializing SX1262...");

    // Initialize SPI for LoRa radio
    loraSPI.begin(PIN_LORA_SCK, PIN_LORA_MISO, PIN_LORA_MOSI);

    // Create RadioLib module
    radioModule = new Module(PIN_LORA_CS, PIN_LORA_DIO1, PIN_LORA_RST, PIN_LORA_BUSY, loraSPI);
    radio = new SX1262(radioModule);

    // Get frequency for region
    float freq = getFrequency();
    if (region != LORA_REGION_US915) {
        currentRegion = region;
        freq = getFrequency();
    }

    // Initialize radio with MeshCore-compatible settings
    // Using TCXO voltage 1.8V as per MeshCore T-Deck config
    int state = radio->begin(
        freq,                           // Frequency
        (float)LORA_BW,                 // Bandwidth (kHz)
        LORA_SF,                        // Spreading factor
        LORA_CR,                        // Coding rate
        RADIOLIB_SX126X_SYNC_WORD_PRIVATE, // Sync word
        LORA_TX_POWER,                  // TX power (dBm)
        LORA_PREAMBLE_LEN,              // Preamble length
        SX126X_DIO3_TCXO_VOLTAGE        // TCXO voltage
    );

    if (state == RADIOLIB_ERR_SPI_CMD_FAILED || state == RADIOLIB_ERR_SPI_CMD_INVALID) {
        // Try again without TCXO (some boards don't have it)
        Serial.println("[LORA] TCXO init failed, trying without...");
        state = radio->begin(
            freq, (float)LORA_BW, LORA_SF, LORA_CR,
            RADIOLIB_SX126X_SYNC_WORD_PRIVATE, LORA_TX_POWER, LORA_PREAMBLE_LEN, 0.0f
        );
    }

    if (state != RADIOLIB_ERR_NONE) {
        Serial.printf("[LORA] Init failed with error: %d\n", state);
        return false;
    }

    // Apply MeshCore-recommended settings
    radio->setCRC(1);
    radio->setCurrentLimit(SX126X_CURRENT_LIMIT);
    radio->setDio2AsRfSwitch(SX126X_DIO2_AS_RF_SWITCH);
    radio->setRxBoostedGainMode(SX126X_RX_BOOSTED_GAIN);

    // Set up DIO1 interrupt for receive
    radio->setPacketReceivedAction(onDio1Rise);

    // Start receiving
    state = radio->startReceive();
    if (state != RADIOLIB_ERR_NONE) {
        Serial.printf("[LORA] Failed to start receive: %d\n", state);
        return false;
    }

    radioInitialized = true;
    Serial.printf("[LORA] Initialized at %.1f MHz, SF%d, BW%d kHz\n",
                  freq, LORA_SF, LORA_BW);

    return true;
}

bool setRegion(LoRaRegion_t region) {
    if (!radioInitialized) return false;

    currentRegion = region;
    float freq = getFrequency();

    int state = radio->setFrequency(freq);
    if (state != RADIOLIB_ERR_NONE) {
        Serial.printf("[LORA] Failed to set frequency: %d\n", state);
        return false;
    }

    Serial.printf("[LORA] Region set to %.1f MHz\n", freq);
    return true;
}

float getFrequency() {
    switch (currentRegion) {
        case LORA_REGION_EU868:
            return 868.0f;
        case LORA_REGION_AU915:
        case LORA_REGION_US915:
            return 915.0f;
        case LORA_REGION_AS923:
            return 923.0f;
        case LORA_REGION_KR920:
            return 920.0f;
        case LORA_REGION_IN865:
            return 865.0f;
        case LORA_REGION_433:
            return 433.0f;
        default:
            return 915.0f;
    }
}

void setTxPower(int8_t dbm) {
    if (!radioInitialized) return;

    // Clamp to valid range
    if (dbm > 22) dbm = 22;
    if (dbm < -9) dbm = -9;

    radio->setOutputPower(dbm);
}

void setSpreadingFactor(uint8_t sf) {
    if (!radioInitialized) return;

    // Clamp to valid range
    if (sf < 5) sf = 5;
    if (sf > 12) sf = 12;

    radio->setSpreadingFactor(sf);
}

void setBandwidth(float bw) {
    if (!radioInitialized) return;

    radio->setBandwidth(bw);
}

bool send(const LoRaPacket_t& packet) {
    if (!radioInitialized) return false;

    // Build raw packet from structure
    uint8_t buffer[256];
    size_t len = 0;

    // Simple packet format: type(1) + hop(1) + senderId(4) + destId(4) + data(n)
    buffer[len++] = (uint8_t)packet.type;
    buffer[len++] = packet.hopCount;

    memcpy(&buffer[len], &packet.senderId, 4);
    len += 4;

    memcpy(&buffer[len], &packet.destinationId, 4);
    len += 4;

    if (packet.dataLength > 0 && packet.dataLength <= MAX_MESSAGE_LENGTH) {
        memcpy(&buffer[len], packet.data, packet.dataLength);
        len += packet.dataLength;
    }

    return sendRaw(buffer, len);
}

bool sendRaw(const uint8_t* data, size_t length) {
    if (!radioInitialized || length == 0 || length > 255) return false;

    // Stop receiving before transmit
    radio->standby();

    int state = radio->transmit(const_cast<uint8_t*>(data), length);

    // Resume receiving
    radio->startReceive();

    if (state != RADIOLIB_ERR_NONE) {
        Serial.printf("[LORA] Transmit failed: %d\n", state);
        return false;
    }

    return true;
}

bool available() {
    return rxFlag;
}

bool receive(LoRaPacket_t& packet) {
    if (!radioInitialized || !rxFlag) return false;

    rxFlag = false;

    uint8_t buffer[256];
    size_t len = radio->getPacketLength();

    if (len == 0 || len > 255) {
        radio->startReceive();
        return false;
    }

    int state = radio->readData(buffer, len);
    if (state != RADIOLIB_ERR_NONE) {
        radio->startReceive();
        return false;
    }

    // Parse packet
    if (len < 10) {
        radio->startReceive();
        return false;
    }

    size_t idx = 0;
    packet.type = (PacketType_t)buffer[idx++];
    packet.hopCount = buffer[idx++];

    memcpy(&packet.senderId, &buffer[idx], 4);
    idx += 4;

    memcpy(&packet.destinationId, &buffer[idx], 4);
    idx += 4;

    packet.dataLength = len - idx;
    if (packet.dataLength > MAX_MESSAGE_LENGTH) {
        packet.dataLength = MAX_MESSAGE_LENGTH;
    }
    memcpy(packet.data, &buffer[idx], packet.dataLength);

    // Get signal info
    packet.rssi = getRSSI();
    packet.snr = getSNR();

    // Resume receiving
    radio->startReceive();

    // Call callback if set
    if (rxCallback) {
        rxCallback(packet);
    }

    return true;
}

int16_t getRSSI() {
    if (!radioInitialized) return -120;
    return (int16_t)radio->getRSSI();
}

float getSNR() {
    if (!radioInitialized) return 0.0f;
    return radio->getSNR();
}

void sleep() {
    if (!radioInitialized) return;
    radio->sleep(false);
}

void wake() {
    if (!radioInitialized) return;
    radio->standby();
    radio->startReceive();
}

void setReceiveCallback(void (*callback)(LoRaPacket_t& packet)) {
    rxCallback = callback;
}

// =============================================================================
// RUNTIME SETTINGS FUNCTIONS
// =============================================================================

bool initWithSettings(const RadioSettings& settings) {
    if (radioInitialized) {
        return applySettings(settings);
    }

    Serial.println("[LORA] Initializing SX1262 with settings...");

    // Initialize SPI for LoRa radio
    loraSPI.begin(PIN_LORA_SCK, PIN_LORA_MISO, PIN_LORA_MOSI);

    // Create RadioLib module
    radioModule = new Module(PIN_LORA_CS, PIN_LORA_DIO1, PIN_LORA_RST, PIN_LORA_BUSY, loraSPI);
    radio = new SX1262(radioModule);

    // Store settings
    currentFrequency = settings.frequency;
    currentBandwidth = settings.bandwidth;
    currentSF = settings.spreadingFactor;
    currentCR = settings.codingRate;
    currentTxPower = settings.txPower;

    // Initialize radio
    int state = radio->begin(
        currentFrequency,
        currentBandwidth,
        currentSF,
        currentCR,
        RADIOLIB_SX126X_SYNC_WORD_PRIVATE,
        currentTxPower,
        LORA_PREAMBLE_LEN,
        SX126X_DIO3_TCXO_VOLTAGE
    );

    if (state == RADIOLIB_ERR_SPI_CMD_FAILED || state == RADIOLIB_ERR_SPI_CMD_INVALID) {
        Serial.println("[LORA] TCXO init failed, trying without...");
        state = radio->begin(
            currentFrequency, currentBandwidth, currentSF, currentCR,
            RADIOLIB_SX126X_SYNC_WORD_PRIVATE, currentTxPower, LORA_PREAMBLE_LEN, 0.0f
        );
    }

    if (state != RADIOLIB_ERR_NONE) {
        Serial.printf("[LORA] Init failed with error: %d\n", state);
        return false;
    }

    // Apply MeshCore-recommended settings
    radio->setCRC(1);
    radio->setCurrentLimit(SX126X_CURRENT_LIMIT);
    radio->setDio2AsRfSwitch(SX126X_DIO2_AS_RF_SWITCH);
    radio->setRxBoostedGainMode(SX126X_RX_BOOSTED_GAIN);

    // Set up DIO1 interrupt for receive
    radio->setPacketReceivedAction(onDio1Rise);

    // Start receiving
    state = radio->startReceive();
    if (state != RADIOLIB_ERR_NONE) {
        Serial.printf("[LORA] Failed to start receive: %d\n", state);
        return false;
    }

    radioInitialized = true;
    Serial.printf("[LORA] Initialized: %.3f MHz, SF%d, BW%.0f kHz, CR 4/%d, TX %d dBm\n",
                  currentFrequency, currentSF, currentBandwidth, currentCR, currentTxPower);

    return true;
}

bool applySettings(const RadioSettings& settings) {
    if (!radioInitialized) {
        return initWithSettings(settings);
    }

    Serial.println("[LORA] Applying new settings...");

    // Stop receiving during reconfiguration
    radio->standby();

    bool success = true;

    // Apply frequency
    if (settings.frequency != currentFrequency) {
        int state = radio->setFrequency(settings.frequency);
        if (state == RADIOLIB_ERR_NONE) {
            currentFrequency = settings.frequency;
        } else {
            Serial.printf("[LORA] Failed to set frequency: %d\n", state);
            success = false;
        }
    }

    // Apply bandwidth
    if (settings.bandwidth != currentBandwidth) {
        int state = radio->setBandwidth(settings.bandwidth);
        if (state == RADIOLIB_ERR_NONE) {
            currentBandwidth = settings.bandwidth;
        } else {
            Serial.printf("[LORA] Failed to set bandwidth: %d\n", state);
            success = false;
        }
    }

    // Apply spreading factor
    if (settings.spreadingFactor != currentSF) {
        int state = radio->setSpreadingFactor(settings.spreadingFactor);
        if (state == RADIOLIB_ERR_NONE) {
            currentSF = settings.spreadingFactor;
        } else {
            Serial.printf("[LORA] Failed to set SF: %d\n", state);
            success = false;
        }
    }

    // Apply coding rate
    if (settings.codingRate != currentCR) {
        int state = radio->setCodingRate(settings.codingRate);
        if (state == RADIOLIB_ERR_NONE) {
            currentCR = settings.codingRate;
        } else {
            Serial.printf("[LORA] Failed to set CR: %d\n", state);
            success = false;
        }
    }

    // Apply TX power
    if (settings.txPower != currentTxPower) {
        int state = radio->setOutputPower(settings.txPower);
        if (state == RADIOLIB_ERR_NONE) {
            currentTxPower = settings.txPower;
        } else {
            Serial.printf("[LORA] Failed to set TX power: %d\n", state);
            success = false;
        }
    }

    // Resume receiving
    radio->startReceive();

    Serial.printf("[LORA] Settings applied: %.3f MHz, SF%d, BW%.0f kHz, CR 4/%d, TX %d dBm\n",
                  currentFrequency, currentSF, currentBandwidth, currentCR, currentTxPower);

    return success;
}

bool setFrequencyMHz(float mhz) {
    if (!radioInitialized) return false;
    if (mhz < 137.0f || mhz > 1020.0f) return false;

    radio->standby();
    int state = radio->setFrequency(mhz);
    radio->startReceive();

    if (state == RADIOLIB_ERR_NONE) {
        currentFrequency = mhz;
        return true;
    }
    return false;
}

bool setCodingRate(uint8_t cr) {
    if (!radioInitialized) return false;
    if (cr < 5 || cr > 8) return false;

    radio->standby();
    int state = radio->setCodingRate(cr);
    radio->startReceive();

    if (state == RADIOLIB_ERR_NONE) {
        currentCR = cr;
        return true;
    }
    return false;
}

float getCurrentFrequency() {
    return currentFrequency;
}

float getCurrentBandwidth() {
    return currentBandwidth;
}

uint8_t getCurrentSpreadingFactor() {
    return currentSF;
}

uint8_t getCurrentCodingRate() {
    return currentCR;
}

uint8_t getCurrentTxPower() {
    return currentTxPower;
}

} // namespace LoRa
