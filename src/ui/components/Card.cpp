/**
 * MeshBerry UI Component - Card Implementation
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2026 NodakMesh (nodakmesh.org)
 */

#include "Card.h"
#include "../../drivers/display.h"
#include "../Theme.h"

Card::Card(int16_t x, int16_t y, int16_t w, int16_t h)
    : _x(x), _y(y), _w(w), _h(h)
{
    _bgColor = Theme::COLOR_BG_CARD;
    _hasShadow = false;
    _radius = Theme::CARD_RADIUS;
}

void Card::setBgColor(uint16_t color) {
    _bgColor = color;
}

void Card::setShadow(bool enabled) {
    _hasShadow = enabled;
}

void Card::setRadius(int16_t radius) {
    _radius = radius;
}

void Card::setPosition(int16_t x, int16_t y) {
    _x = x;
    _y = y;
}

void Card::setSize(int16_t w, int16_t h) {
    _w = w;
    _h = h;
}

void Card::draw() {
    Display::drawCard(_x, _y, _w, _h, _bgColor, _radius, _hasShadow);
}

void Card::drawWithContent(std::function<void(int16_t, int16_t, int16_t, int16_t)> drawContent) {
    // Draw card background first
    draw();

    // Calculate content area (with padding)
    int16_t contentX = _x + Theme::CARD_PADDING;
    int16_t contentY = _y + Theme::CARD_PADDING;
    int16_t contentW = _w - (Theme::CARD_PADDING * 2);
    int16_t contentH = _h - (Theme::CARD_PADDING * 2);

    // Call content rendering callback
    if (drawContent) {
        drawContent(contentX, contentY, contentW, contentH);
    }
}

bool Card::contains(int16_t x, int16_t y) {
    return (x >= _x && x < _x + _w && y >= _y && y < _y + _h);
}

void Card::getBounds(int16_t& x, int16_t& y, int16_t& w, int16_t& h) {
    x = _x;
    y = _y;
    w = _w;
    h = _h;
}
