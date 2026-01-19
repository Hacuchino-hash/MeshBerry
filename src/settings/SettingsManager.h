/**
 * MeshBerry Settings Manager
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2026 NodakMesh (nodakmesh.org)
 *
 * This file is part of MeshBerry.
 *
 * Manages persistent storage of settings to SPIFFS.
 */

#ifndef MESHBERRY_SETTINGS_MANAGER_H
#define MESHBERRY_SETTINGS_MANAGER_H

#include <Arduino.h>
#include <Identity.h>
#include "RadioSettings.h"
#include "ChannelSettings.h"
#include "ContactSettings.h"

namespace SettingsManager {

/**
 * Initialize the settings manager
 * Must be called after SPIFFS is mounted
 * @return true if settings loaded successfully, false if using defaults
 */
bool init();

/**
 * Load settings from SPIFFS
 * @return true if loaded, false if file missing or corrupt (defaults applied)
 */
bool load();

/**
 * Save current settings to SPIFFS
 * @return true if saved successfully
 */
bool save();

/**
 * Reset all settings to defaults
 */
void resetToDefaults();

/**
 * Get reference to current radio settings
 */
RadioSettings& getRadioSettings();

/**
 * Get reference to current channel settings
 */
ChannelSettings& getChannelSettings();

/**
 * Get reference to current contact settings
 */
ContactSettings& getContactSettings();

/**
 * Save contacts to SPIFFS
 * Call this after modifying contacts
 */
bool saveContacts();

/**
 * Load node identity from SPIFFS
 * @param identity Reference to LocalIdentity to populate
 * @return true if loaded successfully, false if no identity file exists
 */
bool loadIdentity(mesh::LocalIdentity& identity);

/**
 * Save node identity to SPIFFS
 * @param identity The LocalIdentity to persist
 * @return true if saved successfully
 */
bool saveIdentity(mesh::LocalIdentity& identity);

/**
 * Check if identity file exists
 */
bool hasIdentity();

/**
 * Apply current radio settings to the LoRa driver
 * @return true if applied successfully
 */
bool applyRadioSettings();

/**
 * Set region and apply preset values
 * @param region Region to set
 */
void setRegion(LoRaRegion region);

/**
 * Set individual radio parameters (marks as CUSTOM region if changed)
 */
void setFrequency(float mhz);
void setBandwidth(float bw);
void setSpreadingFactor(uint8_t sf);
void setCodingRate(uint8_t cr);
void setTxPower(uint8_t dbm);

} // namespace SettingsManager

#endif // MESHBERRY_SETTINGS_MANAGER_H
