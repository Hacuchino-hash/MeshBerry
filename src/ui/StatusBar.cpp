/**
 * MeshBerry Status Bar Implementation
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2026 NodakMesh (nodakmesh.org)
 */

#include "StatusBar.h"
#include "Icons.h"
#include "../drivers/display.h"
#include <string.h>

namespace StatusBar {

// State
static uint8_t batteryPercent = 100;
static bool loraConnected = false;
static int16_t loraRssi = 0;
static bool gpsPresent = false;
static bool gpsFix = false;
static uint8_t notifCount = 0;
static uint32_t currentTime = 0;
static int8_t timezoneOffset = 0;  // Hours offset from UTC
static char nodeName[16] = "";
static char statusMessage[32] = "";
static uint32_t statusMessageExpiry = 0;
static uint32_t lastDrawTime = 0;
static bool forceRedraw = true;

// Previous state for partial updates
static uint8_t prevBatteryPercent = 255;
static bool prevLoraConnected = false;
static bool prevGpsFix = false;
static uint8_t prevNotifCount = 255;
static uint32_t prevTime = 0;  // Stored as minutes (currentTime / 60)

void init() {
    batteryPercent = 100;
    loraConnected = false;
    loraRssi = 0;
    gpsPresent = false;
    gpsFix = false;
    notifCount = 0;
    currentTime = 0;
    nodeName[0] = '\0';
    statusMessage[0] = '\0';
    statusMessageExpiry = 0;
    lastDrawTime = 0;
    forceRedraw = true;
}

void draw() {
    uint32_t now = millis();

    // Check if status message expired
    if (statusMessageExpiry > 0 && now > statusMessageExpiry) {
        statusMessage[0] = '\0';
        statusMessageExpiry = 0;
        forceRedraw = true;
    }

    // Throttle updates to ~1 second
    if (!forceRedraw && (now - lastDrawTime) < 1000) {
        return;
    }

    // Check what changed (compare minutes for time, not seconds)
    uint32_t currentMinute = currentTime / 60;
    bool batteryChanged = (batteryPercent != prevBatteryPercent);
    bool loraChanged = (loraConnected != prevLoraConnected);
    bool gpsChanged = (gpsFix != prevGpsFix);
    bool notifChanged = (notifCount != prevNotifCount);
    bool timeChanged = (currentMinute != prevTime);

    // Draw background if full redraw needed
    if (forceRedraw) {
        Display::fillRect(0, 0, Theme::SCREEN_WIDTH, Theme::STATUS_BAR_HEIGHT, Theme::BG_SECONDARY);
        // Bottom divider line
        Display::drawHLine(0, Theme::STATUS_BAR_HEIGHT - 1, Theme::SCREEN_WIDTH, Theme::DIVIDER);
    }

    int16_t x = 4;
    int16_t y = (Theme::STATUS_BAR_HEIGHT - 8) / 2;  // Center 8px icons vertically

    // === LEFT SIDE: Status icons ===

    // LoRa status icon
    if (forceRedraw || loraChanged) {
        uint16_t loraColor = loraConnected ? Theme::GREEN : Theme::GRAY_LIGHT;
        Display::fillRect(x, y - 1, 10, 10, Theme::BG_SECONDARY);  // Clear area
        Display::drawBitmap(x, y, Icons::LORA_ICON, 8, 8, loraColor);
        prevLoraConnected = loraConnected;
    }
    x += 12;

    // Signal strength (if connected)
    if (forceRedraw || loraChanged) {
        Display::fillRect(x, y - 1, 10, 10, Theme::BG_SECONDARY);
        if (loraConnected) {
            const uint8_t* signalIcon;
            if (loraRssi > -70) {
                signalIcon = Icons::SIGNAL_FULL;
            } else if (loraRssi > -90) {
                signalIcon = Icons::SIGNAL_MID;
            } else if (loraRssi > -110) {
                signalIcon = Icons::SIGNAL_LOW;
            } else {
                signalIcon = Icons::SIGNAL_NONE;
            }
            Display::drawBitmap(x, y, signalIcon, 8, 8, Theme::WHITE);
        }
    }
    x += 12;

    // GPS status (if present)
    if (gpsPresent && (forceRedraw || gpsChanged)) {
        Display::fillRect(x, y - 1, 10, 10, Theme::BG_SECONDARY);
        uint16_t gpsColor = gpsFix ? Theme::GREEN : Theme::GRAY_LIGHT;
        // Simple GPS indicator (just a dot for now)
        Display::fillCircle(x + 4, y + 4, 3, gpsColor);
        prevGpsFix = gpsFix;
    }
    if (gpsPresent) x += 12;

    // Notification badge
    if (notifCount > 0 && (forceRedraw || notifChanged)) {
        Display::fillRect(x, y - 1, 16, 10, Theme::BG_SECONDARY);
        Display::fillCircle(x + 4, y + 4, 4, Theme::RED);
        if (notifCount < 10) {
            char numStr[2] = { (char)('0' + notifCount), '\0' };
            Display::drawText(x + 2, y, numStr, Theme::WHITE, 1);
        } else {
            Display::drawText(x + 1, y, "+", Theme::WHITE, 1);
        }
        prevNotifCount = notifCount;
    }

    // === CENTER: Node name or status message ===
    int16_t centerX = 80;
    int16_t centerWidth = 160;

    if (forceRedraw || statusMessage[0]) {
        Display::fillRect(centerX, 0, centerWidth, Theme::STATUS_BAR_HEIGHT - 1, Theme::BG_SECONDARY);

        if (statusMessage[0]) {
            // Show temporary status message
            Display::drawTextCentered(centerX, y, centerWidth, statusMessage, Theme::YELLOW, 1);
        } else if (nodeName[0]) {
            // Show node name
            Display::drawTextCentered(centerX, y, centerWidth, nodeName, Theme::TEXT_SECONDARY, 1);
        }
    }

    // === RIGHT SIDE: Battery and time ===
    x = Theme::SCREEN_WIDTH - 4;

    // Time (HH:MM) - currentTime is Unix epoch, apply timezone offset
    if (forceRedraw || timeChanged) {
        char timeStr[6];
        // Convert Unix epoch to local time
        uint32_t secondsToday = currentTime % 86400;  // Seconds since midnight UTC
        int32_t localSeconds = (int32_t)secondsToday + (timezoneOffset * 3600);
        // Handle day wrap
        if (localSeconds < 0) localSeconds += 86400;
        if (localSeconds >= 86400) localSeconds -= 86400;
        uint32_t hours = localSeconds / 3600;
        uint32_t mins = (localSeconds % 3600) / 60;
        snprintf(timeStr, sizeof(timeStr), "%02d:%02d", (int)hours, (int)mins);

        Display::fillRect(x - 36, y - 1, 36, 10, Theme::BG_SECONDARY);
        Display::drawTextRight(x, y, timeStr, Theme::WHITE, 1);
        prevTime = currentMinute;  // Store as minutes for comparison
    }
    x -= 40;

    // Battery percentage
    if (forceRedraw || batteryChanged) {
        char battStr[5];
        snprintf(battStr, sizeof(battStr), "%d%%", batteryPercent);

        // Choose color based on level
        uint16_t battColor;
        if (batteryPercent > 50) {
            battColor = Theme::GREEN;
        } else if (batteryPercent > 20) {
            battColor = Theme::YELLOW;
        } else {
            battColor = Theme::RED;
        }

        Display::fillRect(x - 30, y - 1, 30, 10, Theme::BG_SECONDARY);
        Display::drawTextRight(x, y, battStr, battColor, 1);
        prevBatteryPercent = batteryPercent;
    }
    x -= 34;

    // Battery icon
    if (forceRedraw || batteryChanged) {
        const uint8_t* battIcon;
        if (batteryPercent > 60) {
            battIcon = Icons::BATTERY_FULL;
        } else if (batteryPercent > 30) {
            battIcon = Icons::BATTERY_MID;
        } else {
            battIcon = Icons::BATTERY_LOW;
        }

        Display::fillRect(x - 10, y - 1, 10, 10, Theme::BG_SECONDARY);
        Display::drawBitmap(x - 8, y, battIcon, 8, 8, Theme::WHITE);
    }

    lastDrawTime = now;
    forceRedraw = false;
}

void redraw() {
    forceRedraw = true;
    draw();
}

void setBatteryPercent(uint8_t percent) {
    if (percent > 100) percent = 100;
    batteryPercent = percent;
}

void setLoRaStatus(bool connected, int16_t rssi) {
    loraConnected = connected;
    loraRssi = rssi;
}

void setGpsStatus(bool hasGps, bool hasFix) {
    gpsPresent = hasGps;
    gpsFix = hasFix;
}

void setNotificationCount(uint8_t count) {
    notifCount = count;
}

void showStatus(const char* message, uint32_t durationMs) {
    if (message) {
        strlcpy(statusMessage, message, sizeof(statusMessage));
        statusMessageExpiry = millis() + durationMs;
        forceRedraw = true;
    }
}

void setTime(uint32_t epochTime) {
    currentTime = epochTime;
}

void setTimezoneOffset(int8_t hours) {
    if (hours != timezoneOffset) {
        timezoneOffset = hours;
        forceRedraw = true;  // Time display needs update
    }
}

void setNodeName(const char* name) {
    if (name) {
        strlcpy(nodeName, name, sizeof(nodeName));
    } else {
        nodeName[0] = '\0';
    }
}

bool needsUpdate() {
    // Compare minutes for time, not seconds (reduces unnecessary redraws)
    uint32_t currentMinute = currentTime / 60;
    return forceRedraw ||
           (batteryPercent != prevBatteryPercent) ||
           (loraConnected != prevLoraConnected) ||
           (gpsFix != prevGpsFix) ||
           (notifCount != prevNotifCount) ||
           (currentMinute != prevTime);
}

} // namespace StatusBar
