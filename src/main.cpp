/**
 * MeshBerry - Open Source Firmware for LILYGO T-Deck
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
 * Main entry point for MeshBerry firmware
 * See: dev-docs/REQUIREMENTS.md for full specifications
 */

#include <Arduino.h>
#include <Wire.h>
#include <SPI.h>
#include <SPIFFS.h>
#include "config.h"

// Board abstraction
#include "board/TDeckBoard.h"

// Drivers
#include "drivers/display.h"
#include "drivers/keyboard.h"
#include "drivers/lora.h"
#include "drivers/gps.h"
#include "drivers/audio.h"

// MeshCore integration
#include <RadioLib.h>
#include <helpers/SimpleMeshTables.h>
#include <helpers/StaticPoolPacketManager.h>
#include <helpers/radiolib/CustomSX1262.h>

// Mesh application - use our own wrapper for RadioLib 7.x compatibility
#include "mesh/MeshBerrySX1262Wrapper.h"
#include "mesh/MeshBerryMesh.h"

// =============================================================================
// GLOBAL OBJECTS
// =============================================================================

// Board
TDeckBoard board;

// SPI and radio
static SPIClass radioSPI(HSPI);
static CustomSX1262* radio = nullptr;
static MeshBerrySX1262Wrapper* radioWrapper = nullptr;

// Mesh components
static ESP32RNG rng;
static ESP32RTCClock rtcClock;
static SimpleMeshTables meshTables;
static StaticPoolPacketManager packetMgr(32);  // Pool size of 32 packets
static MeshBerryMesh* theMesh = nullptr;

// State
bool gpsPresent = false;
static uint32_t lastAdvertTime = 0;
static const uint32_t ADVERT_INTERVAL_MS = 300000;  // 5 minutes

// =============================================================================
// FORWARD DECLARATIONS
// =============================================================================

void initHardware();
bool initRadio();
bool initMesh();
void showBootScreen();
void showStatusScreen();
void handleInput();
void onMessageReceived(const Message& msg);
void onNodeDiscovered(const NodeInfo& node);

// =============================================================================
// SETUP
// =============================================================================

void setup() {
    // Initialize serial for debugging
    Serial.begin(115200);
    delay(1000);

    Serial.println();
    Serial.println("====================================");
    Serial.println("  MeshBerry v" MESHBERRY_VERSION);
    Serial.println("  Open Source T-Deck Firmware");
    Serial.println("  GPL-3.0-or-later License");
    Serial.println("  (C) 2026 NodakMesh");
    Serial.println("====================================");
    Serial.println();

    // Initialize file system
    if (!SPIFFS.begin(true)) {
        Serial.println("[INIT] SPIFFS mount failed");
    } else {
        Serial.println("[INIT] SPIFFS mounted");
    }

    // Initialize hardware
    initHardware();

    // Initialize radio and theMesh
    if (initRadio()) {
        if (initMesh()) {
            Serial.println("[INIT] Mesh network ready");
        }
    }

    // Show boot complete screen
    showStatusScreen();

    Serial.println();
    Serial.println("[BOOT] MeshBerry ready!");
    Serial.println();
}

// =============================================================================
// MAIN LOOP
// =============================================================================

void loop() {
    // Process theMesh network
    if (theMesh) {
        theMesh->loop();

        // Periodic advertisement
        uint32_t now = millis();
        if (now - lastAdvertTime > ADVERT_INTERVAL_MS) {
            theMesh->sendAdvertisement();
            lastAdvertTime = now;
        }
    }

    // Handle keyboard/trackball input
    handleInput();

    // Yield to other tasks
    delay(1);
}

// =============================================================================
// HARDWARE INITIALIZATION
// =============================================================================

void initHardware() {
    Serial.println("[INIT] Starting hardware initialization...");

    // Initialize board (enables peripheral power)
    board.begin();

    // Initialize I2C for keyboard and other peripherals
    Wire.begin(PIN_KB_SDA, PIN_KB_SCL, KB_I2C_FREQ);
    Serial.println("[INIT] I2C bus initialized");

    // Initialize display
    if (Display::init()) {
        showBootScreen();
    }

    // Initialize keyboard
    Keyboard::init();

    // Initialize trackball GPIOs
    pinMode(PIN_TRACKBALL_UP, INPUT_PULLUP);
    pinMode(PIN_TRACKBALL_DOWN, INPUT_PULLUP);
    pinMode(PIN_TRACKBALL_LEFT, INPUT_PULLUP);
    pinMode(PIN_TRACKBALL_RIGHT, INPUT_PULLUP);
    pinMode(PIN_TRACKBALL_CLICK, INPUT_PULLUP);
    Serial.println("[INIT] Trackball: OK");

    // Detect GPS (T-Deck Plus)
    Serial1.begin(GPS_BAUD, SERIAL_8N1, PIN_GPS_RX, PIN_GPS_TX);
    unsigned long start = millis();
    while (millis() - start < GPS_DETECT_TIMEOUT_MS) {
        if (Serial1.available() && Serial1.read() == '$') {
            gpsPresent = true;
            break;
        }
    }
    if (gpsPresent) {
        Serial.println("[INIT] GPS: OK (T-Deck Plus detected)");
    } else {
        Serial.println("[INIT] GPS: Not present (T-Deck base model)");
        Serial1.end();
    }

    // Read battery status
    uint8_t percent = board.getBatteryPercent();
    uint16_t mv = board.getBattMilliVolts();
    Serial.printf("[INIT] Battery: %dmV (%d%%)\n", mv, percent);
}

bool initRadio() {
    Serial.println("[INIT] Initializing SX1262 radio...");

    // Initialize SPI for radio
    radioSPI.begin(PIN_LORA_SCK, PIN_LORA_MISO, PIN_LORA_MOSI);

    // Create radio instance
    Module* mod = new Module(PIN_LORA_CS, PIN_LORA_DIO1, PIN_LORA_RST, PIN_LORA_BUSY, radioSPI);
    radio = new CustomSX1262(mod);

    // Initialize radio with MeshCore-compatible settings
    if (!radio->std_init(&radioSPI)) {
        Serial.println("[INIT] Radio init FAILED");
        return false;
    }

    // Create wrapper for MeshCore (using our RadioLib 7.x compatible wrapper)
    radioWrapper = new MeshBerrySX1262Wrapper(*radio, board);
    radioWrapper->begin();

    Serial.println("[INIT] Radio: OK");
    return true;
}

bool initMesh() {
    if (!radioWrapper) return false;

    Serial.println("[INIT] Starting theMesh network...");

    rtcClock.begin();

    // Create theMesh instance
    theMesh = new MeshBerryMesh(*radioWrapper, rng, rtcClock, meshTables, packetMgr);

    // Set callbacks
    theMesh->setMessageCallback(onMessageReceived);
    theMesh->setNodeCallback(onNodeDiscovered);

    // Start theMesh
    if (!theMesh->begin()) {
        Serial.println("[INIT] Mesh start FAILED");
        return false;
    }

    // Set node name
    theMesh->setNodeName("MeshBerry");

    // Send initial advertisement
    theMesh->sendAdvertisement();
    lastAdvertTime = millis();

    Serial.println("[INIT] Mesh: OK");
    return true;
}

// =============================================================================
// UI FUNCTIONS
// =============================================================================

void showBootScreen() {
    Display::clear(COLOR_BACKGROUND);
    Display::drawText(10, 20, "MeshBerry", COLOR_ACCENT, 3);
    Display::drawText(10, 60, "v" MESHBERRY_VERSION, COLOR_TEXT, 2);
    Display::drawText(10, 90, "Initializing...", COLOR_TEXT, 1);
}

void showStatusScreen() {
    Display::clear(COLOR_BACKGROUND);
    Display::drawText(10, 10, "MeshBerry", COLOR_ACCENT, 2);

    // Battery
    uint8_t batt = board.getBatteryPercent();
    char battStr[32];
    snprintf(battStr, sizeof(battStr), "Battery: %d%%", batt);
    Display::drawText(10, 40, battStr, COLOR_TEXT, 1);

    // Radio status
    Display::drawText(10, 60, "Radio: Active", COLOR_ACCENT, 1);

    // Region
    char freqStr[32];
    snprintf(freqStr, sizeof(freqStr), "Freq: %.1f MHz", (float)LORA_FREQ);
    Display::drawText(10, 80, freqStr, COLOR_TEXT, 1);

    // GPS
    if (gpsPresent) {
        Display::drawText(10, 100, "GPS: T-Deck Plus", COLOR_ACCENT, 1);
    } else {
        Display::drawText(10, 100, "GPS: Not present", COLOR_TEXT, 1);
    }

    // Nodes
    int nodes = theMesh ? theMesh->getNodeCount() : 0;
    char nodeStr[32];
    snprintf(nodeStr, sizeof(nodeStr), "Nodes: %d", nodes);
    Display::drawText(10, 120, nodeStr, COLOR_TEXT, 1);

    // Messages
    int msgs = theMesh ? theMesh->getMessageCount() : 0;
    char msgStr[32];
    snprintf(msgStr, sizeof(msgStr), "Messages: %d", msgs);
    Display::drawText(10, 140, msgStr, COLOR_TEXT, 1);

    // Instructions
    Display::drawText(10, 180, "Press any key to continue", COLOR_WARNING, 1);
}

void handleInput() {
    // Handle keyboard
    if (Keyboard::available()) {
        uint8_t key = Keyboard::read();
        char c = Keyboard::getChar(key);

        if (c != 0) {
            Serial.printf("[INPUT] Key: '%c' (0x%02X)\n", c, key);

            // Handle special keys
            if (key == KEY_ESC) {
                showStatusScreen();
            }
        }
    }

    // Handle trackball (basic example)
    static bool prevClick = true;
    bool click = digitalRead(PIN_TRACKBALL_CLICK);
    if (!click && prevClick) {
        Serial.println("[INPUT] Trackball click");
        showStatusScreen();
    }
    prevClick = click;
}

// =============================================================================
// MESH CALLBACKS
// =============================================================================

void onMessageReceived(const Message& msg) {
    Serial.printf("[MSG] From %08X: %s\n", msg.senderId, msg.text);

    // Update display
    Display::fillRect(0, 160, Display::getWidth(), 40, COLOR_BACKGROUND);
    Display::drawText(10, 160, "New message!", COLOR_ACCENT, 1);
    Display::drawText(10, 180, msg.text, COLOR_TEXT, 1);
}

void onNodeDiscovered(const NodeInfo& node) {
    Serial.printf("[NODE] Discovered: %s (RSSI: %d)\n", node.name, node.rssi);

    // Could update node list display here
}
