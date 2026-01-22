/**
 * MeshBerry UI Component - Button Implementation
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2026 NodakMesh (nodakmesh.org)
 */

#include "Button.h"
#include "../../drivers/display.h"
#include "../Theme.h"

Button::Button(int16_t x, int16_t y, int16_t w, const char* label)
    : _x(x), _y(y), _w(w), _h(Theme::BTN_HEIGHT)
{
    _label = String(label);
    _icon = nullptr;
    _isPrimary = false;
    _state = Display::BTN_NORMAL;
    _onClick = nullptr;
    _enabled = true;
}

void Button::setIcon(const uint8_t* icon) {
    _icon = icon;
}

void Button::setPrimary(bool isPrimary) {
    _isPrimary = isPrimary;
}

void Button::setState(Display::ButtonState state) {
    _state = state;
}

void Button::setEnabled(bool enabled) {
    _enabled = enabled;
    _state = enabled ? Display::BTN_NORMAL : Display::BTN_DISABLED;
}

void Button::setOnClick(std::function<void()> callback) {
    _onClick = callback;
}

void Button::setLabel(const char* label) {
    _label = String(label);
}

void Button::setPosition(int16_t x, int16_t y) {
    _x = x;
    _y = y;
}

void Button::setWidth(int16_t w) {
    _w = w;
}

void Button::draw() {
    // Use Display helper function
    Display::drawButton(_x, _y, _w, _h, _label.c_str(), _state, _isPrimary);

    // Draw icon if present
    if (_icon) {
        // Position icon to the left of text
        int16_t iconX = _x + 8;
        int16_t iconY = _y + (_h - 16) / 2;  // Center vertically

        uint16_t iconColor = _isPrimary ? Theme::WHITE : Theme::COLOR_PRIMARY;
        if (!_enabled) {
            iconColor = Theme::COLOR_TEXT_DISABLED;
        }

        Display::drawBitmap(iconX, iconY, _icon, 16, 16, iconColor);
    }
}

bool Button::handleClick(int16_t x, int16_t y) {
    if (!_enabled) return false;

    if (contains(x, y)) {
        // Trigger callback if set
        if (_onClick) {
            _onClick();
        }
        return true;
    }
    return false;
}

void Button::getBounds(int16_t& x, int16_t& y, int16_t& w, int16_t& h) {
    x = _x;
    y = _y;
    w = _w;
    h = _h;
}

bool Button::contains(int16_t x, int16_t y) {
    return (x >= _x && x < _x + _w && y >= _y && y < _y + _h);
}
