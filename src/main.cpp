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

// Settings
#include "settings/RadioSettings.h"
#include "settings/ChannelSettings.h"
#include "settings/SettingsManager.h"

// UI
#include "ui/ChannelUI.h"
#include "ui/MessagingUI.h"
#include "ui/ContactUI.h"

// =============================================================================
// GLOBAL OBJECTS
// =============================================================================

// Board
TDeckBoard board;

// SPI and radio - use pointers to avoid static initialization conflicts with display
static SPIClass* radioSPI = nullptr;
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

// Input test state
static char inputBuffer[64] = "";
static int inputPos = 0;
static int trackballX = 160;  // Center of 320 width
static int trackballY = 120;  // Center of 240 height

// UI state
enum UIScreen {
    SCREEN_INPUT_TEST,
    SCREEN_STATUS,
    SCREEN_SETTINGS,
    SCREEN_CHANNELS,
    SCREEN_MESSAGES,
    SCREEN_CONTACTS
};
static UIScreen currentScreen = SCREEN_INPUT_TEST;
static int settingsMenuItem = 0;
static const int SETTINGS_MENU_ITEMS = 6;  // Region, Freq, SF, BW, CR, TX Power

// CLI state
static char cmdBuffer[128] = "";
static int cmdPos = 0;

// =============================================================================
// FORWARD DECLARATIONS
// =============================================================================

void initHardware();
bool initRadio();
bool initMesh();
void showBootScreen();
void showStatusScreen();
void showInputTestScreen();
void showSettingsScreen();
void showChannelsScreen();
void showMessagesScreen();
void showContactsScreen();
void handleInput();
void handleSerialCLI();
void processCommand(const char* cmd);
void onSendMessage(const char* text);
void onMessageReceived(const Message& msg);
void onNodeDiscovered(const NodeInfo& node);
void onLoginResponse(bool success, uint8_t permissions, const char* repeaterName);
void onCLIResponse(const char* response);

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

    // Initialize settings manager (loads from SPIFFS or uses defaults)
    SettingsManager::init();
    RadioSettings& settings = SettingsManager::getRadioSettings();
    Serial.printf("  Region: %s\n", settings.getRegionName());
    Serial.printf("  LoRa Freq: %.3f MHz\n", settings.frequency);

    // Initialize hardware
    initHardware();

    // Initialize radio and theMesh
    if (initRadio()) {
        if (initMesh()) {
            Serial.println("[INIT] Mesh network ready");
        }
    }

    // Show input test screen for keyboard/trackball testing
    showInputTestScreen();

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

    // Handle serial CLI commands
    handleSerialCLI();

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

    // I2C Scanner - debug to find devices
    Serial.println("[I2C] Scanning for devices...");
    int foundDevices = 0;
    for (uint8_t addr = 1; addr < 127; addr++) {
        Wire.beginTransmission(addr);
        if (Wire.endTransmission() == 0) {
            Serial.printf("[I2C] Device found at 0x%02X\n", addr);
            foundDevices++;
        }
    }
    Serial.printf("[I2C] Scan complete. Found %d device(s)\n", foundDevices);

    // Initialize display FIRST - before any other SPI users
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

    // Create SPI instance for radio (after display init to avoid conflicts)
    radioSPI = new SPIClass(HSPI);
    radioSPI->begin(PIN_LORA_SCK, PIN_LORA_MISO, PIN_LORA_MOSI);

    // Create radio instance
    Module* mod = new Module(PIN_LORA_CS, PIN_LORA_DIO1, PIN_LORA_RST, PIN_LORA_BUSY, *radioSPI);
    radio = new CustomSX1262(mod);

    // Initialize radio with MeshCore-compatible settings
    if (!radio->std_init(radioSPI)) {
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
    theMesh->setLoginCallback(onLoginResponse);
    theMesh->setCLIResponseCallback(onCLIResponse);

    // Pass mesh to ContactUI for admin operations
    ContactUI::setMesh(theMesh);

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

    // Show region info from settings
    RadioSettings& settings = SettingsManager::getRadioSettings();
    char regionStr[48];
    snprintf(regionStr, sizeof(regionStr), "Region: %s (%.3f MHz)",
             settings.getRegionName(), settings.frequency);
    Display::drawText(10, 90, regionStr, COLOR_TEXT, 1);

    Display::drawText(10, 110, "Initializing...", COLOR_TEXT, 1);
}

void showStatusScreen() {
    currentScreen = SCREEN_STATUS;
    Display::clear(COLOR_BACKGROUND);
    Display::drawText(10, 10, "MeshBerry Status", COLOR_ACCENT, 2);

    RadioSettings& settings = SettingsManager::getRadioSettings();

    // Battery
    uint8_t batt = board.getBatteryPercent();
    char battStr[32];
    snprintf(battStr, sizeof(battStr), "Battery: %d%%", batt);
    Display::drawText(10, 40, battStr, COLOR_TEXT, 1);

    // Radio status with current settings
    char radioStr[64];
    snprintf(radioStr, sizeof(radioStr), "LoRa: %.3f MHz SF%d BW%s",
             settings.frequency, settings.spreadingFactor, settings.getBandwidthStr());
    Display::drawText(10, 60, radioStr, COLOR_ACCENT, 1);

    // Region
    char regionStr[32];
    snprintf(regionStr, sizeof(regionStr), "Region: %s  TX: %d dBm",
             settings.getRegionName(), settings.txPower);
    Display::drawText(10, 80, regionStr, COLOR_TEXT, 1);

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

    // Active channel
    ChannelSettings& chSettings = SettingsManager::getChannelSettings();
    const ChannelEntry* activeCh = chSettings.getActiveChannel();
    char chStr[48];
    snprintf(chStr, sizeof(chStr), "Channel: %s", activeCh ? activeCh->name : "None");
    Display::drawText(10, 160, chStr, COLOR_ACCENT, 1);

    // Instructions
    Display::drawText(10, 185, "S=Sett  C=Chan  M=Msg  N=Nodes", COLOR_WARNING, 1);
    Display::drawText(10, 205, "ESC/DEL=Back", COLOR_WARNING, 1);
}

void showInputTestScreen() {
    currentScreen = SCREEN_INPUT_TEST;
    Display::clear(COLOR_BACKGROUND);
    Display::drawText(10, 10, "Input Test Mode", COLOR_ACCENT, 2);
    Display::drawText(10, 40, "Type on keyboard:", COLOR_TEXT, 1);

    // Draw input buffer area
    Display::drawRect(10, 60, 300, 30, COLOR_TEXT);
    Display::drawText(15, 68, inputBuffer, COLOR_ACCENT, 1);

    // Draw trackball position indicator
    Display::drawText(10, 100, "Trackball: Move to test", COLOR_TEXT, 1);

    // Draw cursor position
    Display::fillRect(trackballX - 5, trackballY - 5, 10, 10, COLOR_ACCENT);

    // Instructions
    Display::drawText(10, 200, "ESC=Status S=Sett C=Ch M=Msg N=Nod", COLOR_WARNING, 1);
}

void showSettingsScreen() {
    currentScreen = SCREEN_SETTINGS;
    Display::clear(COLOR_BACKGROUND);
    Display::drawText(10, 10, "Radio Settings", COLOR_ACCENT, 2);

    RadioSettings& settings = SettingsManager::getRadioSettings();

    int y = 45;
    const int lineHeight = 22;
    char buf[48];

    // Region
    snprintf(buf, sizeof(buf), "%s Region: %s",
             settingsMenuItem == 0 ? ">" : " ", settings.getRegionName());
    Display::drawText(10, y, buf, settingsMenuItem == 0 ? COLOR_ACCENT : COLOR_TEXT, 1);
    y += lineHeight;

    // Frequency
    snprintf(buf, sizeof(buf), "%s Frequency: %.3f MHz",
             settingsMenuItem == 1 ? ">" : " ", settings.frequency);
    Display::drawText(10, y, buf, settingsMenuItem == 1 ? COLOR_ACCENT : COLOR_TEXT, 1);
    y += lineHeight;

    // Spreading Factor
    snprintf(buf, sizeof(buf), "%s Spreading Factor: %d",
             settingsMenuItem == 2 ? ">" : " ", settings.spreadingFactor);
    Display::drawText(10, y, buf, settingsMenuItem == 2 ? COLOR_ACCENT : COLOR_TEXT, 1);
    y += lineHeight;

    // Bandwidth
    snprintf(buf, sizeof(buf), "%s Bandwidth: %s kHz",
             settingsMenuItem == 3 ? ">" : " ", settings.getBandwidthStr());
    Display::drawText(10, y, buf, settingsMenuItem == 3 ? COLOR_ACCENT : COLOR_TEXT, 1);
    y += lineHeight;

    // Coding Rate
    snprintf(buf, sizeof(buf), "%s Coding Rate: 4/%d",
             settingsMenuItem == 4 ? ">" : " ", settings.codingRate);
    Display::drawText(10, y, buf, settingsMenuItem == 4 ? COLOR_ACCENT : COLOR_TEXT, 1);
    y += lineHeight;

    // TX Power
    snprintf(buf, sizeof(buf), "%s TX Power: %d dBm",
             settingsMenuItem == 5 ? ">" : " ", settings.txPower);
    Display::drawText(10, y, buf, settingsMenuItem == 5 ? COLOR_ACCENT : COLOR_TEXT, 1);

    // Instructions
    Display::drawText(10, 200, "UP/DN=Nav  L/R=Edit  ESC=Save", COLOR_WARNING, 1);
}

void showChannelsScreen() {
    currentScreen = SCREEN_CHANNELS;
    ChannelUI::init();
    ChannelUI::show();
}

void showMessagesScreen() {
    currentScreen = SCREEN_MESSAGES;
    MessagingUI::init();
    MessagingUI::setSendCallback(onSendMessage);
    MessagingUI::show();
}

void showContactsScreen() {
    currentScreen = SCREEN_CONTACTS;
    ContactUI::init();
    ContactUI::show();
}

void onSendMessage(const char* text) {
    if (theMesh) {
        ChannelSettings& chSettings = SettingsManager::getChannelSettings();
        theMesh->sendToChannel(chSettings.activeChannel, text);
        Serial.printf("[MSG] Sent to channel %d: %s\n", chSettings.activeChannel, text);
    }
}

void handleInput() {
    static bool needsRedraw = false;

    // Handle keyboard - poll directly
    uint8_t key = Keyboard::read();
    if (key != KEY_NONE) {
        char c = Keyboard::getChar(key);

        Serial.printf("[INPUT] Key code: 0x%02X, char: '%c'\n", key, c ? c : '?');

        // Handle based on current screen
        if (currentScreen == SCREEN_SETTINGS) {
            // Settings screen input handling
            RadioSettings& settings = SettingsManager::getRadioSettings();

            if (key == KEY_ESC || key == KEY_BACKSPACE) {
                // Save and exit settings (Delete/Backspace acts as back)
                SettingsManager::save();
                LoRa::applySettings(settings);
                showStatusScreen();
                return;
            }
            needsRedraw = true;  // Most actions need redraw in settings
        } else if (currentScreen == SCREEN_CHANNELS) {
            // Channel screen input handling
            if (ChannelUI::handleKey(key)) {
                // Exit back to status
                showStatusScreen();
                return;
            }
            // ChannelUI handles its own redraws
        } else if (currentScreen == SCREEN_MESSAGES) {
            // Messages screen input handling
            if (MessagingUI::handleKey(key)) {
                // Exit back to status
                showStatusScreen();
                return;
            }
            // MessagingUI handles its own redraws
        } else if (currentScreen == SCREEN_CONTACTS) {
            // Contacts screen input handling
            if (ContactUI::handleKey(key)) {
                // Exit back to status
                showStatusScreen();
                return;
            }
            // ContactUI handles its own redraws
        } else {
            // Status and Input test screens
            // ESC or Backspace (when not typing) acts as navigation
            if (key == KEY_ESC || (key == KEY_BACKSPACE && currentScreen == SCREEN_STATUS)) {
                if (currentScreen == SCREEN_INPUT_TEST) {
                    showStatusScreen();
                } else {
                    showInputTestScreen();
                }
                return;
            } else if (c == 's' || c == 'S') {
                showSettingsScreen();
                return;
            } else if (c == 'c' || c == 'C') {
                showChannelsScreen();
                return;
            } else if (c == 'm' || c == 'M') {
                showMessagesScreen();
                return;
            } else if (c == 'n' || c == 'N') {
                showContactsScreen();
                return;
            } else if (currentScreen == SCREEN_INPUT_TEST) {
                if (c == '\b' && inputPos > 0) {
                    inputBuffer[--inputPos] = '\0';
                    needsRedraw = true;
                } else if (c == '\n') {
                    inputBuffer[0] = '\0';
                    inputPos = 0;
                    needsRedraw = true;
                } else if (c >= 32 && c < 127 && inputPos < 62) {
                    inputBuffer[inputPos++] = c;
                    inputBuffer[inputPos] = '\0';
                    needsRedraw = true;
                }
            }
        }
    }

    // Handle trackball movement
    static bool prevUp = true, prevDown = true, prevLeft = true, prevRight = true;
    static bool prevClick = true;
    bool up = digitalRead(PIN_TRACKBALL_UP);
    bool down = digitalRead(PIN_TRACKBALL_DOWN);
    bool left = digitalRead(PIN_TRACKBALL_LEFT);
    bool right = digitalRead(PIN_TRACKBALL_RIGHT);
    bool click = digitalRead(PIN_TRACKBALL_CLICK);

    if (currentScreen == SCREEN_CHANNELS) {
        // Channel screen trackball handling
        bool upEdge = !up && prevUp;
        bool downEdge = !down && prevDown;
        bool leftEdge = !left && prevLeft;
        bool rightEdge = !right && prevRight;
        bool clickEdge = !click && prevClick;

        ChannelUI::handleTrackball(upEdge, downEdge, leftEdge, rightEdge, clickEdge);
        // ChannelUI handles its own redraws
    } else if (currentScreen == SCREEN_MESSAGES) {
        // Messages screen trackball handling
        bool upEdge = !up && prevUp;
        bool downEdge = !down && prevDown;
        bool leftEdge = !left && prevLeft;
        bool rightEdge = !right && prevRight;
        bool clickEdge = !click && prevClick;

        MessagingUI::handleTrackball(upEdge, downEdge, leftEdge, rightEdge, clickEdge);
        // MessagingUI handles its own redraws
    } else if (currentScreen == SCREEN_CONTACTS) {
        // Contacts screen trackball handling
        bool upEdge = !up && prevUp;
        bool downEdge = !down && prevDown;
        bool leftEdge = !left && prevLeft;
        bool rightEdge = !right && prevRight;
        bool clickEdge = !click && prevClick;

        ContactUI::handleTrackball(upEdge, downEdge, leftEdge, rightEdge, clickEdge);
        // ContactUI handles its own redraws
    } else if (currentScreen == SCREEN_SETTINGS) {
        // Settings navigation
        RadioSettings& settings = SettingsManager::getRadioSettings();

        if (!up && prevUp) {
            settingsMenuItem = (settingsMenuItem - 1 + SETTINGS_MENU_ITEMS) % SETTINGS_MENU_ITEMS;
            needsRedraw = true;
        }
        if (!down && prevDown) {
            settingsMenuItem = (settingsMenuItem + 1) % SETTINGS_MENU_ITEMS;
            needsRedraw = true;
        }
        if (!left && prevLeft) {
            // Decrease value
            switch (settingsMenuItem) {
                case 0:  // Region
                    if (settings.region > 0) {
                        settings.setRegionPreset((LoRaRegion)(settings.region - 1));
                    }
                    break;
                case 1:  // Frequency
                    settings.frequency -= 0.5f;
                    if (settings.frequency < 137.0f) settings.frequency = 137.0f;
                    settings.region = REGION_CUSTOM;
                    break;
                case 2:  // SF
                    if (settings.spreadingFactor > SF_MIN) {
                        settings.spreadingFactor--;
                        settings.region = REGION_CUSTOM;
                    }
                    break;
                case 3:  // BW
                    if (settings.bandwidth == BW_500) settings.bandwidth = BW_250;
                    else if (settings.bandwidth == BW_250) settings.bandwidth = BW_125;
                    else if (settings.bandwidth == BW_125) settings.bandwidth = BW_62_5;
                    settings.region = REGION_CUSTOM;
                    break;
                case 4:  // CR
                    if (settings.codingRate > 5) {
                        settings.codingRate--;
                        settings.region = REGION_CUSTOM;
                    }
                    break;
                case 5:  // TX Power
                    if (settings.txPower > TX_POWER_MIN) {
                        settings.txPower--;
                        settings.region = REGION_CUSTOM;
                    }
                    break;
            }
            needsRedraw = true;
        }
        if (!right && prevRight) {
            // Increase value
            switch (settingsMenuItem) {
                case 0:  // Region
                    if (settings.region < REGION_CUSTOM) {
                        settings.setRegionPreset((LoRaRegion)(settings.region + 1));
                    }
                    break;
                case 1:  // Frequency
                    settings.frequency += 0.5f;
                    if (settings.frequency > 1020.0f) settings.frequency = 1020.0f;
                    settings.region = REGION_CUSTOM;
                    break;
                case 2:  // SF
                    if (settings.spreadingFactor < SF_MAX) {
                        settings.spreadingFactor++;
                        settings.region = REGION_CUSTOM;
                    }
                    break;
                case 3:  // BW
                    if (settings.bandwidth == BW_62_5) settings.bandwidth = BW_125;
                    else if (settings.bandwidth == BW_125) settings.bandwidth = BW_250;
                    else if (settings.bandwidth == BW_250) settings.bandwidth = BW_500;
                    settings.region = REGION_CUSTOM;
                    break;
                case 4:  // CR
                    if (settings.codingRate < 8) {
                        settings.codingRate++;
                        settings.region = REGION_CUSTOM;
                    }
                    break;
                case 5:  // TX Power
                    if (settings.txPower < TX_POWER_MAX) {
                        settings.txPower++;
                        settings.region = REGION_CUSTOM;
                    }
                    break;
            }
            needsRedraw = true;
        }
    } else if (currentScreen == SCREEN_INPUT_TEST) {
        // Input test trackball
        if (!up && prevUp) {
            trackballY = max(10, trackballY - 5);
            needsRedraw = true;
        }
        if (!down && prevDown) {
            trackballY = min(230, trackballY + 5);
            needsRedraw = true;
        }
        if (!left && prevLeft) {
            trackballX = max(10, trackballX - 5);
            needsRedraw = true;
        }
        if (!right && prevRight) {
            trackballX = min(310, trackballX + 5);
            needsRedraw = true;
        }
    }

    prevUp = up;
    prevDown = down;
    prevLeft = left;
    prevRight = right;

    // Handle trackball click
    if (!click && prevClick) {
        if (currentScreen == SCREEN_INPUT_TEST) {
            trackballX = 160;
            trackballY = 120;
            inputBuffer[0] = '\0';
            inputPos = 0;
            needsRedraw = true;
        }
    }
    prevClick = click;

    // Redraw if needed
    if (needsRedraw) {
        switch (currentScreen) {
            case SCREEN_INPUT_TEST: showInputTestScreen(); break;
            case SCREEN_STATUS: showStatusScreen(); break;
            case SCREEN_SETTINGS: showSettingsScreen(); break;
        }
        needsRedraw = false;
    }
}

// =============================================================================
// MESH CALLBACKS
// =============================================================================

void onMessageReceived(const Message& msg) {
    Serial.printf("[MSG] From %08X: %s\n", msg.senderId, msg.text);

    // Add to messaging UI buffer
    // Convert sender ID to a short name (first 4 hex chars)
    char senderName[16];
    snprintf(senderName, sizeof(senderName), "Node%04X", (uint16_t)(msg.senderId & 0xFFFF));

    ChannelSettings& chSettings = SettingsManager::getChannelSettings();
    MessagingUI::addIncomingMessage(senderName, msg.text, millis(), chSettings.activeChannel);

    // If on messages screen, refresh
    if (currentScreen == SCREEN_MESSAGES) {
        MessagingUI::show();
    } else {
        // Show notification on other screens
        Display::fillRect(0, 160, Display::getWidth(), 40, COLOR_BACKGROUND);
        Display::drawText(10, 160, "New message!", COLOR_ACCENT, 1);
        Display::drawText(10, 180, msg.text, COLOR_TEXT, 1);
    }
}

void onNodeDiscovered(const NodeInfo& node) {
    Serial.printf("[NODE] Discovered: %s (type=%d, RSSI: %d)\n", node.name, node.type, node.rssi);

    // Note: Contact saving with pubKey is handled in MeshBerryMesh::onAdvertRecv
    // Don't duplicate here as it would overwrite the pubKey with empty data

    // Refresh contacts screen if active
    if (currentScreen == SCREEN_CONTACTS) {
        ContactUI::show();
    }
}

void onLoginResponse(bool success, uint8_t permissions, const char* repeaterName) {
    Serial.printf("[REPEATER] Login %s: perms=%d, name=%s\n",
                  success ? "SUCCESS" : "FAILED", permissions, repeaterName ? repeaterName : "?");

    // Forward to ContactUI if it's in password input state
    if (currentScreen == SCREEN_CONTACTS) {
        ContactUI::State uiState = ContactUI::getState();
        if (uiState == ContactUI::STATE_PASSWORD_INPUT || uiState == ContactUI::STATE_ADMIN_SESSION) {
            if (success) {
                ContactUI::onLoginSuccess();
            } else {
                ContactUI::onLoginFailed();
            }
        }
    }
}

void onCLIResponse(const char* response) {
    Serial.printf("[REPEATER] CLI response: %s\n", response ? response : "(null)");

    // Forward to ContactUI if it's in admin session state
    if (currentScreen == SCREEN_CONTACTS) {
        ContactUI::State uiState = ContactUI::getState();
        if (uiState == ContactUI::STATE_ADMIN_SESSION) {
            ContactUI::onCLIResponse(response);
        }
    }
}

// =============================================================================
// SERIAL CLI
// =============================================================================

void handleSerialCLI() {
    while (Serial.available()) {
        char c = Serial.read();

        if (c == '\n' || c == '\r') {
            if (cmdPos > 0) {
                cmdBuffer[cmdPos] = '\0';
                Serial.println();  // New line after command
                processCommand(cmdBuffer);
                cmdPos = 0;
                cmdBuffer[0] = '\0';
            }
        } else if (c == '\b' || c == 127) {  // Backspace
            if (cmdPos > 0) {
                cmdPos--;
                Serial.print("\b \b");  // Erase character
            }
        } else if (cmdPos < sizeof(cmdBuffer) - 1 && c >= 32) {
            cmdBuffer[cmdPos++] = c;
            Serial.print(c);  // Echo
        }
    }
}

void processCommand(const char* cmd) {
    // Skip leading whitespace
    while (*cmd == ' ') cmd++;

    if (strlen(cmd) == 0) return;

    // help - Show commands
    if (strcmp(cmd, "help") == 0 || strcmp(cmd, "?") == 0) {
        Serial.println("MeshBerry CLI Commands:");
        Serial.println("  help              - Show this help");
        Serial.println("  status            - Show node status");
        Serial.println("  nodes             - List all known nodes");
        Serial.println("  repeaters         - List known repeaters");
        Serial.println("  advert            - Send advertisement now");
        Serial.println("  name <name>       - Set node name");
        Serial.println("  forward on|off    - Toggle packet forwarding");
        Serial.println();
        Serial.println("Repeater Management:");
        Serial.println("  login <name> <pwd>  - Login to repeater");
        Serial.println("  logout              - Disconnect from repeater");
        Serial.println("  cmd <command>       - Send CLI command to repeater");
        Serial.println("  save <name> <pwd>   - Save password for repeater");
        Serial.println("  forget <name>       - Clear saved password");
        Serial.println("  saved               - List saved credentials");
        Serial.println("  clear contacts      - Delete all contacts");
        Serial.println("  dump contacts       - Show raw contacts.json file");
    }
    // status - Show node info
    else if (strcmp(cmd, "status") == 0) {
        RadioSettings& radio = SettingsManager::getRadioSettings();
        ContactSettings& contacts = SettingsManager::getContactSettings();

        Serial.println("=== MeshBerry Status ===");

        // Node ID
        if (theMesh) {
            uint8_t hash[8];
            theMesh->self_id.copyHashTo(hash);
            Serial.printf("Node ID:    %02X%02X%02X%02X\n", hash[0], hash[1], hash[2], hash[3]);
            Serial.printf("Node Name:  %s\n", theMesh->getNodeName());
        }

        // Battery
        Serial.printf("Battery:    %d%% (%dmV)\n", board.getBatteryPercent(), board.getBattMilliVolts());

        // Radio
        Serial.printf("Frequency:  %.3f MHz\n", radio.frequency);
        Serial.printf("SF/BW/CR:   SF%d / %s kHz / 4/%d\n",
                      radio.spreadingFactor, radio.getBandwidthStr(), radio.codingRate);
        Serial.printf("TX Power:   %d dBm\n", radio.txPower);
        Serial.printf("Region:     %s\n", radio.getRegionName());

        // Mesh
        Serial.printf("Forwarding: %s\n", theMesh->isForwardingEnabled() ? "ON" : "OFF");
        Serial.printf("Nodes:      %d known\n", contacts.numContacts);
        Serial.printf("Repeaters:  %d known\n", contacts.countRepeaters());

        // GPS
        Serial.printf("GPS:        %s\n", gpsPresent ? "Present (T-Deck Plus)" : "Not present");
    }
    // nodes - List all known nodes
    else if (strcmp(cmd, "nodes") == 0) {
        ContactSettings& contacts = SettingsManager::getContactSettings();

        if (contacts.numContacts == 0) {
            Serial.println("No nodes discovered yet.");
            return;
        }

        Serial.printf("=== Known Nodes (%d) ===\n", contacts.numContacts);
        for (int i = 0; i < contacts.numContacts; i++) {
            const ContactEntry* c = contacts.getContact(i);
            if (!c) continue;

            char typeChar = '?';
            switch (c->type) {
                case NODE_TYPE_CHAT:     typeChar = 'C'; break;
                case NODE_TYPE_REPEATER: typeChar = 'R'; break;
                case NODE_TYPE_ROOM:     typeChar = 'S'; break;
                case NODE_TYPE_SENSOR:   typeChar = 'X'; break;
                default: break;
            }

            Serial.printf("[%c] %-16s  ID:%08X  RSSI:%4d  SNR:%.1f%s\n",
                          typeChar, c->name, c->id, c->lastRssi, c->lastSnr,
                          c->isFavorite ? " *" : "");
        }
    }
    // repeaters - List only repeaters
    else if (strcmp(cmd, "repeaters") == 0) {
        ContactSettings& contacts = SettingsManager::getContactSettings();
        int count = contacts.countRepeaters();

        if (count == 0) {
            Serial.println("No repeaters discovered yet.");
            return;
        }

        Serial.printf("=== Known Repeaters (%d) ===\n", count);
        for (int i = 0; i < contacts.numContacts; i++) {
            const ContactEntry* c = contacts.getContact(i);
            if (!c || c->type != NODE_TYPE_REPEATER) continue;

            Serial.printf("[R] %-16s  ID:%08X  RSSI:%4d  SNR:%.1f%s\n",
                          c->name, c->id, c->lastRssi, c->lastSnr,
                          c->isFavorite ? " *" : "");
        }
    }
    // advert - Send advertisement
    else if (strcmp(cmd, "advert") == 0) {
        if (theMesh) {
            theMesh->sendAdvertisement();
            lastAdvertTime = millis();
            Serial.println("Advertisement sent.");
        } else {
            Serial.println("Error: Mesh not initialized.");
        }
    }
    // name <name> - Set node name
    else if (strncmp(cmd, "name ", 5) == 0) {
        const char* newName = cmd + 5;
        while (*newName == ' ') newName++;  // Skip spaces

        if (strlen(newName) == 0) {
            Serial.println("Usage: name <nodename>");
            return;
        }

        if (theMesh) {
            theMesh->setNodeName(newName);
            Serial.printf("Node name set to: %s\n", newName);
            // Send new advertisement with updated name
            theMesh->sendAdvertisement();
            lastAdvertTime = millis();
        } else {
            Serial.println("Error: Mesh not initialized.");
        }
    }
    // forward on/off - Toggle forwarding
    else if (strcmp(cmd, "forward on") == 0) {
        if (theMesh) {
            theMesh->setForwardingEnabled(true);
            Serial.println("Packet forwarding ENABLED.");
        } else {
            Serial.println("Error: Mesh not initialized.");
        }
    }
    else if (strcmp(cmd, "forward off") == 0) {
        if (theMesh) {
            theMesh->setForwardingEnabled(false);
            Serial.println("Packet forwarding DISABLED.");
        } else {
            Serial.println("Error: Mesh not initialized.");
        }
    }
    else if (strncmp(cmd, "forward", 7) == 0) {
        if (theMesh) {
            Serial.printf("Packet forwarding is currently %s.\n", theMesh->isForwardingEnabled() ? "ON" : "OFF");
        }
        Serial.println("Usage: forward on|off");
    }
    // ==========================================================================
    // REPEATER MANAGEMENT COMMANDS
    // ==========================================================================
    // login <name> <password> - Login to repeater
    else if (strncmp(cmd, "login ", 6) == 0) {
        const char* args = cmd + 6;
        while (*args == ' ') args++;

        // Parse name and password
        char repeaterName[32] = "";
        char password[16] = "";

        const char* space = strchr(args, ' ');
        if (!space) {
            Serial.println("Usage: login <repeater-name> <password>");
            return;
        }

        size_t nameLen = space - args;
        if (nameLen > sizeof(repeaterName) - 1) nameLen = sizeof(repeaterName) - 1;
        strncpy(repeaterName, args, nameLen);
        repeaterName[nameLen] = '\0';

        const char* pwd = space + 1;
        while (*pwd == ' ') pwd++;
        strncpy(password, pwd, sizeof(password) - 1);
        password[sizeof(password) - 1] = '\0';

        if (strlen(repeaterName) == 0 || strlen(password) == 0) {
            Serial.println("Usage: login <repeater-name> <password>");
            return;
        }

        // Find repeater by name
        ContactSettings& contacts = SettingsManager::getContactSettings();
        int idx = contacts.findContactByName(repeaterName);
        if (idx < 0) {
            Serial.printf("Repeater '%s' not found in contacts.\n", repeaterName);
            return;
        }

        const ContactEntry* contact = contacts.getContact(idx);
        if (!contact || contact->type != NODE_TYPE_REPEATER) {
            Serial.printf("'%s' is not a repeater.\n", repeaterName);
            return;
        }

        // Check if public key is available
        bool hasPubKey = false;
        for (int i = 0; i < 32; i++) {
            if (contact->pubKey[i] != 0) {
                hasPubKey = true;
                break;
            }
        }

        if (!hasPubKey) {
            Serial.println("Repeater public key not available. Wait for advertisement.");
            return;
        }

        // Send login request
        if (theMesh) {
            if (theMesh->sendRepeaterLogin(contact->id, contact->pubKey, password)) {
                Serial.printf("Login request sent to %s...\n", contact->name);
            } else {
                Serial.println("Failed to send login request.");
            }
        }
    }
    // logout - Disconnect from repeater
    else if (strcmp(cmd, "logout") == 0) {
        if (!theMesh) {
            Serial.println("Error: Mesh not initialized.");
            return;
        }
        if (!theMesh->isRepeaterConnected()) {
            Serial.println("Not connected to any repeater.");
            return;
        }
        theMesh->disconnectRepeater();
        Serial.println("Disconnected.");
    }
    // cmd <command> - Send CLI command to repeater
    else if (strncmp(cmd, "cmd ", 4) == 0) {
        const char* command = cmd + 4;
        while (*command == ' ') command++;

        if (strlen(command) == 0) {
            Serial.println("Usage: cmd <command>");
            return;
        }

        if (!theMesh) {
            Serial.println("Error: Mesh not initialized.");
            return;
        }

        if (!theMesh->isRepeaterConnected()) {
            Serial.println("Not connected to a repeater. Use 'login' first.");
            return;
        }

        if (theMesh->sendRepeaterCommand(command)) {
            Serial.printf("Command sent: %s\n", command);
        } else {
            Serial.println("Failed to send command.");
        }
    }
    // save <name> <password> - Save password for repeater
    else if (strncmp(cmd, "save ", 5) == 0) {
        const char* args = cmd + 5;
        while (*args == ' ') args++;

        char repeaterName[32] = "";
        char password[16] = "";

        const char* space = strchr(args, ' ');
        if (!space) {
            Serial.println("Usage: save <repeater-name> <password>");
            return;
        }

        size_t nameLen = space - args;
        if (nameLen > sizeof(repeaterName) - 1) nameLen = sizeof(repeaterName) - 1;
        strncpy(repeaterName, args, nameLen);
        repeaterName[nameLen] = '\0';

        const char* pwd = space + 1;
        while (*pwd == ' ') pwd++;
        strncpy(password, pwd, sizeof(password) - 1);
        password[sizeof(password) - 1] = '\0';

        if (strlen(repeaterName) == 0 || strlen(password) == 0) {
            Serial.println("Usage: save <repeater-name> <password>");
            return;
        }

        ContactSettings& contacts = SettingsManager::getContactSettings();
        int idx = contacts.findContactByName(repeaterName);
        if (idx < 0) {
            Serial.printf("Repeater '%s' not found.\n", repeaterName);
            return;
        }

        if (contacts.savePassword(idx, password)) {
            SettingsManager::saveContacts();
            Serial.printf("Password saved for %s\n", contacts.getContact(idx)->name);
        } else {
            Serial.println("Failed to save password.");
        }
    }
    // forget <name> - Clear saved password
    else if (strncmp(cmd, "forget ", 7) == 0) {
        const char* name = cmd + 7;
        while (*name == ' ') name++;

        if (strlen(name) == 0) {
            Serial.println("Usage: forget <repeater-name>");
            return;
        }

        ContactSettings& contacts = SettingsManager::getContactSettings();
        int idx = contacts.findContactByName(name);
        if (idx < 0) {
            Serial.printf("Repeater '%s' not found.\n", name);
            return;
        }

        if (contacts.clearPassword(idx)) {
            SettingsManager::saveContacts();
            Serial.printf("Password cleared for %s\n", contacts.getContact(idx)->name);
        } else {
            Serial.println("Failed to clear password.");
        }
    }
    // saved - List saved credentials
    else if (strcmp(cmd, "saved") == 0) {
        ContactSettings& contacts = SettingsManager::getContactSettings();
        int count = 0;

        Serial.println("=== Saved Credentials ===");
        for (int i = 0; i < contacts.numContacts; i++) {
            const ContactEntry* c = contacts.getContact(i);
            if (c && c->hasSavedPassword()) {
                Serial.printf("  %s\n", c->name);
                count++;
            }
        }

        if (count == 0) {
            Serial.println("  No saved credentials.");
        } else {
            Serial.printf("Total: %d\n", count);
        }
    }
    // clear contacts - Delete all contacts
    else if (strcmp(cmd, "clear contacts") == 0) {
        ContactSettings& contacts = SettingsManager::getContactSettings();
        contacts.setDefaults();
        SettingsManager::saveContacts();
        Serial.println("All contacts cleared.");
    }
    // dump contacts - Show raw contacts.json file
    else if (strcmp(cmd, "dump contacts") == 0) {
        if (SPIFFS.exists("/contacts.json")) {
            File f = SPIFFS.open("/contacts.json", "r");
            if (f) {
                Serial.printf("[CLI] Contents of /contacts.json (%d bytes):\n", f.size());
                while (f.available()) {
                    Serial.write(f.read());
                }
                Serial.println();
                f.close();
            } else {
                Serial.println("[CLI] Failed to open /contacts.json");
            }
        } else {
            Serial.println("[CLI] /contacts.json does not exist");
        }
    }
    // Unknown command
    else {
        Serial.printf("Unknown command: %s\n", cmd);
        Serial.println("Type 'help' for available commands.");
    }
}
