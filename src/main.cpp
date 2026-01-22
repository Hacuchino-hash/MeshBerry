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
#include <driver/gpio.h>
#include <soc/gpio_reg.h>  // For direct GPIO register read (GPIO 0 workaround)
#include <esp_task_wdt.h>
#include "config.h"

// Board abstraction
#include "board/TDeckBoard.h"

// Drivers
#include "drivers/display.h"
#include "drivers/keyboard.h"
#include "drivers/lora.h"
#include "drivers/gps.h"
#include "drivers/audio.h"
#include "drivers/power.h"
#include "drivers/touch.h"

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
#include "settings/MessageArchive.h"

// Drivers
#include "drivers/storage.h"

// New UI system
#include "ui/Theme.h"
#include "ui/ScreenManager.h"
#include "ui/HomeScreen.h"
#include "ui/StatusScreen.h"
#include "ui/SettingsScreen.h"
#include "ui/ContactsScreen.h"
#include "ui/MessagesScreen.h"
#include "ui/ChatScreen.h"
#include "ui/ChannelsScreen.h"
#include "ui/GpsScreen.h"
#include "ui/PlaceholderScreen.h"
#include "ui/AboutScreen.h"
#include "ui/EmojiPickerScreen.h"
#include "ui/RepeaterAdminScreen.h"
#include "ui/RepeaterCLIScreen.h"
#include "ui/DMChatScreen.h"
#include "ui/DMSettingsScreen.h"
#include "ui/BootLogo.h"

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
MeshBerryMesh* theMesh = nullptr;  // Non-static - accessed by ChatScreen

// State
bool gpsPresent = false;
uint32_t gpsBaudRate = 0;  // Detected GPS baud rate (9600 or 38400)
static uint32_t lastAdvertTime = 0;
static const uint32_t ADVERT_INTERVAL_MS = 300000;  // 5 minutes
static bool rtcSyncedFromGps = false;  // Track if we've synced RTC from GPS

// RTC memory for boot loop detection (survives reboots, reset on power loss)
RTC_DATA_ATTR uint32_t bootCount = 0;
RTC_DATA_ATTR uint32_t lastBootTime = 0;
static bool timezoneSynced = false;    // Track if timezone calculated from GPS

// NOTE: Power management state (lastActivityTime, backlight, sleep) is now
// handled by the Power module's FSM. See Power::updatePowerState().

// Battery and charging monitoring
static uint32_t lastBatteryCheckTime = 0;
static const uint32_t BATTERY_CHECK_INTERVAL_MS = 10000;  // Check every 10 seconds
static uint8_t lastBatteryPercent = 100;
static uint16_t lastBatteryMV = 4200;
static bool lowBatteryAlerted = false;  // Only alert once per low battery event
static const uint8_t LOW_BATTERY_THRESHOLD = 15;  // Alert at 15%
static const uint16_t CHARGING_VOLTAGE_THRESHOLD = 4300;  // Likely charging if > 4.3V
static bool wasCharging = false;  // Track charging state for change detection

// Screen instances for new UI system
static HomeScreen homeScreen;
static StatusScreen statusScreen;
static SettingsScreen settingsScreen;
static ContactsScreen contactsScreen;
static MessagesScreen messagesScreen;
ChatScreen chatScreen;  // Non-static so MessagesScreen can access it
static ChannelsScreen channelsScreen;
static GpsScreen gpsScreen;
RepeaterAdminScreen repeaterAdminScreen;  // Non-static - accessed by ContactsScreen
RepeaterCLIScreen* repeaterCLIScreen = nullptr;  // Pointer for external access
static RepeaterCLIScreen _repeaterCLIScreen;  // Actual instance
DMChatScreen dmChatScreen;  // Non-static - accessed by ContactsScreen
DMSettingsScreen dmSettingsScreen;  // Non-static - accessed by DMChatScreen
static AboutScreen aboutScreen;
EmojiPickerScreen emojiPickerScreen;  // Non-static - accessed by ChatScreen

// CLI state
static char cmdBuffer[128] = "";
static int cmdPos = 0;

// =============================================================================
// FORWARD DECLARATIONS
// =============================================================================

void initHardware();
void initUI();
bool initRadio();
bool initMesh();
void showBootScreen();
void handleInput();
void handleSerialCLI();
void processCommand(const char* cmd);
void onMessageReceived(const Message& msg);
void onNodeDiscovered(const NodeInfo& node);
void onLoginResponse(bool success, uint8_t permissions, const char* repeaterName);
void onCLIResponse(const char* response);
void onChannelMessage(int channelIdx, const char* senderAndText, uint32_t timestamp, uint8_t hops);
void onDMReceived(uint32_t senderId, const char* senderName, const char* text, uint32_t timestamp);
void onDMDeliveryStatus(uint32_t contactId, uint32_t ack_crc, bool delivered, uint8_t attempts);
void onChannelRepeat(int channelIdx, uint32_t contentHash, uint8_t repeatCount);

// =============================================================================
// HELPER FUNCTIONS
// =============================================================================

/**
 * Convert date/time components to UNIX epoch timestamp
 * Simplified calculation (no leap seconds)
 */
uint32_t makeUnixTime(uint16_t year, uint8_t month, uint8_t day,
                      uint8_t hour, uint8_t minute, uint8_t second) {
    // Days from start of year to start of each month (non-leap year)
    static const uint16_t daysToMonth[] = {0, 31, 59, 90, 120, 151, 181, 212, 243, 273, 304, 334};

    uint32_t days = 0;

    // Count days for complete years since 1970
    for (uint16_t y = 1970; y < year; y++) {
        bool isLeap = (y % 4 == 0 && (y % 100 != 0 || y % 400 == 0));
        days += isLeap ? 366 : 365;
    }

    // Add days for complete months in current year
    if (month >= 1 && month <= 12) {
        days += daysToMonth[month - 1];
    }

    // Leap year adjustment for current year (if past February)
    bool isLeapYear = (year % 4 == 0 && (year % 100 != 0 || year % 400 == 0));
    if (month > 2 && isLeapYear) {
        days++;
    }

    // Add days (1-based, so subtract 1)
    days += day - 1;

    // Convert to seconds and add time
    return days * 86400UL + hour * 3600UL + minute * 60UL + second;
}

// =============================================================================
// SETUP
// =============================================================================

void setup() {
    // Initialize serial for debugging
    Serial.begin(115200);
    delay(1000);

    // Configure watchdog timer - disable panic on timeout to allow graceful recovery
    esp_task_wdt_init(15, false);  // 15 second timeout, no panic
    esp_task_wdt_add(NULL);  // Add current task to watchdog

    // Boot loop detection - check if we're boot looping
    bootCount++;
    uint32_t now = millis();
    uint32_t timeSinceLastBoot = now - lastBootTime;

    if (timeSinceLastBoot < 5000) {  // Boot within 5 seconds of previous boot
        Serial.printf("[BOOT] Rapid boot detected! Count: %d (interval: %lums)\n",
                      bootCount, timeSinceLastBoot);

        if (bootCount > 3) {
            Serial.println("[BOOT] BOOT LOOP DETECTED - Entering safe mode");
            Serial.println("[BOOT] Safe mode: Sleep disabled for 30 seconds");

            // Stay awake for 30 seconds to allow user intervention
            delay(30000);

            // Reset boot count after recovery period
            bootCount = 0;
            Serial.println("[BOOT] Safe mode complete - resuming normal operation");
        }
    } else {
        // Normal boot - reset counter
        bootCount = 0;
    }

    lastBootTime = now;

    // Initialize power management FIRST to detect wake reason
    Power::init();

    Serial.println();
    Serial.println("====================================");
    Serial.println("  MeshBerry v" MESHBERRY_VERSION);
    Serial.println("  Open Source T-Deck Firmware");
    Serial.println("  GPL-3.0-or-later License");
    Serial.println("  (C) 2026 NodakMesh");
    Serial.println("====================================");
    Serial.printf("  Wake reason: %s\n", Power::getWakeReasonString());
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

    // Initialize storage driver (SD card with SPIFFS fallback)
    Storage::init();
    Serial.printf("[INIT] Storage: %s (%d KB free)\n",
                  Storage::getStorageType(),
                  Storage::getAvailableSpace() / 1024);

    // Initialize message archive
    MessageArchive::init();

    // Initialize hardware
    initHardware();

    // Initialize new UI system
    initUI();

    // Initialize radio and mesh
    if (initRadio()) {
        if (initMesh()) {
            Serial.println("[INIT] Mesh network ready");

            // Update status bar with mesh info
            Screens.setNodeName(theMesh->getNodeName());

            // Set mesh instance for repeater admin (must be after initMesh)
            repeaterAdminScreen.setMesh(theMesh);
        }
    }

    // Update status bar with initial state
    Screens.setBatteryPercent(board.getBatteryPercent());
    Screens.setGpsStatus(gpsPresent, false);

    // Initialize power FSM activity timer (handled by Power::init())

    // Navigate to home screen
    Screens.navigateTo(ScreenId::HOME, false);

    // CRITICAL: Reset strapping pin GPIOs one final time AFTER all initialization
    // The RTC GPIO state can persist across soft resets and may have been corrupted
    // by previous sleep wake configuration. Resetting here ensures they're clean
    // before the main loop starts reading them.
    // GPIO 0 = trackball click (BOOT strapping pin)
    // GPIO 1 = trackball left (UART strapping pin)
    gpio_reset_pin((gpio_num_t)PIN_TRACKBALL_CLICK);
    gpio_reset_pin((gpio_num_t)PIN_TRACKBALL_LEFT);
    pinMode(PIN_TRACKBALL_CLICK, INPUT_PULLUP);
    pinMode(PIN_TRACKBALL_LEFT, INPUT_PULLUP);
    delay(5);  // Allow pullups to stabilize
    Serial.printf("[INIT] Final GPIO reset - click(0)=%d left(1)=%d (should be 1)\n",
                  digitalRead(PIN_TRACKBALL_CLICK), digitalRead(PIN_TRACKBALL_LEFT));

    Serial.println();
    Serial.println("[BOOT] MeshBerry ready!");
    Serial.println();
}

// =============================================================================
// MAIN LOOP
// =============================================================================

void loop() {
    // Heap monitoring - check every 10 seconds for low memory
    static uint32_t lastHeapCheck = 0;
    uint32_t now = millis();

    if (now - lastHeapCheck > 10000) {
        uint32_t freeHeap = ESP.getFreeHeap();
        uint32_t largestBlock = ESP.getMaxAllocHeap();

        if (freeHeap < 10000) {  // Less than 10KB free
            Serial.printf("[MEM] LOW HEAP WARNING: %u bytes free, largest block: %u\n",
                          freeHeap, largestBlock);
        }

        if (freeHeap < 5000) {  // Critical low memory (< 5KB)
            Serial.println("[MEM] CRITICAL LOW MEMORY - Forcing restart to prevent crash");
            Serial.flush();
            delay(1000);
            ESP.restart();
        }

        lastHeapCheck = now;
    }

    // Process mesh network
    if (theMesh) {
        theMesh->loop();

        // Periodic advertisement
        if (now - lastAdvertTime > ADVERT_INTERVAL_MS) {
            theMesh->sendAdvertisement();
            lastAdvertTime = now;
        }
    }

    // Update GPS and status bar fix indicator
    if (gpsPresent) {
        GPS::update();

        // Auto-sync RTC from GPS (one-time, when GPS gets time lock)
        DeviceSettings& deviceSettings = SettingsManager::getDeviceSettings();
        if (!rtcSyncedFromGps && deviceSettings.gpsRtcSyncEnabled &&
            GPS::hasValidTime() && GPS::hasValidDate()) {
            uint8_t hour, minute, second;
            uint8_t day, month;
            uint16_t year;

            if (GPS::getTime(&hour, &minute, &second) && GPS::getDate(&day, &month, &year)) {
                // Sanity check: GPS may report "valid" before having real data
                // Real GPS time should be year >= 2024
                if (year >= 2024 && month >= 1 && month <= 12 && day >= 1 && day <= 31) {
                    uint32_t gpsEpoch = makeUnixTime(year, month, day, hour, minute, second);
                    rtcClock.setCurrentTime(gpsEpoch);
                    rtcSyncedFromGps = true;
                    Serial.printf("[RTC] Synced from GPS: %u (UTC %04d-%02d-%02d %02d:%02d:%02d)\n",
                                  gpsEpoch, year, month, day, hour, minute, second);

                    // Disable GPS after sync if user doesn't want it always on
                    if (!deviceSettings.gpsEnabled) {
                        GPS::disable();
                        Serial.println("[GPS] Disabled after RTC sync (power save)");
                    }
                }
            }
        }

        // Update status bar GPS fix status every second
        static uint32_t lastGpsUiUpdate = 0;
        if (millis() - lastGpsUiUpdate > 1000) {
            // Show GPS status: present and enabled, fix status based on whether we have a fix
            Screens.setGpsStatus(GPS::isEnabled(), GPS::isEnabled() && GPS::hasFix());
            lastGpsUiUpdate = millis();
        }

        // Calculate timezone from GPS longitude (one-time, when we get valid coordinates)
        if (!timezoneSynced && GPS::hasFix()) {
            double longitude = GPS::getLongitude();
            // Simple timezone calculation: longitude / 15 = hours offset from UTC
            int8_t tzOffset = (int8_t)round(longitude / 15.0);
            Screens.setTimezoneOffset(tzOffset);
            timezoneSynced = true;
            Serial.printf("[RTC] Timezone set to UTC%+d from GPS longitude %.2f\n", tzOffset, longitude);
        }
    }

    // Update status bar time from RTC (only when minute changes to reduce redraws)
    static uint32_t lastMinute = 0;
    uint32_t currentMinute = rtcClock.getCurrentTime() / 60;
    if (currentMinute != lastMinute) {
        Screens.setTime(rtcClock.getCurrentTime());
        lastMinute = currentMinute;
    }

    // Battery monitoring and audio alerts
    if (millis() - lastBatteryCheckTime > BATTERY_CHECK_INTERVAL_MS) {
        lastBatteryCheckTime = millis();

        uint8_t batteryPercent = board.getBatteryPercent();
        uint16_t batteryMV = board.getBattMilliVolts();

        // Update status bar
        Screens.setBatteryPercent(batteryPercent);

        // Detect charging state (voltage > 4.3V typically means USB power connected)
        bool isCharging = (batteryMV > CHARGING_VOLTAGE_THRESHOLD);

        // Get audio settings for tone selection
        DeviceSettings& audioSettings = SettingsManager::getDeviceSettings();

        // Note: Removed charging tones - they cause wake/beep loops
        // when device wakes from sleep while plugged in
        if (isCharging && !wasCharging) {
            Serial.println("[POWER] Charger connected");
        }
        if (isCharging && batteryPercent >= 99 && lastBatteryPercent < 99) {
            Serial.println("[POWER] Charging complete");
        }

        // Low battery alert (only once per low battery event)
        if (batteryPercent <= LOW_BATTERY_THRESHOLD && !lowBatteryAlerted && !isCharging) {
            Serial.printf("[POWER] Low battery warning: %d%%\n", batteryPercent);
            Audio::playAlertTone(audioSettings.toneLowBattery);
            lowBatteryAlerted = true;
        }

        // Reset low battery alert flag when battery recovers (charging)
        if (batteryPercent > LOW_BATTERY_THRESHOLD + 5) {
            lowBatteryAlerted = false;
        }

        lastBatteryPercent = batteryPercent;
        lastBatteryMV = batteryMV;
        wasCharging = isCharging;
    }

    // Power management - Meshtastic-style FSM handles screen timeout and sleep
    // Configuration from DeviceSettings:
    // - screenTimeoutSecs (30): seconds until screen turns off
    // - sleepTimeoutSecs (300): seconds after screen off until sleep entry
    // - sleepIntervalSecs (30): duration of each light sleep cycle
    // - minWakeSecs (10): minimum wake time after LoRa packet
    // - wakeOnLoRa: enable LoRa wake source
    DeviceSettings& devicePower = SettingsManager::getDeviceSettings();

    // Default screen timeout to 30 seconds if not configured
    uint32_t screenTimeout = 30;

    // sleepTimeoutSecs = 0 disables sleep
    Power::updatePowerState(
        screenTimeout,
        devicePower.sleepTimeoutSecs,
        30,  // sleepIntervalSecs - 30 second sleep cycles (Meshtastic default)
        10,  // minWakeSecs - 10 seconds min wake for LoRa processing
        devicePower.wakeOnLoRa
    );

    // Handle serial CLI commands
    handleSerialCLI();

    // Handle keyboard/trackball input
    handleInput();

    // Update UI system (handles redraws)
    Screens.update();

    // Feed watchdog to prevent timeout during normal operation
    esp_task_wdt_reset();

    // Yield to other tasks
    delay(1);
}

// =============================================================================
// HARDWARE INITIALIZATION
// =============================================================================

void initHardware() {
    Serial.println("[INIT] Starting hardware initialization...");

    // FIRST: Reset GPIO 0 in case RTC domain corrupted it from previous sleep
    // GPIO 0 is the ESP32-S3 BOOT strapping pin and using gpio_wakeup_enable()
    // on it corrupts its normal input function. This clears any bad state.
    gpio_reset_pin((gpio_num_t)PIN_TRACKBALL_CLICK);
    Serial.println("[INIT] GPIO 0 reset (clear RTC state)");

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

    // Give keyboard controller extra time to fully initialize
    // ESP32-C3 may need a moment after I2C bus starts
    delay(150);

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

    // GPIO 0 is the ESP32-S3 BOOT strapping pin
    // Use simple Arduino API - avoid ESP-IDF gpio_config which may cause latch-up
    pinMode(PIN_TRACKBALL_CLICK, INPUT_PULLUP);
    delay(10);

    Serial.printf("[INIT] Trackball click GPIO 0 state: %d\n", digitalRead(PIN_TRACKBALL_CLICK));
    Serial.println("[INIT] Trackball: OK");

    // Initialize touch screen
    if (Touch::init()) {
        Serial.println("[INIT] Touch: OK");
    } else {
        Serial.println("[INIT] Touch: Not detected (or failed)");
    }

    // Initialize audio driver
    if (Audio::initSpeaker()) {
        Serial.println("[INIT] Audio: OK");

        // Sync audio settings from saved preferences
        DeviceSettings& audioSettings = SettingsManager::getDeviceSettings();
        Audio::setVolume(audioSettings.audioVolume);
        if (audioSettings.audioMuted) {
            Audio::mute();
        }
        Serial.printf("[INIT] Audio volume: %d%%, muted: %s\n",
                      audioSettings.audioVolume, audioSettings.audioMuted ? "yes" : "no");
    } else {
        Serial.println("[INIT] Audio: FAILED");
    }

    // Detect GPS (T-Deck Plus has two GPS variants with different baud rates)
    // Try both L76K (9600) and M10Q (38400) baud rates
    Serial.println("[INIT] Detecting GPS...");

    const uint32_t baudRates[] = { GPS_BAUD_L76K, GPS_BAUD_M10Q };
    const char* baudNames[] = { "L76K (9600)", "M10Q (38400)" };

    for (int attempt = 0; attempt < 2 && !gpsPresent; attempt++) {
        uint32_t baud = baudRates[attempt];
        Serial.printf("[INIT] Trying GPS at %s...\n", baudNames[attempt]);

        Serial1.end();  // Close any previous connection
        delay(50);
        Serial1.begin(baud, SERIAL_8N1, PIN_GPS_RX, PIN_GPS_TX);
        delay(100);  // Give serial time to initialize

        // Clear any garbage in the buffer
        while (Serial1.available()) Serial1.read();

        unsigned long start = millis();
        int bytesRead = 0;
        char debugBuf[64];
        int debugPos = 0;

        while (millis() - start < GPS_DETECT_TIMEOUT_MS) {
            while (Serial1.available()) {
                char c = Serial1.read();
                bytesRead++;
                // Capture first 60 bytes for debug
                if (debugPos < 60) {
                    debugBuf[debugPos++] = c;
                }
                if (c == '$') {
                    // Found start of NMEA sentence - valid GPS data!
                    gpsPresent = true;
                    gpsBaudRate = baud;  // Store for GPS driver
                    break;
                }
            }
            if (gpsPresent) break;
            delay(10);
        }
        debugBuf[debugPos] = '\0';

        if (gpsPresent) {
            Serial.printf("[INIT] GPS: OK at %s (detected after %d bytes)\n", baudNames[attempt], bytesRead);
        } else if (debugPos > 0) {
            Serial.printf("[INIT] GPS: No NMEA at %s (%d bytes read)\n", baudNames[attempt], bytesRead);
            // Print debug info on last attempt
            if (attempt == 1) {
                Serial.print("[INIT] Last bytes (hex): ");
                for (int i = 0; i < debugPos && i < 16; i++) {
                    Serial.printf("%02X ", (uint8_t)debugBuf[i]);
                }
                Serial.println();
            }
        } else {
            Serial.printf("[INIT] GPS: No data at %s\n", baudNames[attempt]);
        }
    }

    if (gpsPresent) {
        // Tell GPS driver about detected GPS
        GPS::setPresent(true);
        GPS::setBaudRate(gpsBaudRate);

        // Check device settings to determine if GPS should be enabled
        DeviceSettings& deviceSettings = SettingsManager::getDeviceSettings();
        if (deviceSettings.gpsRtcSyncEnabled || deviceSettings.gpsEnabled) {
            GPS::enable();
            Serial.println("[INIT] GPS: Enabled for RTC sync");
        } else {
            Serial.println("[INIT] GPS: Disabled (user preference)");
            Serial1.end();
        }
    } else {
        Serial.println("[INIT] GPS: Not detected at any baud rate");
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
    theMesh->setChannelMessageCallback(onChannelMessage);
    theMesh->setDMCallback(onDMReceived);
    theMesh->setDeliveryCallback(onDMDeliveryStatus);
    theMesh->setRepeatCallback(onChannelRepeat);

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
    Display::clear(Theme::BG_PRIMARY);

    // Center the grayscale logo horizontally and vertically (top portion)
    int16_t logoX = (Theme::SCREEN_WIDTH - BootLogo::WIDTH) / 2;
    int16_t logoY = 8;

    // Draw the MeshBerry grayscale logo (RGB565 format)
    Display::drawRGB565(logoX, logoY, BootLogo::LOGO,
                        BootLogo::WIDTH, BootLogo::HEIGHT);

    // Title centered below logo
    int16_t textY = logoY + BootLogo::HEIGHT + 10;
    Display::drawTextCentered(0, textY, Theme::SCREEN_WIDTH, "MeshBerry", Theme::WHITE, 3);

    // Version below title
    textY += 28;
    char verStr[16];
    snprintf(verStr, sizeof(verStr), "v%s", MESHBERRY_VERSION);
    Display::drawTextCentered(0, textY, Theme::SCREEN_WIDTH, verStr, Theme::ACCENT, 2);

    // Region info from settings
    textY += 24;
    RadioSettings& settings = SettingsManager::getRadioSettings();
    char regionStr[48];
    snprintf(regionStr, sizeof(regionStr), "Region: %s (%.3f MHz)",
             settings.getRegionName(), settings.frequency);
    Display::drawTextCentered(0, textY, Theme::SCREEN_WIDTH, regionStr, Theme::TEXT_SECONDARY, 1);

    // Status at bottom
    textY += 20;
    Display::drawTextCentered(0, textY, Theme::SCREEN_WIDTH, "Initializing...", Theme::TEXT_SECONDARY, 1);
}

void initUI() {
    Serial.println("[UI] Initializing screen manager...");

    // Initialize the screen manager (also inits StatusBar and SoftKeyBar)
    Screens.init();

    // Set up the CLI screen pointer for external access
    repeaterCLIScreen = &_repeaterCLIScreen;

    // Register all screens
    Screens.registerScreen(&homeScreen);
    Screens.registerScreen(&statusScreen);
    Screens.registerScreen(&messagesScreen);
    Screens.registerScreen(&chatScreen);
    Screens.registerScreen(&dmChatScreen);
    Screens.registerScreen(&dmSettingsScreen);
    Screens.registerScreen(&contactsScreen);
    Screens.registerScreen(&settingsScreen);
    Screens.registerScreen(&channelsScreen);
    Screens.registerScreen(&gpsScreen);
    Screens.registerScreen(&repeaterAdminScreen);
    Screens.registerScreen(&_repeaterCLIScreen);
    Screens.registerScreen(&aboutScreen);
    Screens.registerScreen(&emojiPickerScreen);

    // Note: repeaterAdminScreen.setMesh() is called after initMesh() in setup()

    Serial.println("[UI] Screen manager ready");
}

void handleInput() {
    // Handle keyboard
    uint8_t key = Keyboard::read();
    if (key != KEY_NONE) {
        // Notify power FSM of user activity (resets timers, wakes screen)
        Power::onUserActivity();

        char c = Keyboard::getChar(key);
        Screens.handleKey(key, c);
    }

    // Handle touch screen - Meshtastic-style state machine
    // Only fire tap on state TRANSITION (touched -> not touched)
    static bool touchedOld = false;
    static int16_t lastTouchX = 0;
    static int16_t lastTouchY = 0;

    int16_t x, y;
    bool touched = Touch::read(&x, &y);

    // Update coordinates while touching
    if (touched) {
        // Notify power FSM of user activity
        Power::onUserActivity();
        lastTouchX = x;
        lastTouchY = y;
    }

    // Only act on state TRANSITIONS
    if (touched != touchedOld) {
        if (!touched) {
            // Transition from touching to not-touching = release = fire tap
            InputData input(InputEvent::TOUCH_TAP, lastTouchX, lastTouchY);
            Screens.handleInput(input);
        }
    }
    touchedOld = touched;  // Update AFTER comparison

    // Handle trackball (polling with edge detection)
    static bool prevUp = true, prevDown = true, prevLeft = true, prevRight = true, prevClick = true;
    static uint32_t lastClickDebounce = 0;

    bool up = digitalRead(PIN_TRACKBALL_UP);
    bool down = digitalRead(PIN_TRACKBALL_DOWN);
    bool left = digitalRead(PIN_TRACKBALL_LEFT);
    bool right = digitalRead(PIN_TRACKBALL_RIGHT);

    // GPIO 0 (trackball click) is ESP32-S3's BOOT strapping pin
    // Use direct register read to bypass Arduino's digitalRead() which may have issues
    // with strapping pins. REG_READ(GPIO_IN_REG) reads the raw GPIO input register.
    bool click = (REG_READ(GPIO_IN_REG) & BIT(PIN_TRACKBALL_CLICK)) != 0;

    // Edge detection (active LOW - edge is when going from HIGH to LOW)
    bool upEdge = !up && prevUp;
    bool downEdge = !down && prevDown;
    bool leftEdge = !left && prevLeft;
    bool rightEdge = !right && prevRight;

    // Click edge with debouncing (50ms)
    bool clickEdge = false;
    if (!click && prevClick && (millis() - lastClickDebounce > 50)) {
        clickEdge = true;
        lastClickDebounce = millis();
    }

    if (upEdge || downEdge || leftEdge || rightEdge || clickEdge) {
        // Notify power FSM of user activity
        Power::onUserActivity();

        Screens.handleTrackball(upEdge, downEdge, leftEdge, rightEdge, clickEdge);
    }

    prevUp = up;
    prevDown = down;
    prevLeft = left;
    prevRight = right;
    prevClick = click;
}

// =============================================================================
// MESH CALLBACKS
// =============================================================================

void onMessageReceived(const Message& msg) {
    Serial.printf("[MSG] From %08X: %s\n", msg.senderId, msg.text);

    // Direct messages (non-channel) are handled here
    // Note: Channel messages go through onChannelMessage callback instead

    // Play configurable notification sound
    DeviceSettings& settings = SettingsManager::getDeviceSettings();
    Audio::playAlertTone(settings.toneMessage);

    // Show notification in status bar
    Screens.showStatus("New message!", 3000);

    // Update notification count and badge
    int unread = MessagesScreen::getUnreadCount();
    Screens.setNotificationCount(unread);
    homeScreen.setBadge(HOME_MESSAGES, unread);
}

void onChannelMessage(int channelIdx, const char* senderAndText, uint32_t timestamp, uint8_t hops) {
    Serial.printf("[CHANNEL] Ch%d (hops=%d): %s\n", channelIdx, hops, senderAndText);

    // Route to MessagesScreen (handles conversation tracking AND forwards to ChatScreen with hops)
    MessagesScreen::onChannelMessage(channelIdx, senderAndText, timestamp, hops);

    // Play configurable notification sound
    DeviceSettings& settings = SettingsManager::getDeviceSettings();
    Audio::playAlertTone(settings.toneMessage);

    // Show notification in status bar
    Screens.showStatus("New message!", 3000);

    // Update notification count and badge
    int unread = MessagesScreen::getUnreadCount();
    Screens.setNotificationCount(unread);
    homeScreen.setBadge(HOME_MESSAGES, unread);
}

void onNodeDiscovered(const NodeInfo& node) {
    Serial.printf("[NODE] Discovered: %s (type=%d, RSSI: %d)\n", node.name, node.type, node.rssi);

    // Play configurable connect sound
    DeviceSettings& settings = SettingsManager::getDeviceSettings();
    Audio::playAlertTone(settings.toneNodeConnect);

    // Note: Contact saving with pubKey is handled in MeshBerryMesh::onAdvertRecv
    // StatusScreen will show updated node count on next draw
}

void onLoginResponse(bool success, uint8_t permissions, const char* repeaterName) {
    Serial.printf("[REPEATER] Login %s: perms=%d, name=%s\n",
                  success ? "SUCCESS" : "FAILED", permissions, repeaterName ? repeaterName : "?");

    // Notify RepeaterAdminScreen
    if (success) {
        repeaterAdminScreen.onLoginSuccess(permissions);
        Screens.showStatus("Login successful", 2000);
    } else {
        repeaterAdminScreen.onLoginFailed();
        Screens.showStatus("Login failed", 2000);
    }
}

void onCLIResponse(const char* response) {
    Serial.printf("[REPEATER] CLI response: %s\n", response ? response : "(null)");

    // Route to CLI screen if it's the active screen
    if (Screens.getCurrentScreenId() == ScreenId::REPEATER_CLI) {
        _repeaterCLIScreen.onCLIResponse(response);
    } else {
        // Fallback to admin screen
        repeaterAdminScreen.onCLIResponse(response);
    }
}

void onDMReceived(uint32_t senderId, const char* senderName, const char* text, uint32_t timestamp) {
    Serial.printf("[DM] From %s (%08X): %s\n", senderName ? senderName : "?", senderId, text ? text : "(null)");

    if (!text) return;

    // Store in DMSettings as incoming message
    DMSettings& dms = SettingsManager::getDMSettings();
    dms.addMessage(senderId, text, false, timestamp);

    // Also persist to archive for extended history
    ArchivedMessage archived;
    archived.clear();
    archived.timestamp = timestamp;
    strncpy(archived.sender, senderName ? senderName : "Unknown", ARCHIVE_SENDER_LEN - 1);
    archived.sender[ARCHIVE_SENDER_LEN - 1] = '\0';
    strncpy(archived.text, text, ARCHIVE_TEXT_LEN - 1);
    archived.text[ARCHIVE_TEXT_LEN - 1] = '\0';
    archived.isOutgoing = 0;
    MessageArchive::saveDMMessage(senderId, archived);

    // Save DMs to persist the new message
    SettingsManager::saveDMs();

    // Play configurable notification sound
    DeviceSettings& settings = SettingsManager::getDeviceSettings();
    Audio::playAlertTone(settings.toneMessage);

    // If DMChatScreen is open for this contact, update it
    if (Screens.getCurrentScreenId() == ScreenId::DM_CHAT) {
        DMChatScreen::addToCurrentChat(senderId, text, timestamp);
    }

    // Show notification
    char notifyMsg[48];
    snprintf(notifyMsg, sizeof(notifyMsg), "DM from %s", senderName ? senderName : "Unknown");
    Screens.showStatus(notifyMsg, 3000);

    // Update notification count - count total unread DMs
    int unread = dms.getTotalUnreadCount();
    if (unread > 0) {
        Screens.setNotificationCount(unread);
    }
}

void onDMDeliveryStatus(uint32_t contactId, uint32_t ack_crc, bool delivered, uint8_t attempts) {
    Serial.printf("[DM] Delivery status: contact=%08X, ack=%08X, delivered=%d, attempts=%d\n",
                  contactId, ack_crc, delivered, attempts);

    // Update the message status in DMSettings
    DMSettings& dms = SettingsManager::getDMSettings();
    int convIdx = dms.findConversation(contactId);
    if (convIdx >= 0) {
        DMConversation* conv = dms.getConversation(convIdx);
        if (conv) {
            DMDeliveryStatus newStatus = delivered ? DM_STATUS_DELIVERED : DM_STATUS_FAILED;
            conv->updateDeliveryStatus(ack_crc, newStatus);
        }
    }

    // If DMChatScreen is open for this contact, refresh it
    if (Screens.getCurrentScreenId() == ScreenId::DM_CHAT &&
        dmChatScreen.getContactId() == contactId) {
        dmChatScreen.requestRedraw();
    }

    // Show brief notification for delivery status
    if (delivered) {
        Screens.showStatus("Message delivered", 1500);
    } else {
        Screens.showStatus("Message failed", 2000);
    }
}

void onChannelRepeat(int channelIdx, uint32_t contentHash, uint8_t repeatCount) {
    Serial.printf("[CHANNEL] Repeat heard: ch=%d, hash=%08X, count=%d\n",
                  channelIdx, contentHash, repeatCount);

    // Update the ChatScreen message with repeat count
    ChatScreen::updateRepeatCount(channelIdx, contentHash, repeatCount);
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
        Serial.println();
        Serial.println("Time Management:");
        Serial.println("  time                - Show current RTC time");
        Serial.println("  time <epoch>        - Set RTC to UNIX timestamp");
        Serial.println("  time sync           - Sync RTC from GPS");
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
    // ==========================================================================
    // TIME MANAGEMENT COMMANDS
    // ==========================================================================
    // time - Show current RTC time
    else if (strcmp(cmd, "time") == 0) {
        uint32_t epoch = rtcClock.getCurrentTime();
        Serial.printf("Current RTC time: %u (UNIX epoch)\n", epoch);
        Serial.printf("GPS sync status: %s\n", rtcSyncedFromGps ? "synced" : "not synced");
    }
    // time sync - Sync from GPS
    else if (strcmp(cmd, "time sync") == 0) {
        if (!gpsPresent) {
            Serial.println("GPS not available on this device.");
        } else if (!GPS::hasValidTime() || !GPS::hasValidDate()) {
            Serial.println("GPS does not have valid time yet. Wait for satellite lock.");
        } else {
            uint8_t hour, minute, second;
            uint8_t day, month;
            uint16_t year;

            if (GPS::getTime(&hour, &minute, &second) && GPS::getDate(&day, &month, &year)) {
                // Sanity check: GPS may report "valid" before having real data
                if (year < 2024 || month < 1 || month > 12 || day < 1 || day > 31) {
                    Serial.printf("GPS time not reliable yet (year=%d). Wait for full satellite lock.\n", year);
                } else {
                    uint32_t gpsEpoch = makeUnixTime(year, month, day, hour, minute, second);
                    rtcClock.setCurrentTime(gpsEpoch);
                    rtcSyncedFromGps = true;
                    Serial.printf("RTC synced from GPS: %u (UTC %04d-%02d-%02d %02d:%02d:%02d)\n",
                                  gpsEpoch, year, month, day, hour, minute, second);
                }
            } else {
                Serial.println("Failed to read GPS time.");
            }
        }
    }
    // time <epoch> - Set RTC to specific UNIX timestamp
    else if (strncmp(cmd, "time ", 5) == 0) {
        const char* arg = cmd + 5;
        while (*arg == ' ') arg++;

        if (strlen(arg) == 0) {
            Serial.println("Usage: time <unix_epoch>");
            Serial.println("       time sync        - Sync from GPS");
            Serial.println("Get current epoch at: https://www.unixtimestamp.com/");
        } else {
            uint32_t epoch = strtoul(arg, nullptr, 10);
            if (epoch > 1700000000) {  // Sanity check (after Nov 2023)
                rtcClock.setCurrentTime(epoch);
                Serial.printf("RTC set to: %u\n", epoch);
            } else {
                Serial.println("Invalid timestamp. Must be > 1700000000 (after Nov 2023)");
                Serial.println("Get current epoch at: https://www.unixtimestamp.com/");
            }
        }
    }
    // Unknown command
    else {
        Serial.printf("Unknown command: %s\n", cmd);
        Serial.println("Type 'help' for available commands.");
    }
}
