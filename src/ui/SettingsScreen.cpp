/**
 * MeshBerry Settings Screen Implementation
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2026 NodakMesh (nodakmesh.org)
 */

#include "SettingsScreen.h"
#include "SoftKeyBar.h"
#include "Icons.h"
#include "../drivers/display.h"
#include "../drivers/keyboard.h"
#include "../drivers/lora.h"
#include "../drivers/gps.h"
#include "../settings/SettingsManager.h"
#include "../config.h"
#include <stdio.h>

SettingsScreen::SettingsScreen() {
    _listView.setBounds(0, Theme::CONTENT_Y, Theme::SCREEN_WIDTH, Theme::CONTENT_HEIGHT);
    _listView.setItemHeight(40);
}

void SettingsScreen::onEnter() {
    _currentLevel = SETTINGS_MAIN;
    _editingIndex = -1;
    buildMenu();
    requestRedraw();
}

void SettingsScreen::onExit() {
    // Save settings when leaving
    SettingsManager::save();
}

const char* SettingsScreen::getTitle() const {
    switch (_currentLevel) {
        case SETTINGS_RADIO:   return "Radio Settings";
        case SETTINGS_DISPLAY: return "Display";
        case SETTINGS_NETWORK: return "Network";
        case SETTINGS_GPS:     return "GPS Settings";
        case SETTINGS_ABOUT:   return "About";
        default:               return "Settings";
    }
}

void SettingsScreen::configureSoftKeys() {
    if (_editingIndex >= 0) {
        SoftKeyBar::setLabels("<", "OK", ">");
    } else if (_currentLevel == SETTINGS_MAIN) {
        SoftKeyBar::setLabels(nullptr, "Select", "Back");
    } else if (_currentLevel == SETTINGS_GPS) {
        SoftKeyBar::setLabels(nullptr, "Toggle", "Back");
    } else {
        SoftKeyBar::setLabels(nullptr, "Edit", "Back");
    }
}

void SettingsScreen::buildMenu() {
    _menuItemCount = 0;
    memset(_menuItems, 0, sizeof(_menuItems));

    RadioSettings& radio = SettingsManager::getRadioSettings();

    switch (_currentLevel) {
        case SETTINGS_MAIN:
            // Main settings menu
            _menuItems[0] = { "Radio", "Frequency, power, spreading", Icons::SETTINGS_ICON, Theme::ACCENT, false, 0, nullptr };
            _menuItems[1] = { "Display", "Brightness, timeout", Icons::SETTINGS_ICON, Theme::ACCENT, false, 0, nullptr };
            _menuItems[2] = { "Network", "Node name, forwarding", Icons::CONTACTS_ICON, Theme::ACCENT, false, 0, nullptr };
            _menuItems[3] = { "GPS", "Power, RTC sync options", Icons::SETTINGS_ICON, Theme::ACCENT, false, 0, nullptr };
            _menuItems[4] = { "About", "Version, licenses", Icons::INFO_ICON, Theme::ACCENT, false, 0, nullptr };
            _menuItemCount = 5;
            break;

        case SETTINGS_RADIO:
            // Radio settings
            snprintf(_valueStrings[0], 32, "%s", radio.getRegionName());
            _menuItems[0] = { "Region", _valueStrings[0], nullptr, Theme::ACCENT, false, 0, nullptr };

            snprintf(_valueStrings[1], 32, "%.3f MHz", radio.frequency);
            _menuItems[1] = { "Frequency", _valueStrings[1], nullptr, Theme::ACCENT, false, 0, nullptr };

            snprintf(_valueStrings[2], 32, "SF%d", radio.spreadingFactor);
            _menuItems[2] = { "Spreading Factor", _valueStrings[2], nullptr, Theme::ACCENT, false, 0, nullptr };

            snprintf(_valueStrings[3], 32, "%s kHz", radio.getBandwidthStr());
            _menuItems[3] = { "Bandwidth", _valueStrings[3], nullptr, Theme::ACCENT, false, 0, nullptr };

            snprintf(_valueStrings[4], 32, "4/%d", radio.codingRate);
            _menuItems[4] = { "Coding Rate", _valueStrings[4], nullptr, Theme::ACCENT, false, 0, nullptr };

            snprintf(_valueStrings[5], 32, "%d dBm", radio.txPower);
            _menuItems[5] = { "TX Power", _valueStrings[5], nullptr, Theme::ACCENT, false, 0, nullptr };

            _menuItemCount = 6;
            break;

        case SETTINGS_DISPLAY:
            _menuItems[0] = { "Brightness", "80%", nullptr, Theme::ACCENT, false, 0, nullptr };
            _menuItems[1] = { "Screen Timeout", "30 seconds", nullptr, Theme::ACCENT, false, 0, nullptr };
            _menuItemCount = 2;
            break;

        case SETTINGS_NETWORK:
            _menuItems[0] = { "Node Name", "MeshBerry", nullptr, Theme::ACCENT, false, 0, nullptr };
            _menuItems[1] = { "Packet Forwarding", "Enabled", nullptr, Theme::GREEN, false, 0, nullptr };
            _menuItemCount = 2;
            break;

        case SETTINGS_GPS: {
            // GPS settings
            DeviceSettings& device = SettingsManager::getDeviceSettings();

            _menuItems[0] = { "GPS Always On",
                              device.gpsEnabled ? "On" : "Off",
                              nullptr,
                              device.gpsEnabled ? Theme::GREEN : Theme::GRAY_LIGHT,
                              false, 0, nullptr };

            _menuItems[1] = { "RTC Auto-Sync",
                              device.gpsRtcSyncEnabled ? "On" : "Off",
                              nullptr,
                              device.gpsRtcSyncEnabled ? Theme::GREEN : Theme::GRAY_LIGHT,
                              false, 0, nullptr };

            _menuItemCount = 2;
            break;
        }

        case SETTINGS_ABOUT:
            _menuItems[0] = { "Version", MESHBERRY_VERSION, nullptr, Theme::ACCENT, false, 0, nullptr };
            _menuItems[1] = { "License", "GPL-3.0-or-later", nullptr, Theme::ACCENT, false, 0, nullptr };
            _menuItems[2] = { "Organization", "NodakMesh", nullptr, Theme::ACCENT, false, 0, nullptr };
            _menuItems[3] = { "Website", "nodakmesh.org", nullptr, Theme::ACCENT, false, 0, nullptr };
            _menuItemCount = 4;
            break;
    }

    _listView.setItems(_menuItems, _menuItemCount);
    _listView.setSelectedIndex(0);
}

void SettingsScreen::draw(bool fullRedraw) {
    if (fullRedraw) {
        Display::fillRect(0, Theme::CONTENT_Y,
                          Theme::SCREEN_WIDTH, Theme::CONTENT_HEIGHT,
                          Theme::BG_PRIMARY);

        // Draw title
        Display::drawText(12, Theme::CONTENT_Y + 4, getTitle(), Theme::ACCENT, 2);

        // Divider below title
        Display::drawHLine(12, Theme::CONTENT_Y + 26, Theme::SCREEN_WIDTH - 24, Theme::DIVIDER);
    }

    // Adjust list bounds below title
    _listView.setBounds(0, Theme::CONTENT_Y + 30, Theme::SCREEN_WIDTH, Theme::CONTENT_HEIGHT - 30);
    _listView.draw(fullRedraw);
}

bool SettingsScreen::handleInput(const InputData& input) {
    RadioSettings& radio = SettingsManager::getRadioSettings();

    // Handle editing mode
    if (_editingIndex >= 0 && _currentLevel == SETTINGS_RADIO) {
        bool changed = false;

        if (input.event == InputEvent::TRACKBALL_LEFT) {
            // Decrease value
            switch (_editingIndex) {
                case 0:  // Region
                    if (radio.region > 0) {
                        radio.setRegionPreset((LoRaRegion)(radio.region - 1));
                        changed = true;
                    }
                    break;
                case 1:  // Frequency
                    radio.frequency -= 0.5f;
                    if (radio.frequency < 137.0f) radio.frequency = 137.0f;
                    radio.region = REGION_CUSTOM;
                    changed = true;
                    break;
                case 2:  // SF
                    if (radio.spreadingFactor > SF_MIN) {
                        radio.spreadingFactor--;
                        radio.region = REGION_CUSTOM;
                        changed = true;
                    }
                    break;
                case 3:  // BW
                    if (radio.bandwidth == BW_500) radio.bandwidth = BW_250;
                    else if (radio.bandwidth == BW_250) radio.bandwidth = BW_125;
                    else if (radio.bandwidth == BW_125) radio.bandwidth = BW_62_5;
                    radio.region = REGION_CUSTOM;
                    changed = true;
                    break;
                case 4:  // CR
                    if (radio.codingRate > 5) {
                        radio.codingRate--;
                        radio.region = REGION_CUSTOM;
                        changed = true;
                    }
                    break;
                case 5:  // TX Power
                    if (radio.txPower > TX_POWER_MIN) {
                        radio.txPower--;
                        radio.region = REGION_CUSTOM;
                        changed = true;
                    }
                    break;
            }
        } else if (input.event == InputEvent::TRACKBALL_RIGHT) {
            // Increase value
            switch (_editingIndex) {
                case 0:  // Region
                    if (radio.region < REGION_CUSTOM) {
                        radio.setRegionPreset((LoRaRegion)(radio.region + 1));
                        changed = true;
                    }
                    break;
                case 1:  // Frequency
                    radio.frequency += 0.5f;
                    if (radio.frequency > 1020.0f) radio.frequency = 1020.0f;
                    radio.region = REGION_CUSTOM;
                    changed = true;
                    break;
                case 2:  // SF
                    if (radio.spreadingFactor < SF_MAX) {
                        radio.spreadingFactor++;
                        radio.region = REGION_CUSTOM;
                        changed = true;
                    }
                    break;
                case 3:  // BW
                    if (radio.bandwidth == BW_62_5) radio.bandwidth = BW_125;
                    else if (radio.bandwidth == BW_125) radio.bandwidth = BW_250;
                    else if (radio.bandwidth == BW_250) radio.bandwidth = BW_500;
                    radio.region = REGION_CUSTOM;
                    changed = true;
                    break;
                case 4:  // CR
                    if (radio.codingRate < 8) {
                        radio.codingRate++;
                        radio.region = REGION_CUSTOM;
                        changed = true;
                    }
                    break;
                case 5:  // TX Power
                    if (radio.txPower < TX_POWER_MAX) {
                        radio.txPower++;
                        radio.region = REGION_CUSTOM;
                        changed = true;
                    }
                    break;
            }
        } else if (input.event == InputEvent::TRACKBALL_CLICK ||
                   input.event == InputEvent::BACK) {
            // Exit editing mode
            _editingIndex = -1;
            applyRadioSettings();
            configureSoftKeys();
            buildMenu();
            requestRedraw();
            return true;
        }

        if (changed) {
            buildMenu();
            requestRedraw();
            return true;
        }
    }

    // Normal navigation
    // Treat backspace as back since this screen has no text input
    bool isBackKey = (input.event == InputEvent::KEY_PRESS && input.keyCode == KEY_BACKSPACE);
    if (input.event == InputEvent::BACK ||
        input.event == InputEvent::TRACKBALL_LEFT ||
        isBackKey) {
        if (_currentLevel != SETTINGS_MAIN) {
            _currentLevel = SETTINGS_MAIN;
            buildMenu();
            configureSoftKeys();
            requestRedraw();
            return true;
        } else {
            Screens.goBack();
            return true;
        }
    }

    if (input.event == InputEvent::TRACKBALL_CLICK) {
        onItemSelected(_listView.getSelectedIndex());
        return true;
    }

    // Let list view handle up/down
    if (_listView.handleTrackball(
            input.event == InputEvent::TRACKBALL_UP,
            input.event == InputEvent::TRACKBALL_DOWN,
            false, false,
            false)) {
        requestRedraw();
        return true;
    }

    return false;
}

void SettingsScreen::onItemSelected(int index) {
    switch (_currentLevel) {
        case SETTINGS_MAIN:
            switch (index) {
                case 0: _currentLevel = SETTINGS_RADIO; break;
                case 1: _currentLevel = SETTINGS_DISPLAY; break;
                case 2: _currentLevel = SETTINGS_NETWORK; break;
                case 3: _currentLevel = SETTINGS_GPS; break;
                case 4: _currentLevel = SETTINGS_ABOUT; break;
            }
            buildMenu();
            configureSoftKeys();
            requestRedraw();
            break;

        case SETTINGS_RADIO:
            // Enter editing mode for this item
            _editingIndex = index;
            configureSoftKeys();
            requestRedraw();
            break;

        case SETTINGS_GPS: {
            // Toggle GPS settings
            DeviceSettings& device = SettingsManager::getDeviceSettings();
            switch (index) {
                case 0:  // GPS Always On
                    device.gpsEnabled = !device.gpsEnabled;
                    // Immediately enable/disable GPS
                    if (device.gpsEnabled) {
                        GPS::enable();
                    } else {
                        GPS::disable();
                    }
                    break;
                case 1:  // RTC Auto-Sync
                    device.gpsRtcSyncEnabled = !device.gpsRtcSyncEnabled;
                    break;
            }
            SettingsManager::saveDeviceSettings();
            buildMenu();
            requestRedraw();
            break;
        }

        default:
            // Other submenus - no action yet
            break;
    }
}

void SettingsScreen::applyRadioSettings() {
    RadioSettings& radio = SettingsManager::getRadioSettings();
    LoRa::applySettings(radio);
    SettingsManager::save();
}
