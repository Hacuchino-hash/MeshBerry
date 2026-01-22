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
#include "../drivers/audio.h"
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
        case SETTINGS_POWER:   return "Sleep Mode";
        case SETTINGS_AUDIO:   return "Audio";
        case SETTINGS_ABOUT:   return "About";
        default:               return "Settings";
    }
}

void SettingsScreen::configureSoftKeys() {
    if (_editingIndex >= 0) {
        SoftKeyBar::setLabels("<", "OK", ">");
    } else if (_currentLevel == SETTINGS_MAIN) {
        SoftKeyBar::setLabels(nullptr, "Select", "Back");
    } else if (_currentLevel == SETTINGS_GPS || _currentLevel == SETTINGS_POWER || _currentLevel == SETTINGS_AUDIO) {
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
            _menuItems[3] = { "GPS", "Power, RTC sync options", Icons::GPS_ICON, Theme::ACCENT, false, 0, nullptr };
            _menuItems[4] = { "Power", "Sleep timeout, wake sources", Icons::SETTINGS_ICON, Theme::ACCENT, false, 0, nullptr };
            _menuItems[5] = { "Audio", "Volume, notification tones", Icons::SETTINGS_ICON, Theme::ACCENT, false, 0, nullptr };
            _menuItems[6] = { "About", "Version, licenses", Icons::INFO_ICON, Theme::ACCENT, false, 0, nullptr };
            _menuItemCount = 7;
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

        case SETTINGS_POWER: {
            // Sleep mode settings (sleep is always enabled for power savings)
            DeviceSettings& device = SettingsManager::getDeviceSettings();

            // Sleep timeout (convert seconds to readable format)
            if (device.sleepTimeoutSecs < 60) {
                snprintf(_valueStrings[0], 32, "%d sec", device.sleepTimeoutSecs);
            } else {
                snprintf(_valueStrings[0], 32, "%d min", device.sleepTimeoutSecs / 60);
            }
            _menuItems[0] = { "Sleep After",
                              _valueStrings[0],
                              nullptr,
                              Theme::ACCENT,
                              false, 0, nullptr };

            // LoRa wake uses LIGHT SLEEP since GPIO 45 isn't RTC-capable
            _menuItems[1] = { "Wake on LoRa",
                              device.wakeOnLoRa ? "On" : "Off",
                              nullptr,
                              device.wakeOnLoRa ? Theme::GREEN : Theme::GRAY_LIGHT,
                              false, 0, nullptr };

            _menuItems[2] = { "Wake on Button",
                              device.wakeOnButton ? "On" : "Off",
                              nullptr,
                              device.wakeOnButton ? Theme::GREEN : Theme::GRAY_LIGHT,
                              false, 0, nullptr };

            // Deep sleep option - lower power but reboots on wake, no LoRa wake
            _menuItems[3] = { "Deep Sleep Mode",
                              device.useDeepSleep ? "On" : "Off",
                              nullptr,
                              device.useDeepSleep ? Theme::ORANGE : Theme::GRAY_LIGHT,
                              false, 0, nullptr };

            _menuItemCount = 4;
            break;
        }

        case SETTINGS_AUDIO: {
            // Audio settings
            DeviceSettings& device = SettingsManager::getDeviceSettings();

            snprintf(_valueStrings[0], 32, "%d%%", device.audioVolume);
            _menuItems[0] = { "Volume",
                              _valueStrings[0],
                              nullptr,
                              Theme::ACCENT,
                              false, 0, nullptr };

            _menuItems[1] = { "Mute All",
                              device.audioMuted ? "On" : "Off",
                              nullptr,
                              device.audioMuted ? Theme::RED : Theme::GRAY_LIGHT,
                              false, 0, nullptr };

            _menuItems[2] = { "Message Tone",
                              getAlertToneName(device.toneMessage),
                              nullptr,
                              Theme::ACCENT,
                              false, 0, nullptr };

            _menuItems[3] = { "Connect Tone",
                              getAlertToneName(device.toneNodeConnect),
                              nullptr,
                              Theme::ACCENT,
                              false, 0, nullptr };

            _menuItems[4] = { "Low Battery",
                              getAlertToneName(device.toneLowBattery),
                              nullptr,
                              Theme::ACCENT,
                              false, 0, nullptr };

            _menuItems[5] = { "Sent Tone",
                              getAlertToneName(device.toneSent),
                              nullptr,
                              Theme::ACCENT,
                              false, 0, nullptr };

            _menuItems[6] = { "Error Tone",
                              getAlertToneName(device.toneError),
                              nullptr,
                              Theme::ACCENT,
                              false, 0, nullptr };

            _menuItemCount = 7;
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

        // Modern title area with card background
        const int16_t titleCardH = 28;
        Display::drawCard(4, Theme::CONTENT_Y + 2, Theme::SCREEN_WIDTH - 8, titleCardH,
                         Theme::COLOR_BG_ELEVATED, Theme::CARD_RADIUS, false);

        // Draw title with gradient background
        Display::fillGradient(4, Theme::CONTENT_Y + 2, Theme::SCREEN_WIDTH - 8, titleCardH,
                             Theme::COLOR_PRIMARY_DARK, Theme::COLOR_BG_ELEVATED);

        // Title text (centered vertically in card)
        Display::drawText(12, Theme::CONTENT_Y + 8, getTitle(), Theme::WHITE, 2);

        // Visual separator between title and list
        Display::drawHLine(4, Theme::CONTENT_Y + 32, Theme::SCREEN_WIDTH - 8, Theme::COLOR_DIVIDER);
    }

    // Adjust list bounds below title card
    _listView.setBounds(0, Theme::CONTENT_Y + 36, Theme::SCREEN_WIDTH, Theme::CONTENT_HEIGHT - 36);
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
                   input.event == InputEvent::SOFTKEY_CENTER ||
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

    if (input.event == InputEvent::TRACKBALL_CLICK ||
        input.event == InputEvent::SOFTKEY_CENTER) {
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

    // Handle touch tap (only when not editing)
    if (input.event == InputEvent::TOUCH_TAP && _editingIndex < 0) {
        int16_t ty = input.touchY;
        int16_t tx = input.touchX;

        // Soft key bar touch (Y >= 210)
        if (ty >= Theme::SOFTKEY_BAR_Y) {
            if (tx >= 214) {
                // Right soft key = Back
                if (_currentLevel != SETTINGS_MAIN) {
                    _currentLevel = SETTINGS_MAIN;
                    buildMenu();
                    configureSoftKeys();
                    requestRedraw();
                } else {
                    Screens.goBack();
                }
            } else if (tx >= 107) {
                // Center soft key = Select/Edit/Toggle
                onItemSelected(_listView.getSelectedIndex());
            }
            // Left soft key has no action
            return true;
        }

        // List touch - starts at CONTENT_Y + 30 (below title), item height is 40
        int16_t listTop = Theme::CONTENT_Y + 30;
        if (ty >= listTop && _menuItemCount > 0) {
            int itemIndex = _listView.getScrollOffset() + (ty - listTop) / 40;
            if (itemIndex >= 0 && itemIndex < _menuItemCount) {
                _listView.setSelectedIndex(itemIndex);
                requestRedraw();
                onItemSelected(itemIndex);
            }
        }
        return true;
    }

    return false;
}

// Helper to cycle through AlertTone values
static AlertTone cycleAlertTone(AlertTone current) {
    uint8_t next = (uint8_t)current + 1;
    if (next >= (uint8_t)TONE_COUNT) {
        next = 0;  // Wrap to TONE_NONE
    }
    return (AlertTone)next;
}

void SettingsScreen::onItemSelected(int index) {
    switch (_currentLevel) {
        case SETTINGS_MAIN:
            switch (index) {
                case 0: _currentLevel = SETTINGS_RADIO; break;
                case 1: _currentLevel = SETTINGS_DISPLAY; break;
                case 2: _currentLevel = SETTINGS_NETWORK; break;
                case 3: _currentLevel = SETTINGS_GPS; break;
                case 4: _currentLevel = SETTINGS_POWER; break;
                case 5: _currentLevel = SETTINGS_AUDIO; break;
                case 6: _currentLevel = SETTINGS_ABOUT; break;
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

        case SETTINGS_POWER: {
            // Toggle sleep settings
            DeviceSettings& device = SettingsManager::getDeviceSettings();
            switch (index) {
                case 0:  // Sleep After (cycle through values: 1, 2, 5, 10 minutes)
                    if (device.sleepTimeoutSecs <= 60) {
                        device.sleepTimeoutSecs = 120;    // 2 min
                    } else if (device.sleepTimeoutSecs <= 120) {
                        device.sleepTimeoutSecs = 300;    // 5 min
                    } else if (device.sleepTimeoutSecs <= 300) {
                        device.sleepTimeoutSecs = 600;    // 10 min
                    } else {
                        device.sleepTimeoutSecs = 60;     // Back to 1 min
                    }
                    break;
                case 1:  // Wake on LoRa (uses light sleep)
                    device.wakeOnLoRa = !device.wakeOnLoRa;
                    break;
                case 2:  // Wake on Button
                    device.wakeOnButton = !device.wakeOnButton;
                    break;
                case 3:  // Deep Sleep Mode
                    device.useDeepSleep = !device.useDeepSleep;
                    // If deep sleep enabled, disable LoRa wake (not supported)
                    if (device.useDeepSleep) {
                        device.wakeOnLoRa = false;
                    }
                    break;
            }
            SettingsManager::saveDeviceSettings();
            buildMenu();
            requestRedraw();
            break;
        }

        case SETTINGS_AUDIO: {
            // Toggle audio settings
            DeviceSettings& device = SettingsManager::getDeviceSettings();
            switch (index) {
                case 0:  // Volume (cycle: 0, 20, 40, 60, 80, 100)
                    device.audioVolume = (device.audioVolume + 20) % 120;
                    if (device.audioVolume > 100) device.audioVolume = 0;
                    Audio::setVolume(device.audioVolume);
                    break;
                case 1:  // Mute All
                    device.audioMuted = !device.audioMuted;
                    if (device.audioMuted) {
                        Audio::mute();
                    } else {
                        Audio::unmute();
                    }
                    break;
                case 2:  // Message Tone
                    device.toneMessage = cycleAlertTone(device.toneMessage);
                    Audio::playAlertTone(device.toneMessage);  // Preview
                    break;
                case 3:  // Connect Tone
                    device.toneNodeConnect = cycleAlertTone(device.toneNodeConnect);
                    Audio::playAlertTone(device.toneNodeConnect);  // Preview
                    break;
                case 4:  // Low Battery Tone
                    device.toneLowBattery = cycleAlertTone(device.toneLowBattery);
                    Audio::playAlertTone(device.toneLowBattery);  // Preview
                    break;
                case 5:  // Sent Tone
                    device.toneSent = cycleAlertTone(device.toneSent);
                    Audio::playAlertTone(device.toneSent);  // Preview
                    break;
                case 6:  // Error Tone
                    device.toneError = cycleAlertTone(device.toneError);
                    Audio::playAlertTone(device.toneError);  // Preview
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
