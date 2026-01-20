/**
 * MeshBerry Status Screen Implementation
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2026 NodakMesh (nodakmesh.org)
 */

#include "StatusScreen.h"
#include "SoftKeyBar.h"
#include "../drivers/display.h"
#include <stdio.h>

void StatusScreen::onEnter() {
    requestRedraw();
}

void StatusScreen::configureSoftKeys() {
    SoftKeyBar::setLabels(nullptr, nullptr, "Back");
}

void StatusScreen::draw(bool fullRedraw) {
    int16_t y = Theme::CONTENT_Y + 8;
    const int16_t lineHeight = 20;
    const int16_t labelX = 12;
    const int16_t valueX = 140;
    char buf[48];

    if (fullRedraw) {
        Display::fillRect(0, Theme::CONTENT_Y,
                          Theme::SCREEN_WIDTH, Theme::CONTENT_HEIGHT,
                          Theme::BG_PRIMARY);
    }

    // Title
    Display::drawText(labelX, y, "Device Status", Theme::ACCENT, 2);
    y += 28;

    // Divider
    Display::drawHLine(labelX, y, Theme::SCREEN_WIDTH - 24, Theme::DIVIDER);
    y += 8;

    // Battery
    Display::drawText(labelX, y, "Battery:", Theme::TEXT_SECONDARY, 1);
    snprintf(buf, sizeof(buf), "%d%% (%dmV)", _batteryPercent, _batteryMv);
    uint16_t battColor = (_batteryPercent > 20) ? Theme::GREEN : Theme::RED;
    Display::drawText(valueX, y, buf, battColor, 1);
    y += lineHeight;

    // Radio
    Display::drawText(labelX, y, "Radio:", Theme::TEXT_SECONDARY, 1);
    snprintf(buf, sizeof(buf), "%.3f MHz", _loraFreq);
    Display::drawText(valueX, y, buf, Theme::WHITE, 1);
    y += lineHeight;

    Display::drawText(labelX, y, "Params:", Theme::TEXT_SECONDARY, 1);
    snprintf(buf, sizeof(buf), "SF%d BW%.0f TX%ddBm", _loraSf, _loraBw, _loraTxPower);
    Display::drawText(valueX, y, buf, Theme::WHITE, 1);
    y += lineHeight;

    // Mesh
    Display::drawText(labelX, y, "Nodes:", Theme::TEXT_SECONDARY, 1);
    snprintf(buf, sizeof(buf), "%d known", _nodeCount);
    Display::drawText(valueX, y, buf, Theme::WHITE, 1);
    y += lineHeight;

    Display::drawText(labelX, y, "Messages:", Theme::TEXT_SECONDARY, 1);
    snprintf(buf, sizeof(buf), "%d", _msgCount);
    Display::drawText(valueX, y, buf, Theme::WHITE, 1);
    y += lineHeight;

    Display::drawText(labelX, y, "Forwarding:", Theme::TEXT_SECONDARY, 1);
    Display::drawText(valueX, y, _forwarding ? "ON" : "OFF",
                      _forwarding ? Theme::GREEN : Theme::GRAY_LIGHT, 1);
    y += lineHeight;

    // GPS
    Display::drawText(labelX, y, "GPS:", Theme::TEXT_SECONDARY, 1);
    if (_gpsPresent) {
        Display::drawText(valueX, y, _gpsFix ? "Fix" : "No fix",
                          _gpsFix ? Theme::GREEN : Theme::YELLOW, 1);
    } else {
        Display::drawText(valueX, y, "Not present", Theme::GRAY_LIGHT, 1);
    }
}

bool StatusScreen::handleInput(const InputData& input) {
    // Treat backspace as back since this screen has no text input
    bool isBackKey = (input.event == InputEvent::KEY_PRESS && input.keyCode == KEY_BACKSPACE);
    if (isBackKey) {
        Screens.goBack();
        return true;
    }

    switch (input.event) {
        case InputEvent::BACK:
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
