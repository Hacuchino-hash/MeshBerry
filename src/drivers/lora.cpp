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
        LORA_BANDWIDTH,                 // Bandwidth (kHz)
        LORA_SPREADING_FACTOR,          // Spreading factor
        LORA_CODING_RATE,               // Coding rate
        RADIOLIB_SX126X_SYNC_WORD_PRIVATE, // Sync word
        LORA_TX_POWER,                  // TX power (dBm)
        LORA_PREAMBLE_LEN,              // Preamble length
        SX126X_DIO3_TCXO_VOLTAGE        // TCXO voltage
    );

    if (state == RADIOLIB_ERR_SPI_CMD_FAILED || state == RADIOLIB_ERR_SPI_CMD_INVALID) {
        // Try again without TCXO (some boards don't have it)
        Serial.println("[LORA] TCXO init failed, trying without...");
        state = radio->begin(
            freq, LORA_BANDWIDTH, LORA_SPREADING_FACTOR, LORA_CODING_RATE,
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
    Serial.printf("[LORA] Initialized at %.1f MHz, SF%d, BW%.0f kHz\n",
                  freq, LORA_SPREADING_FACTOR, LORA_BANDWIDTH);

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

} // namespace LoRa
