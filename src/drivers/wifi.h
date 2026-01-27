/**
 * MeshBerry WiFi Driver
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2026 NodakMesh (nodakmesh.org)
 *
 * This file is part of MeshBerry.
 *
 * WiFi connectivity for NTP time sync, OTA updates, and future features
 */

#ifndef MESHBERRY_WIFI_H
#define MESHBERRY_WIFI_H

#include <Arduino.h>

namespace WiFiManager {

/**
 * Initialize WiFi subsystem
 */
void init();

/**
 * Connect to a WiFi network
 * @param ssid Network SSID
 * @param password Network password
 * @param timeoutMs Connection timeout in milliseconds (default 10 seconds)
 * @return true if connected successfully
 */
bool connect(const char* ssid, const char* password, uint32_t timeoutMs = 10000);

/**
 * Disconnect from WiFi network
 */
void disconnect();

/**
 * Check if WiFi is connected
 * @return true if connected to a network
 */
bool isConnected();

/**
 * Get current IP address
 * @return IP address string or empty if not connected
 */
String getIP();

/**
 * Get signal strength
 * @return RSSI in dBm, or 0 if not connected
 */
int8_t getRSSI();

/**
 * Sync time via NTP
 * @param ntpServer NTP server hostname (default: pool.ntp.org)
 * @param gmtOffsetSec GMT offset in seconds (default: 0 for UTC)
 * @param daylightOffsetSec Daylight saving offset in seconds (default: 0)
 * @return true if time sync succeeded
 */
bool syncTime(const char* ntpServer = "pool.ntp.org",
              long gmtOffsetSec = 0,
              int daylightOffsetSec = 0);

/**
 * Check if NTP time has been synced
 * @return true if time has been synced via NTP
 */
bool isTimeSynced();

/**
 * Get NTP synced epoch time
 * @return UNIX timestamp or 0 if not synced
 */
uint32_t getEpochTime();

/**
 * Process WiFi events (call in main loop)
 */
void loop();

/**
 * Get WiFi status string for UI
 * @return Status string: "Disconnected", "Connecting...", "Connected", etc.
 */
const char* getStatusString();

// =============================================================================
// NETWORK SCANNING
// =============================================================================

/**
 * WiFi network scan result
 */
struct WiFiNetwork {
    char ssid[33];          // Network name (max 32 chars + null)
    int8_t rssi;            // Signal strength in dBm
    uint8_t encryptionType; // Security type (WIFI_AUTH_OPEN, WIFI_AUTH_WPA2_PSK, etc.)

    bool isOpen() const { return encryptionType == 0; }  // WIFI_AUTH_OPEN = 0
};

/**
 * Scan for available WiFi networks (blocking)
 * @return Number of networks found, or negative on error
 */
int16_t scanNetworks();

/**
 * Get number of networks from last scan
 * @return Network count, or -1 if scan not complete, -2 if scan failed
 */
int16_t getScanResultCount();

/**
 * Get network info at index from last scan
 * @param index Network index (0 to count-1)
 * @param network Output structure to fill
 * @return true if valid index, false otherwise
 */
bool getNetwork(uint8_t index, WiFiNetwork& network);

/**
 * Clear scan results and free memory
 */
void clearScanResults();

/**
 * Get connected network SSID
 * @return SSID string or empty if not connected
 */
String getSSID();

} // namespace WiFiManager

#endif // MESHBERRY_WIFI_H
