/**
 * MeshBerry UI Component - InputField Implementation
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2026 NodakMesh (nodakmesh.org)
 */

#include "InputField.h"
#include "../../drivers/display.h"
#include "../Theme.h"

InputField::InputField(int16_t x, int16_t y, int16_t w, const char* hint)
    : _x(x), _y(y), _w(w), _h(Theme::INPUT_HEIGHT_NEW)
{
    _text = "";
    _hint = String(hint);
    _isFocused = false;
    _isEnabled = true;
    _cursorPos = 0;
    _lastCursorBlink = 0;
    _cursorVisible = false;
    _maxLength = 128;
    _onChange = nullptr;
    _onEnter = nullptr;
}

void InputField::setFocused(bool focused) {
    _isFocused = focused;
    if (focused) {
        _lastCursorBlink = millis();
        _cursorVisible = true;
    }
}

void InputField::setEnabled(bool enabled) {
    _isEnabled = enabled;
    if (!enabled) {
        _isFocused = false;
    }
}

void InputField::setText(const char* text) {
    _text = String(text);
    _cursorPos = _text.length();
}

void InputField::setHint(const char* hint) {
    _hint = String(hint);
}

void InputField::setMaxLength(int maxLength) {
    _maxLength = maxLength;
}

void InputField::setOnChange(std::function<void(const char*)> callback) {
    _onChange = callback;
}

void InputField::setOnEnter(std::function<void()> callback) {
    _onEnter = callback;
}

void InputField::setPosition(int16_t x, int16_t y) {
    _x = x;
    _y = y;
}

void InputField::setWidth(int16_t w) {
    _w = w;
}

void InputField::draw() {
    // Determine colors based on state
    uint16_t bgColor = Theme::COLOR_BG_INPUT;
    uint16_t borderColor = _isFocused ? Theme::COLOR_FOCUS : Theme::COLOR_BORDER;
    uint16_t textColor = _isEnabled ? Theme::COLOR_TEXT_PRIMARY : Theme::COLOR_TEXT_DISABLED;

    // Draw background
    Display::fillRoundRect(_x, _y, _w, _h, Theme::INPUT_RADIUS_NEW, bgColor);

    // Draw border (thicker if focused)
    if (_isFocused) {
        Display::drawRoundRect(_x, _y, _w, _h, Theme::INPUT_RADIUS_NEW, borderColor);
        Display::drawRoundRect(_x + 1, _y + 1, _w - 2, _h - 2, Theme::INPUT_RADIUS_NEW, borderColor);
    } else {
        Display::drawRoundRect(_x, _y, _w, _h, Theme::INPUT_RADIUS_NEW, borderColor);
    }

    // Calculate text area (with padding)
    int16_t textX = _x + Theme::INPUT_PADDING_NEW;
    int16_t textY = _y + (_h - 8) / 2;  // Center vertically (8px font height)

    // Draw text or hint
    if (_text.length() > 0) {
        Display::drawText(textX, textY, _text.c_str(), textColor, 1);
    } else if (_hint.length() > 0 && !_isFocused) {
        Display::drawText(textX, textY, _hint.c_str(), Theme::COLOR_TEXT_HINT, 1);
    }

    // Draw blinking cursor if focused
    if (_isFocused && _isEnabled) {
        // Blink cursor every 500ms
        if (millis() - _lastCursorBlink > 500) {
            _cursorVisible = !_cursorVisible;
            _lastCursorBlink = millis();
        }

        if (_cursorVisible) {
            // Calculate cursor position (after text)
            int16_t textWidth = _text.length() * 6;  // 6px per char at size 1
            int16_t cursorX = textX + textWidth;
            int16_t cursorY = textY;

            // Draw cursor as vertical line
            Display::fillRect(cursorX, cursorY, 2, 8, Theme::COLOR_FOCUS);
        }
    }
}

bool InputField::handleChar(char c) {
    if (!_isEnabled || !_isFocused) return false;

    // Check if character is printable and within max length
    if (c >= 32 && c < 127 && _text.length() < _maxLength) {
        _text += c;
        _cursorPos = _text.length();

        // Trigger change callback
        if (_onChange) {
            _onChange(_text.c_str());
        }

        return true;
    }

    // Handle Enter key
    if (c == '\n' || c == '\r') {
        if (_onEnter) {
            _onEnter();
        }
        return true;
    }

    return false;
}

bool InputField::handleBackspace() {
    if (!_isEnabled || !_isFocused || _text.length() == 0) return false;

    // Remove last character
    _text.remove(_text.length() - 1);
    _cursorPos = _text.length();

    // Trigger change callback
    if (_onChange) {
        _onChange(_text.c_str());
    }

    return true;
}

bool InputField::handleClick(int16_t x, int16_t y) {
    if (!_isEnabled) return false;

    if (contains(x, y)) {
        setFocused(true);
        return true;
    } else {
        setFocused(false);
        return false;
    }
}

void InputField::clear() {
    _text = "";
    _cursorPos = 0;

    if (_onChange) {
        _onChange(_text.c_str());
    }
}

void InputField::getBounds(int16_t& x, int16_t& y, int16_t& w, int16_t& h) {
    x = _x;
    y = _y;
    w = _w;
    h = _h;
}

bool InputField::contains(int16_t x, int16_t y) {
    return (x >= _x && x < _x + _w && y >= _y && y < _y + _h);
}
