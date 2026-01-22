/**
 * MeshBerry UI Component - Card
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2026 NodakMesh (nodakmesh.org)
 *
 * Reusable card component for modern card-based UI
 */

#ifndef MESHBERRY_CARD_H
#define MESHBERRY_CARD_H

#include <Arduino.h>
#include <functional>

/**
 * Card component - Rounded rectangle with optional shadow
 * Used for grouping related content with visual hierarchy
 */
class Card {
private:
    int16_t _x, _y, _w, _h;
    uint16_t _bgColor;
    bool _hasShadow;
    int16_t _radius;

public:
    /**
     * Create a card component
     * @param x X position
     * @param y Y position
     * @param w Width
     * @param h Height
     */
    Card(int16_t x, int16_t y, int16_t w, int16_t h);

    /**
     * Set background color
     */
    void setBgColor(uint16_t color);

    /**
     * Enable/disable shadow
     */
    void setShadow(bool enabled);

    /**
     * Set corner radius
     */
    void setRadius(int16_t radius);

    /**
     * Set position
     */
    void setPosition(int16_t x, int16_t y);

    /**
     * Set size
     */
    void setSize(int16_t w, int16_t h);

    /**
     * Draw the card (empty)
     */
    void draw();

    /**
     * Draw card with custom content
     * The callback receives content area coordinates (with padding applied)
     * @param drawContent Lambda function to render content inside card
     */
    void drawWithContent(std::function<void(int16_t contentX, int16_t contentY,
                                            int16_t contentW, int16_t contentH)> drawContent);

    /**
     * Hit testing - check if point is within card bounds
     * @param x X coordinate
     * @param y Y coordinate
     * @return true if point is inside card
     */
    bool contains(int16_t x, int16_t y);

    /**
     * Get card bounds
     */
    void getBounds(int16_t& x, int16_t& y, int16_t& w, int16_t& h);
};

#endif // MESHBERRY_CARD_H
