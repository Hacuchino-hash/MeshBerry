/**
 * MeshBerry WiFi Driver Implementation
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2026 NodakMesh (nodakmesh.org)
 */

#include "wifi.h"
#include <WiFi.h>
#include <time.h>
#include "esp_task_wdt.h"

namespace WiFiManager {

// State tracking
static bool initialized = false;
static bool timeSynced = false;
static bool connecting = false;
static uint32_t connectStartTime = 0;
static uint32_t connectTimeoutMs = 0;

void init() {
    if (initialized) return;

    // Set WiFi mode to station (client)
    WiFi.mode(WIFI_STA);
    WiFi.setAutoConnect(false);
    WiFi.setAutoReconnect(true);

    initialized = true;
    Serial.println("WiFi: Initialized in station mode");
}

bool connect(const char* ssid, const char* password, uint32_t timeoutMs) {
    if (!initialized) {
        init();
    }

    if (!ssid || strlen(ssid) == 0) {
        Serial.println("WiFi: No SSID provided");
        return false;
    }

    // Disconnect if already connected to a different network
    if (WiFi.isConnected()) {
        if (strcmp(WiFi.SSID().c_str(), ssid) == 0) {
            Serial.println("WiFi: Already connected to this network");
            return true;
        }
        WiFi.disconnect(true);
        delay(100);
    }

    // Ensure station mode and let WiFi settle
    WiFi.mode(WIFI_STA);
    delay(100);

    // Diagnostic: scan to see what networks are visible
    Serial.println("WiFi: Scanning for networks...");
    int16_t scanCount = WiFi.scanNetworks(false, false);
    if (scanCount > 0) {
        Serial.printf("WiFi: Found %d networks:\n", scanCount);
        for (int i = 0; i < scanCount && i < 10; i++) {
            Serial.printf("  [%d] %s (%d dBm)\n", i, WiFi.SSID(i).c_str(), WiFi.RSSI(i));
        }
    } else {
        Serial.printf("WiFi: Scan found no networks (result=%d)\n", scanCount);
    }
    WiFi.scanDelete();

    Serial.printf("WiFi: Connecting to '%s'...\n", ssid);

    // Start connection
    WiFi.begin(ssid, password);

    // Wait for connection with timeout - check status properly
    uint32_t startTime = millis();
    wl_status_t status = WL_IDLE_STATUS;
    while ((millis() - startTime) < timeoutMs) {
        esp_task_wdt_reset();  // Reset watchdog
        status = WiFi.status();
        if (status == WL_CONNECTED) {
            break;
        }
        if (status == WL_CONNECT_FAILED || status == WL_NO_SSID_AVAIL) {
            Serial.printf("WiFi: Connection failed with status %d\n", status);
            break;
        }
        yield();
        delay(100);
    }

    if (status == WL_CONNECTED) {
        Serial.printf("WiFi: Connected! IP: %s, RSSI: %d dBm\n",
                      WiFi.localIP().toString().c_str(), WiFi.RSSI());
        return true;
    } else {
        Serial.printf("WiFi: Connection failed (status=%d)\n", status);
        WiFi.disconnect(true);
        return false;
    }
}

void disconnect() {
    if (WiFi.isConnected()) {
        WiFi.disconnect();
        Serial.println("WiFi: Disconnected");
    }
}

bool isConnected() {
    return WiFi.isConnected();
}

String getIP() {
    if (WiFi.isConnected()) {
        return WiFi.localIP().toString();
    }
    return "";
}

int8_t getRSSI() {
    if (WiFi.isConnected()) {
        return WiFi.RSSI();
    }
    return 0;
}

bool syncTime(const char* ntpServer, long gmtOffsetSec, int daylightOffsetSec) {
    if (!WiFi.isConnected()) {
        Serial.println("WiFi: Cannot sync NTP - not connected");
        return false;
    }

    Serial.printf("WiFi: Syncing time from %s...\n", ntpServer);

    // Configure NTP (ESP32 built-in SNTP)
    configTime(gmtOffsetSec, daylightOffsetSec, ntpServer);

    // Wait for time to be set (up to 10 seconds)
    struct tm timeinfo;
    uint32_t startTime = millis();

    while ((millis() - startTime) < 10000) {
        if (getLocalTime(&timeinfo, 100)) {
            // Check if time is valid (year >= 2024)
            if (timeinfo.tm_year >= (2024 - 1900)) {
                timeSynced = true;
                Serial.printf("WiFi: NTP sync successful: %04d-%02d-%02d %02d:%02d:%02d\n",
                              timeinfo.tm_year + 1900, timeinfo.tm_mon + 1, timeinfo.tm_mday,
                              timeinfo.tm_hour, timeinfo.tm_min, timeinfo.tm_sec);
                return true;
            }
        }
        delay(100);
    }

    Serial.println("WiFi: NTP sync timeout");
    return false;
}

bool isTimeSynced() {
    return timeSynced;
}

uint32_t getEpochTime() {
    if (!timeSynced) return 0;

    time_t now;
    time(&now);
    return (uint32_t)now;
}

void loop() {
    // Handle async connection tracking if needed
    // Currently not used as connect() is blocking

    // Could add reconnection logic here if desired
}

const char* getStatusString() {
    if (!initialized) {
        return "Not Initialized";
    }

    wl_status_t status = WiFi.status();
    switch (status) {
        case WL_CONNECTED:     return "Connected";
        case WL_NO_SSID_AVAIL: return "SSID Not Found";
        case WL_CONNECT_FAILED: return "Connect Failed";
        case WL_IDLE_STATUS:   return "Idle";
        case WL_DISCONNECTED:  return "Disconnected";
        case WL_CONNECTION_LOST: return "Connection Lost";
        default:               return "Unknown";
    }
}

// =============================================================================
// NETWORK SCANNING
// =============================================================================

int16_t scanNetworks() {
    if (!initialized) {
        init();
    }

    Serial.println("WiFi: Starting network scan...");

    // Synchronous scan (blocking)
    int16_t count = WiFi.scanNetworks(false, false);  // async=false, show_hidden=false

    if (count >= 0) {
        Serial.printf("WiFi: Found %d networks\n", count);
    } else {
        Serial.printf("WiFi: Scan failed with error %d\n", count);
    }

    return count;
}

int16_t getScanResultCount() {
    return WiFi.scanComplete();
}

bool getNetwork(uint8_t index, WiFiNetwork& network) {
    int16_t count = WiFi.scanComplete();
    if (count < 0 || index >= count) {
        return false;
    }

    // Copy SSID (safely)
    String ssid = WiFi.SSID(index);
    strncpy(network.ssid, ssid.c_str(), 32);
    network.ssid[32] = '\0';

    // Get signal strength
    network.rssi = WiFi.RSSI(index);

    // Get encryption type
    network.encryptionType = WiFi.encryptionType(index);

    return true;
}

void clearScanResults() {
    WiFi.scanDelete();
    Serial.println("WiFi: Scan results cleared");
}

String getSSID() {
    if (WiFi.isConnected()) {
        return WiFi.SSID();
    }
    return "";
}

} // namespace WiFiManager
