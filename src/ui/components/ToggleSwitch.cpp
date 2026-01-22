/**
 * MeshBerry UI Component - ToggleSwitch Implementation
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2026 NodakMesh (nodakmesh.org)
 */

#include "ToggleSwitch.h"
#include "../../drivers/display.h"
#include "../Theme.h"

ToggleSwitch::ToggleSwitch(int16_t x, int16_t y, bool initialState)
    : _x(x), _y(y), _isOn(initialState), _isEnabled(true), _onChange(nullptr)
{
}

void ToggleSwitch::setEnabled(bool enabled) {
    _isEnabled = enabled;
}

void ToggleSwitch::setState(bool isOn) {
    _isOn = isOn;
}

void ToggleSwitch::setOnChange(std::function<void(bool)> callback) {
    _onChange = callback;
}

void ToggleSwitch::setPosition(int16_t x, int16_t y) {
    _x = x;
    _y = y;
}

void ToggleSwitch::draw() {
    // Use Display helper function
    Display::drawToggle(_x, _y, _isOn, _isEnabled);
}

bool ToggleSwitch::handleClick(int16_t x, int16_t y) {
    if (!_isEnabled) return false;

    if (contains(x, y)) {
        // Toggle state
        _isOn = !_isOn;

        // Trigger callback
        if (_onChange) {
            _onChange(_isOn);
        }

        return true;
    }
    return false;
}

void ToggleSwitch::getBounds(int16_t& x, int16_t& y, int16_t& w, int16_t& h) {
    x = _x;
    y = _y;
    w = TOGGLE_W;
    h = TOGGLE_H;
}

bool ToggleSwitch::contains(int16_t x, int16_t y) {
    return (x >= _x && x < _x + TOGGLE_W && y >= _y && y < _y + TOGGLE_H);
}
