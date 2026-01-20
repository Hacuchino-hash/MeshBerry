/**
 * MeshBerry Status Bar
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2026 NodakMesh (nodakmesh.org)
 *
 * BlackBerry-style status bar with system indicators
 * Always visible at the top of the screen
 */

#ifndef MESHBERRY_STATUSBAR_H
#define MESHBERRY_STATUSBAR_H

#include <Arduino.h>
#include "Theme.h"

namespace StatusBar {

/**
 * Initialize the status bar
 */
void init();

/**
 * Draw/refresh the status bar
 * Call periodically (every ~1 second) to update time/battery
 */
void draw();

/**
 * Force full redraw (after screen change)
 */
void redraw();

/**
 * Update battery level
 * @param percent Battery percentage (0-100)
 */
void setBatteryPercent(uint8_t percent);

/**
 * Update LoRa connection status
 * @param connected True if LoRa radio is active
 * @param rssi Signal strength (negative dBm)
 */
void setLoRaStatus(bool connected, int16_t rssi = 0);

/**
 * Update GPS status
 * @param hasGps True if GPS module present
 * @param hasFix True if GPS has valid fix
 */
void setGpsStatus(bool hasGps, bool hasFix = false);

/**
 * Set notification badge count
 * @param count Number of unread notifications (0 to hide)
 */
void setNotificationCount(uint8_t count);

/**
 * Show temporary status message (replaces center area briefly)
 * @param message Status message to display
 * @param durationMs How long to show (default 2000ms)
 */
void showStatus(const char* message, uint32_t durationMs = 2000);

/**
 * Update the clock from RTC
 * @param epochTime Unix timestamp
 */
void setTime(uint32_t epochTime);

/**
 * Set timezone offset from UTC
 * @param hours Offset in hours (e.g., -6 for CST, -8 for PST)
 */
void setTimezoneOffset(int8_t hours);

/**
 * Set node name for display
 */
void setNodeName(const char* name);

/**
 * Check if status bar needs refresh
 */
bool needsUpdate();

/**
 * Get status bar height
 */
constexpr int16_t getHeight() { return Theme::STATUS_BAR_HEIGHT; }

} // namespace StatusBar

#endif // MESHBERRY_STATUSBAR_H
