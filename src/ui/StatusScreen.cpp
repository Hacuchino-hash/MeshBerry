/**
 * MeshBerry Status Screen Implementation
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2026 NodakMesh (nodakmesh.org)
 */

#include "StatusScreen.h"
#include "SoftKeyBar.h"
#include "../drivers/display.h"
#include "../drivers/keyboard.h"
#include <stdio.h>

void StatusScreen::onEnter() {
    requestRedraw();
}

void StatusScreen::configureSoftKeys() {
    SoftKeyBar::setLabels(nullptr, nullptr, "Back");
}

void StatusScreen::draw(bool fullRedraw) {
    int16_t y = Theme::CONTENT_Y + 4;
    const int16_t cardX = 8;
    const int16_t cardW = Theme::SCREEN_WIDTH - 16;
    char buf[48];

    if (fullRedraw) {
        Display::fillRect(0, Theme::CONTENT_Y,
                          Theme::SCREEN_WIDTH, Theme::CONTENT_HEIGHT,
                          Theme::BG_PRIMARY);
    }

    // Title (no card, just header)
    Display::drawText(12, y, "System Status", Theme::COLOR_PRIMARY, 2);
    y += 24;

    // === BATTERY CARD ===
    const int16_t battCardH = 56;
    Display::drawCard(cardX, y, cardW, battCardH, Theme::COLOR_BG_CARD, Theme::CARD_RADIUS, false);

    // Battery icon/emoji and title
    Display::drawText(cardX + 12, y + 10, "Battery", Theme::COLOR_TEXT_PRIMARY, 1);

    // Status indicator dot
    uint16_t battColor = (_batteryPercent > 20) ? Theme::COLOR_SUCCESS : COLOR_ERROR;  // Use macro from config.h
    Display::fillCircle(cardX + cardW - 16, y + 16, 5, battColor);

    // Battery percentage and voltage
    snprintf(buf, sizeof(buf), "%d%% · %dmV", _batteryPercent, _batteryMv);
    Display::drawText(cardX + 12, y + 26, buf, Theme::COLOR_TEXT_SECONDARY, 1);

    // Progress bar
    float battProgress = _batteryPercent / 100.0f;
    Display::drawProgressBar(cardX + 12, y + 40, cardW - 24, 8, battProgress, battColor);

    y += battCardH + Theme::SPACE_SM;

    // === LORA RADIO CARD ===
    const int16_t radioCardH = 52;
    Display::drawCard(cardX, y, cardW, radioCardH, Theme::COLOR_BG_CARD, Theme::CARD_RADIUS, false);

    // Radio title
    Display::drawText(cardX + 12, y + 10, "LoRa Radio", Theme::COLOR_TEXT_PRIMARY, 1);

    // Online status dot (always on for now)
    Display::fillCircle(cardX + cardW - 16, y + 16, 5, Theme::COLOR_SUCCESS);
    Display::drawText(cardX + cardW - 32, y + 12, "ON", Theme::COLOR_SUCCESS, 1);

    // Frequency
    snprintf(buf, sizeof(buf), "%.1f MHz", _loraFreq);
    Display::drawText(cardX + 12, y + 26, buf, Theme::COLOR_TEXT_SECONDARY, 1);

    // Parameters
    snprintf(buf, sizeof(buf), "SF%d  BW%.0f  TX%ddBm", _loraSf, _loraBw, _loraTxPower);
    Display::drawText(cardX + 12, y + 38, buf, Theme::COLOR_TEXT_HINT, 1);

    y += radioCardH + Theme::SPACE_SM;

    // === MESH NETWORK CARD ===
    const int16_t meshCardH = 52;
    Display::drawCard(cardX, y, cardW, meshCardH, Theme::COLOR_BG_CARD, Theme::CARD_RADIUS, false);

    // Mesh title
    Display::drawText(cardX + 12, y + 10, "Mesh Network", Theme::COLOR_TEXT_PRIMARY, 1);

    // Forwarding status dot
    uint16_t fwdColor = _forwarding ? Theme::COLOR_SUCCESS : Theme::GRAY_MID;
    Display::fillCircle(cardX + cardW - 16, y + 16, 5, fwdColor);
    Display::drawText(cardX + cardW - 32, y + 12, _forwarding ? "ON" : "OFF", fwdColor, 1);

    // Node count
    snprintf(buf, sizeof(buf), "Nodes: %d", _nodeCount);
    Display::drawText(cardX + 12, y + 26, buf, Theme::COLOR_TEXT_SECONDARY, 1);

    // Message count
    snprintf(buf, sizeof(buf), "Messages: %d TX/RX", _msgCount);
    Display::drawText(cardX + 12, y + 38, buf, Theme::COLOR_TEXT_HINT, 1);

    y += meshCardH + Theme::SPACE_SM;

    // === GPS CARD ===
    const int16_t gpsCardH = 40;
    Display::drawCard(cardX, y, cardW, gpsCardH, Theme::COLOR_BG_CARD, Theme::CARD_RADIUS, false);

    // GPS title
    Display::drawText(cardX + 12, y + 10, "GPS", Theme::COLOR_TEXT_PRIMARY, 1);

    // GPS status
    if (_gpsPresent) {
        uint16_t gpsColor = _gpsFix ? Theme::COLOR_SUCCESS : COLOR_WARNING;  // Use macro from config.h
        const char* gpsStatus = _gpsFix ? "Fix" : "Acquiring";
        Display::fillCircle(cardX + cardW - 16, y + 16, 5, gpsColor);
        Display::drawText(cardX + cardW - 60, y + 12, gpsStatus, gpsColor, 1);
    } else {
        Display::drawText(cardX + 12, y + 24, "Not present", Theme::COLOR_TEXT_DISABLED, 1);
    }
}

bool StatusScreen::handleInput(const InputData& input) {
    // Handle touch tap for soft keys
    if (input.event == InputEvent::TOUCH_TAP) {
        int16_t ty = input.touchY;
        int16_t tx = input.touchX;

        // Soft key bar touch (Y >= 210)
        if (ty >= Theme::SOFTKEY_BAR_Y) {
            if (tx >= 214) {
                // Right soft key = Back
                Screens.goBack();
            }
            return true;
        }
        return true;
    }

    // Treat backspace as back since this screen has no text input
    bool isBackKey = (input.event == InputEvent::KEY_PRESS && input.keyCode == KEY_BACKSPACE);
    if (isBackKey) {
        Screens.goBack();
        return true;
    }

    switch (input.event) {
        case InputEvent::BACK:
        case InputEvent::SOFTKEY_RIGHT:
        case InputEvent::TRACKBALL_LEFT:
            Screens.goBack();
            return true;

        default:
            return false;
    }
}

void StatusScreen::setBatteryInfo(uint8_t percent, uint16_t millivolts) {
    _batteryPercent = percent;
    _batteryMv = millivolts;
    requestRedraw();
}

void StatusScreen::setRadioInfo(float freq, int sf, float bw, int txPower) {
    _loraFreq = freq;
    _loraSf = sf;
    _loraBw = bw;
    _loraTxPower = txPower;
    requestRedraw();
}

void StatusScreen::setMeshInfo(int nodeCount, int msgCount, bool forwarding) {
    _nodeCount = nodeCount;
    _msgCount = msgCount;
    _forwarding = forwarding;
    requestRedraw();
}

void StatusScreen::setGpsInfo(bool present, bool hasFix) {
    _gpsPresent = present;
    _gpsFix = hasFix;
    requestRedraw();
}
