/**
 * MeshBerry SX1262 Wrapper
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
 * Custom SX1262 wrapper for RadioLib 7.x compatibility
 * Works around private member access issues in newer RadioLib versions
 */

#pragma once

#include <helpers/radiolib/CustomSX1262.h>
#include <helpers/radiolib/RadioLibWrappers.h>

/**
 * MeshBerry-specific SX1262 wrapper that avoids accessing private RadioLib members
 * Uses compile-time SF from build configuration instead of runtime access
 */
class MeshBerrySX1262Wrapper : public RadioLibWrapper {
public:
    MeshBerrySX1262Wrapper(CustomSX1262& radio, mesh::MainBoard& board)
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
        // Use compile-time SF from config instead of accessing private member
        // LORA_SF is defined in platformio.ini build flags
#ifdef LORA_SF
        return packetScoreInt(snr, LORA_SF, packet_len);
#else
        return packetScoreInt(snr, 10, packet_len);  // Default SF=10
#endif
    }

    void powerOff() override {
        ((CustomSX1262 *)_radio)->sleep(false);
    }
};
