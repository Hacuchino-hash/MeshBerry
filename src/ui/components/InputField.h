/**
 * MeshBerry UI Component - InputField
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2026 NodakMesh (nodakmesh.org)
 *
 * Text input field with focus states and cursor
 */

#ifndef MESHBERRY_INPUTFIELD_H
#define MESHBERRY_INPUTFIELD_H

#include <Arduino.h>
#include <functional>

/**
 * Text input field component
 */
class InputField {
private:
    int16_t _x, _y, _w, _h;
    String _text;
    String _hint;  // Placeholder text
    bool _isFocused;
    bool _isEnabled;
    int _cursorPos;
    uint32_t _lastCursorBlink;
    bool _cursorVisible;
    int _maxLength;
    std::function<void(const char*)> _onChange;
    std::function<void()> _onEnter;

public:
    /**
     * Create an input field
     * @param x X position
     * @param y Y position
     * @param w Width
     * @param hint Placeholder text (shown when empty)
     */
    InputField(int16_t x, int16_t y, int16_t w, const char* hint = "");

    /**
     * Set focus state
     */
    void setFocused(bool focused);

    /**
     * Set enabled/disabled
     */
    void setEnabled(bool enabled);

    /**
     * Set text content
     */
    void setText(const char* text);

    /**
     * Set hint/placeholder text
     */
    void setHint(const char* hint);

    /**
     * Set maximum text length
     */
    void setMaxLength(int maxLength);

    /**
     * Set change callback (called when text changes)
     */
    void setOnChange(std::function<void(const char*)> callback);

    /**
     * Set enter callback (called when Enter is pressed)
     */
    void setOnEnter(std::function<void()> callback);

    /**
     * Set position
     */
    void setPosition(int16_t x, int16_t y);

    /**
     * Set width
     */
    void setWidth(int16_t w);

    /**
     * Draw the input field
     */
    void draw();

    /**
     * Handle character input
     * @param c Character to append
     * @return true if character was accepted
     */
    bool handleChar(char c);

    /**
     * Handle backspace
     * @return true if character was deleted
     */
    bool handleBackspace();

    /**
     * Handle click (for focus)
     * @param x Click X coordinate
     * @param y Click Y coordinate
     * @return true if field was clicked
     */
    bool handleClick(int16_t x, int16_t y);

    /**
     * Get current text
     */
    const char* getText() const { return _text.c_str(); }

    /**
     * Get text length
     */
    int getLength() const { return _text.length(); }

    /**
     * Clear text
     */
    void clear();

    /**
     * Get focus state
     */
    bool isFocused() const { return _isFocused; }

    /**
     * Get bounds
     */
    void getBounds(int16_t& x, int16_t& y, int16_t& w, int16_t& h);

    /**
     * Check if point is within field
     */
    bool contains(int16_t x, int16_t y);
};

#endif // MESHBERRY_INPUTFIELD_H
