/**
 * MeshBerry Touch Screen Driver
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2026 NodakMesh (nodakmesh.org)
 *
 * This file is part of MeshBerry.
 *
 * GT911 capacitive touch controller driver for LILYGO T-Deck.
 * Shares I2C bus with keyboard controller.
 */

#ifndef MESHBERRY_TOUCH_H
#define MESHBERRY_TOUCH_H

#include <Arduino.h>

namespace Touch {

/**
 * Initialize the touch controller
 * @return true if GT911 was detected and initialized
 */
bool init();

/**
 * Check if a touch event is available (interrupt triggered)
 */
bool available();

/**
 * Read touch coordinates
 * @param x Pointer to store X coordinate (0-319)
 * @param y Pointer to store Y coordinate (0-239)
 * @return true if touch is active, false if released
 */
bool read(int16_t* x, int16_t* y);

/**
 * Check if screen is currently being touched
 */
bool isTouched();

/**
 * Get the number of touch points (GT911 supports multi-touch)
 */
uint8_t getTouchCount();

} // namespace Touch

#endif // MESHBERRY_TOUCH_H
