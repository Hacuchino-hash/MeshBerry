/**
 * MeshBerry GPS Screen Implementation
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2026 NodakMesh (nodakmesh.org)
 */

#include "GpsScreen.h"
#include "SoftKeyBar.h"
#include "../drivers/display.h"
#include "../drivers/gps.h"
#include "../drivers/keyboard.h"
#include <Arduino.h>
#include <stdio.h>
#include <string.h>

// External gpsPresent flag from main.cpp
extern bool gpsPresent;

void GpsScreen::onEnter() {
    _lastUpdate = 0;  // Force full redraw
    requestRedraw();
}

void GpsScreen::configureSoftKeys() {
    SoftKeyBar::setLabels(nullptr, nullptr, "Back");
}

void GpsScreen::formatLatitude(double lat, char* buf, size_t bufSize) {
    char dir = lat >= 0 ? 'N' : 'S';
    if (lat < 0) lat = -lat;
    snprintf(buf, bufSize, "%.6f %c", lat, dir);
}

void GpsScreen::formatLongitude(double lon, char* buf, size_t bufSize) {
    char dir = lon >= 0 ? 'E' : 'W';
    if (lon < 0) lon = -lon;
    snprintf(buf, bufSize, "%.6f %c", lon, dir);
}

void GpsScreen::formatFixAge(uint32_t ageMs, char* buf, size_t bufSize) {
    if (ageMs < 1000) {
        snprintf(buf, bufSize, "<%ds", 1);
    } else if (ageMs < 60000) {
        snprintf(buf, bufSize, "%ds", (int)(ageMs / 1000));
    } else if (ageMs < 3600000) {
        snprintf(buf, bufSize, "%dm", (int)(ageMs / 60000));
    } else {
        snprintf(buf, bufSize, ">1h");
    }
}

const char* GpsScreen::getHdopQuality(float hdop) {
    if (hdop < 1.0) return "Ideal";
    if (hdop < 2.0) return "Excellent";
    if (hdop < 5.0) return "Good";
    if (hdop < 10.0) return "Moderate";
    return "Poor";
}

void GpsScreen::drawDataRow(int16_t y, const char* label, const char* value,
                            uint16_t labelColor, uint16_t valueColor) {
    Display::drawText(16, y, label, labelColor, 1);
    Display::drawText(100, y, value, valueColor, 1);
}

void GpsScreen::drawNoGps() {
    Display::drawTextCentered(0, Theme::CONTENT_Y + 70,
                              Theme::SCREEN_WIDTH,
                              "GPS Not Available", Theme::TEXT_SECONDARY, 2);
    Display::drawTextCentered(0, Theme::CONTENT_Y + 100,
                              Theme::SCREEN_WIDTH,
                              "T-Deck Plus required", Theme::GRAY_LIGHT, 1);
}

void GpsScreen::drawAcquiring() {
    int16_t y = Theme::CONTENT_Y + 40;

    // Show time/date if available (GPS gets time before position fix)
    uint8_t hour, minute, second;
    uint8_t day, month;
    uint16_t year;
    char buf[32];

    if (GPS::getTime(&hour, &minute, &second)) {
        snprintf(buf, sizeof(buf), "Time: %02d:%02d:%02d UTC", hour, minute, second);
        Display::drawTextCentered(0, y, Theme::SCREEN_WIDTH, buf, Theme::ACCENT, 1);
        y += 16;
    }

    if (GPS::getDate(&day, &month, &year)) {
        snprintf(buf, sizeof(buf), "Date: %04d-%02d-%02d", year, month, day);
        Display::drawTextCentered(0, y, Theme::SCREEN_WIDTH, buf, Theme::ACCENT, 1);
        y += 20;
    }

    // Acquiring message
    Display::drawTextCentered(0, y + 10,
                              Theme::SCREEN_WIDTH,
                              "Acquiring Position...", Theme::TEXT_SECONDARY, 2);

    // Show satellite count
    uint8_t sats = GPS::getSatellites();
    snprintf(buf, sizeof(buf), "Satellites: %d", sats);
    Display::drawTextCentered(0, y + 45,
                              Theme::SCREEN_WIDTH,
                              buf, Theme::TEXT_SECONDARY, 1);

    // Animated dots
    static int dotCount = 0;
    dotCount = (dotCount + 1) % 4;
    char dots[5] = "   ";
    for (int i = 0; i < dotCount; i++) dots[i] = '.';
    Display::drawText(Theme::SCREEN_WIDTH / 2 + 95, y + 10, dots, Theme::TEXT_SECONDARY, 2);
}

void GpsScreen::drawGpsData() {
    int16_t y = Theme::CONTENT_Y + 36;
    int16_t lineHeight = 18;
    char buf[32];

    // UTC Time and Date on first row
    uint8_t hour, minute, second;
    uint8_t day, month;
    uint16_t year;

    if (GPS::getTime(&hour, &minute, &second)) {
        snprintf(buf, sizeof(buf), "%02d:%02d:%02d UTC", hour, minute, second);
    } else {
        snprintf(buf, sizeof(buf), "--:--:-- UTC");
    }
    drawDataRow(y, "Time:", buf, Theme::TEXT_SECONDARY, Theme::ACCENT);
    y += lineHeight;

    if (GPS::getDate(&day, &month, &year)) {
        snprintf(buf, sizeof(buf), "%04d-%02d-%02d", year, month, day);
    } else {
        snprintf(buf, sizeof(buf), "----/--/--");
    }
    drawDataRow(y, "Date:", buf, Theme::TEXT_SECONDARY, Theme::ACCENT);
    y += lineHeight;

    // Divider
    Display::drawHLine(16, y + 2, Theme::SCREEN_WIDTH - 32, Theme::DIVIDER);
    y += 8;

    // Latitude
    formatLatitude(GPS::getLatitude(), buf, sizeof(buf));
    drawDataRow(y, "Lat:", buf, Theme::TEXT_SECONDARY, Theme::WHITE);
    y += lineHeight;

    // Longitude
    formatLongitude(GPS::getLongitude(), buf, sizeof(buf));
    drawDataRow(y, "Lon:", buf, Theme::TEXT_SECONDARY, Theme::WHITE);
    y += lineHeight;

    // Altitude and Speed on same conceptual area
    snprintf(buf, sizeof(buf), "%.1f m", GPS::getAltitude());
    drawDataRow(y, "Alt:", buf, Theme::TEXT_SECONDARY, Theme::WHITE);

    // Speed on the right side
    char speedBuf[16];
    snprintf(speedBuf, sizeof(speedBuf), "%.1f km/h", GPS::getSpeed());
    Display::drawText(180, y, speedBuf, Theme::WHITE, 1);
    y += lineHeight;

    // Divider
    Display::drawHLine(16, y + 2, Theme::SCREEN_WIDTH - 32, Theme::DIVIDER);
    y += 8;

    // Satellites and HDOP on same row
    uint8_t sats = GPS::getSatellites();
    float hdop = GPS::getHDOP();
    snprintf(buf, sizeof(buf), "%d sats", sats);

    // Sat count with color based on count
    uint16_t satColor = Theme::RED;
    if (sats >= 6) satColor = Theme::GREEN;
    else if (sats >= 4) satColor = Theme::YELLOW;
    drawDataRow(y, "Signal:", buf, Theme::TEXT_SECONDARY, satColor);

    // HDOP on the right side
    snprintf(buf, sizeof(buf), "HDOP: %.1f", hdop);
    Display::drawText(180, y, buf, Theme::TEXT_SECONDARY, 1);
    y += lineHeight;

    // Fix status and age
    bool hasFix = GPS::hasFix();
    uint32_t age = GPS::getFixAge();

    Display::drawText(16, y, "Fix:", Theme::TEXT_SECONDARY, 1);
    Display::drawText(100, y, hasFix ? "Yes" : "No", hasFix ? Theme::GREEN : Theme::RED, 1);

    if (hasFix) {
        formatFixAge(age, buf, sizeof(buf));
        char ageStr[24];
        snprintf(ageStr, sizeof(ageStr), "Age: %s", buf);
        Display::drawText(180, y, ageStr, Theme::TEXT_SECONDARY, 1);
    }
}

void GpsScreen::draw(bool fullRedraw) {
    // Always clear content area on full redraw
    if (fullRedraw) {
        Display::fillRect(0, Theme::CONTENT_Y,
                          Theme::SCREEN_WIDTH, Theme::CONTENT_HEIGHT,
                          Theme::BG_PRIMARY);

        // Title
        Display::drawText(12, Theme::CONTENT_Y + 8, "GPS", Theme::ACCENT, 2);

        // Divider below title
        Display::drawHLine(12, Theme::CONTENT_Y + 30, Theme::SCREEN_WIDTH - 24, Theme::DIVIDER);
    }

    // Handle different states
    if (!gpsPresent) {
        if (fullRedraw) {
            drawNoGps();
        }
        return;
    }

    // Update GPS data
    GPS::update();

    bool hasFix = GPS::hasFix();

    // Check if we need to redraw data area
    bool needsDataRedraw = fullRedraw;

    // Check for changes that require redraw (every 500ms or on changes)
    if (!fullRedraw && millis() - _lastUpdate > 500) {
        // Check if significant values changed
        double lat = GPS::getLatitude();
        double lon = GPS::getLongitude();
        uint8_t sats = GPS::getSatellites();

        if (lat != _lastLat || lon != _lastLon || sats != _lastSats || hasFix != _lastFix) {
            needsDataRedraw = true;
        }
        _lastUpdate = millis();
    }

    if (needsDataRedraw) {
        // Clear data area (below title)
        Display::fillRect(0, Theme::CONTENT_Y + 32,
                          Theme::SCREEN_WIDTH, Theme::CONTENT_HEIGHT - 32,
                          Theme::BG_PRIMARY);

        if (!hasFix) {
            drawAcquiring();
        } else {
            drawGpsData();
        }

        // Cache current values
        _lastLat = GPS::getLatitude();
        _lastLon = GPS::getLongitude();
        _lastAlt = GPS::getAltitude();
        _lastSpeed = GPS::getSpeed();
        _lastCourse = GPS::getCourse();
        _lastSats = GPS::getSatellites();
        _lastHdop = GPS::getHDOP();
        _lastFix = hasFix;
    }
}

bool GpsScreen::handleInput(const InputData& input) {
    // Treat backspace as back since this screen has no text input
    bool isBackKey = (input.event == InputEvent::KEY_PRESS && input.keyCode == KEY_BACKSPACE);
    if (input.event == InputEvent::BACK ||
        input.event == InputEvent::SOFTKEY_RIGHT ||
        input.event == InputEvent::TRACKBALL_LEFT ||
        isBackKey) {
        Screens.goBack();
        return true;
    }

    return false;
}
