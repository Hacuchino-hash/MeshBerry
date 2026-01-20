/**
 * MeshBerry GPS Driver Implementation
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2026 NodakMesh (nodakmesh.org)
 *
 * Uses TinyGPSPlus library to parse NMEA data from u-blox MIA-M10Q
 */

#include "gps.h"
#include <TinyGPSPlus.h>

// =============================================================================
// STATIC VARIABLES
// =============================================================================

static TinyGPSPlus gps;
static bool _gpsPresent = false;
static bool _gpsEnabled = false;
static uint32_t _lastFixTime = 0;
static uint32_t _fixAge = 0;
static uint32_t _gpsBaudRate = 9600;  // Default, will be set by main.cpp

// =============================================================================
// GPS NAMESPACE IMPLEMENTATION
// =============================================================================

namespace GPS {

bool init() {
    // GPS serial is initialized in main.cpp (Serial1)
    // Check if we already detected GPS during hardware init
    // This function is called after Serial1.begin() has been done

    // Try to detect GPS by looking for NMEA data
    unsigned long start = millis();
    while (millis() - start < 1000) {
        if (Serial1.available()) {
            char c = Serial1.read();
            if (c == '$') {
                _gpsPresent = true;
                _gpsEnabled = true;
                return true;
            }
        }
    }

    _gpsPresent = false;
    _gpsEnabled = false;
    return false;
}

bool isPresent() {
    return _gpsPresent;
}

void update() {
    if (!_gpsPresent || !_gpsEnabled) return;

    // Feed all available characters to the parser
    while (Serial1.available()) {
        char c = Serial1.read();
        gps.encode(c);
    }

    // Update fix age tracking
    if (gps.location.isValid() && gps.location.isUpdated()) {
        _lastFixTime = millis();
    }

    if (_lastFixTime > 0) {
        _fixAge = millis() - _lastFixTime;
    }
}

bool hasFix() {
    return gps.location.isValid() && _fixAge < 10000;  // Fix is valid if less than 10 seconds old
}

GPSData_t getData() {
    GPSData_t data;

    data.latitude = gps.location.lat();
    data.longitude = gps.location.lng();
    data.altitude = gps.altitude.meters();
    data.speed = gps.speed.kmph();
    data.course = gps.course.deg();
    data.satellites = gps.satellites.value();
    data.hdop = gps.hdop.hdop();
    data.valid = hasFix();

    // Time as HHMMSS * 100 + centiseconds
    if (gps.time.isValid()) {
        data.timestamp = gps.time.hour() * 1000000 +
                         gps.time.minute() * 10000 +
                         gps.time.second() * 100 +
                         gps.time.centisecond();
    } else {
        data.timestamp = 0;
    }

    // Date as DDMMYY
    if (gps.date.isValid()) {
        data.date = gps.date.day() * 10000 +
                    gps.date.month() * 100 +
                    (gps.date.year() % 100);
    } else {
        data.date = 0;
    }

    return data;
}

double getLatitude() {
    return gps.location.lat();
}

double getLongitude() {
    return gps.location.lng();
}

double getAltitude() {
    return gps.altitude.meters();
}

float getSpeed() {
    return gps.speed.kmph();
}

float getCourse() {
    return gps.course.deg();
}

uint8_t getSatellites() {
    return gps.satellites.value();
}

float getHDOP() {
    return gps.hdop.hdop();
}

uint32_t getFixAge() {
    return _fixAge;
}

bool getTime(uint8_t* hour, uint8_t* minute, uint8_t* second) {
    if (!gps.time.isValid()) return false;
    if (hour) *hour = gps.time.hour();
    if (minute) *minute = gps.time.minute();
    if (second) *second = gps.time.second();
    return true;
}

bool getDate(uint8_t* day, uint8_t* month, uint16_t* year) {
    if (!gps.date.isValid()) return false;
    if (day) *day = gps.date.day();
    if (month) *month = gps.date.month();
    if (year) *year = gps.date.year();
    return true;
}

bool hasValidTime() {
    return gps.time.isValid();
}

bool hasValidDate() {
    return gps.date.isValid();
}

double distanceTo(double lat, double lng) {
    if (!hasFix()) return 0.0;
    return TinyGPSPlus::distanceBetween(
        gps.location.lat(), gps.location.lng(),
        lat, lng
    );
}

double bearingTo(double lat, double lng) {
    if (!hasFix()) return 0.0;
    return TinyGPSPlus::courseTo(
        gps.location.lat(), gps.location.lng(),
        lat, lng
    );
}

void setBaudRate(uint32_t baud) {
    _gpsBaudRate = baud;
}

void setPresent(bool present) {
    _gpsPresent = present;
}

void enable() {
    if (_gpsPresent) {
        _gpsEnabled = true;
        // Re-initialize Serial1 if it was disabled
        Serial1.begin(_gpsBaudRate, SERIAL_8N1, PIN_GPS_RX, PIN_GPS_TX);
    }
}

void disable() {
    _gpsEnabled = false;
    // Optionally end Serial1 to save power
    Serial1.end();
}

bool isEnabled() {
    return _gpsEnabled;
}

void setPowerSave(bool enable) {
    // u-blox power save mode would require sending UBX commands
    // For now, this is a placeholder
    // Could implement UBX-CFG-PM2 or UBX-CFG-RXM commands
    (void)enable;
}

} // namespace GPS
