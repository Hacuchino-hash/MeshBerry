/**
 * MeshBerry Channels Screen Implementation
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2026 NodakMesh (nodakmesh.org)
 */

#include "ChannelsScreen.h"
#include "ChatScreen.h"
#include "SoftKeyBar.h"
#include "Icons.h"
#include "../drivers/display.h"
#include "../drivers/keyboard.h"
#include "../settings/SettingsManager.h"
#include "../settings/MessageArchive.h"
#include <stdio.h>
#include <string.h>

void ChannelsScreen::onEnter() {
    _state = STATE_LIST;
    _selectedIndex = 0;
    _inputPos = 0;
    _inputBuffer[0] = '\0';
    _pendingName[0] = '\0';
    _deleteIndex = -1;

    _listView.setBounds(0, Theme::CONTENT_Y + 30, Theme::SCREEN_WIDTH, Theme::CONTENT_HEIGHT - 30);
    _listView.setItemHeight(44);

    buildChannelList();
    requestRedraw();
}

void ChannelsScreen::onExit() {
    // Save any pending changes
    SettingsManager::save();
}

const char* ChannelsScreen::getTitle() const {
    switch (_state) {
        case STATE_ADD_TYPE:      return "Add Channel";
        case STATE_ADD_HASHTAG:   return "Hashtag Channel";
        case STATE_ADD_PSK_NAME:  return "Channel Name";
        case STATE_ADD_PSK_KEY:   return "Enter PSK";
        case STATE_CONFIRM_DELETE: return "Delete Channel";
        default:                  return "Channels";
    }
}

void ChannelsScreen::configureSoftKeys() {
    switch (_state) {
        case STATE_LIST:
            SoftKeyBar::setLabels("Add", "Open", "Back");
            break;
        case STATE_ADD_TYPE:
            SoftKeyBar::setLabels(nullptr, "Select", "Cancel");
            break;
        case STATE_ADD_HASHTAG:
        case STATE_ADD_PSK_NAME:
        case STATE_ADD_PSK_KEY:
            SoftKeyBar::setLabels("Clear", "OK", "Cancel");
            break;
        case STATE_CONFIRM_DELETE:
            SoftKeyBar::setLabels("No", "Yes", nullptr);
            break;
    }
}

void ChannelsScreen::buildChannelList() {
    ChannelSettings& channels = SettingsManager::getChannelSettings();
    int count = 0;

    for (int i = 0; i < channels.numChannels && i < MAX_CHANNEL_ITEMS; i++) {
        const ChannelEntry& ch = channels.channels[i];
        if (!ch.isActive) continue;

        // Primary string: channel name with # prefix for hashtag channels
        if (ch.isHashtag) {
            snprintf(_primaryStrings[count], sizeof(_primaryStrings[count]), "#%s", ch.name);
        } else {
            strncpy(_primaryStrings[count], ch.name, sizeof(_primaryStrings[count]) - 1);
            _primaryStrings[count][sizeof(_primaryStrings[count]) - 1] = '\0';
        }

        // Secondary string: last message preview or "No messages"
        int msgCount = MessageArchive::getChannelMessageCount(i);
        if (msgCount > 0) {
            // Load just the last message
            ArchivedMessage lastMsg;
            MessageArchive::loadChannelMessages(i, &lastMsg, 1);
            // Format: "sender: text" truncated to fit
            snprintf(_secondaryStrings[count], sizeof(_secondaryStrings[count]),
                     "%s: %s", lastMsg.sender, lastMsg.text);
        } else {
            strcpy(_secondaryStrings[count], "No messages");
        }

        // Build list item - all channels shown equally
        _channelItems[count] = {
            _primaryStrings[count],
            _secondaryStrings[count],
            Icons::CHANNEL_ICON,
            Theme::ACCENT,
            false,  // No indicator
            Theme::ACCENT,
            (void*)(intptr_t)i  // Store original index
        };

        count++;
    }

    _listView.setItems(_channelItems, count);
    if (_selectedIndex >= count) {
        _selectedIndex = count > 0 ? count - 1 : 0;
    }
    _listView.setSelectedIndex(_selectedIndex);
}

void ChannelsScreen::draw(bool fullRedraw) {
    switch (_state) {
        case STATE_LIST:
            drawList(fullRedraw);
            break;
        case STATE_ADD_TYPE:
            drawAddTypeMenu(fullRedraw);
            break;
        case STATE_ADD_HASHTAG:
            drawTextInput("Hashtag Channel", "Enter channel name:", "#", fullRedraw);
            break;
        case STATE_ADD_PSK_NAME:
            drawTextInput("Custom Channel", "Enter channel name:", nullptr, fullRedraw);
            break;
        case STATE_ADD_PSK_KEY:
            drawTextInput("Enter PSK", "Base64 encryption key:", nullptr, fullRedraw);
            break;
        case STATE_CONFIRM_DELETE:
            drawConfirmDelete(fullRedraw);
            break;
    }
}

void ChannelsScreen::drawList(bool fullRedraw) {
    if (fullRedraw) {
        Display::fillRect(0, Theme::CONTENT_Y,
                          Theme::SCREEN_WIDTH, Theme::CONTENT_HEIGHT,
                          Theme::BG_PRIMARY);

        // Draw title
        Display::drawText(12, Theme::CONTENT_Y + 4, "Channels", Theme::ACCENT, 2);

        // Channel count
        ChannelSettings& channels = SettingsManager::getChannelSettings();
        char countStr[16];
        snprintf(countStr, sizeof(countStr), "%d/%d", channels.numChannels, MAX_CHANNELS);
        Display::drawText(Theme::SCREEN_WIDTH - 50, Theme::CONTENT_Y + 8, countStr, Theme::TEXT_SECONDARY, 1);

        // Divider below title
        Display::drawHLine(12, Theme::CONTENT_Y + 26, Theme::SCREEN_WIDTH - 24, Theme::DIVIDER);
    }

    ChannelSettings& channels = SettingsManager::getChannelSettings();
    if (channels.numChannels == 0) {
        Display::drawTextCentered(0, Theme::CONTENT_Y + 80,
                                  Theme::SCREEN_WIDTH,
                                  "No channels", Theme::TEXT_SECONDARY, 1);
    } else {
        _listView.draw(fullRedraw);
    }
}

void ChannelsScreen::drawAddTypeMenu(bool fullRedraw) {
    if (fullRedraw) {
        Display::fillRect(0, Theme::CONTENT_Y,
                          Theme::SCREEN_WIDTH, Theme::CONTENT_HEIGHT,
                          Theme::BG_PRIMARY);

        // Title
        Display::drawText(12, Theme::CONTENT_Y + 4, "Add Channel", Theme::ACCENT, 2);
        Display::drawHLine(12, Theme::CONTENT_Y + 26, Theme::SCREEN_WIDTH - 24, Theme::DIVIDER);

        // Instructions
        Display::drawText(12, Theme::CONTENT_Y + 40, "Select channel type:", Theme::TEXT_SECONDARY, 1);
    }

    // Option 1: Hashtag
    int16_t y1 = Theme::CONTENT_Y + 70;
    uint16_t bg1 = (_addTypeSelection == 0) ? Theme::BLUE_DARK : Theme::BG_SECONDARY;
    uint16_t text1 = (_addTypeSelection == 0) ? Theme::WHITE : Theme::TEXT_PRIMARY;
    Display::fillRoundRect(12, y1, Theme::SCREEN_WIDTH - 24, 40, Theme::RADIUS_SMALL, bg1);
    if (_addTypeSelection == 0) {
        Display::drawRoundRect(12, y1, Theme::SCREEN_WIDTH - 24, 40, Theme::RADIUS_SMALL, Theme::BLUE);
    }
    Display::drawText(24, y1 + 8, "Hashtag Channel", text1, 1);
    Display::drawText(24, y1 + 22, "Auto-generated key from name", Theme::TEXT_SECONDARY, 1);

    // Option 2: Custom PSK
    int16_t y2 = Theme::CONTENT_Y + 120;
    uint16_t bg2 = (_addTypeSelection == 1) ? Theme::BLUE_DARK : Theme::BG_SECONDARY;
    uint16_t text2 = (_addTypeSelection == 1) ? Theme::WHITE : Theme::TEXT_PRIMARY;
    Display::fillRoundRect(12, y2, Theme::SCREEN_WIDTH - 24, 40, Theme::RADIUS_SMALL, bg2);
    if (_addTypeSelection == 1) {
        Display::drawRoundRect(12, y2, Theme::SCREEN_WIDTH - 24, 40, Theme::RADIUS_SMALL, Theme::BLUE);
    }
    Display::drawText(24, y2 + 8, "Custom PSK", text2, 1);
    Display::drawText(24, y2 + 22, "Enter your own encryption key", Theme::TEXT_SECONDARY, 1);
}

void ChannelsScreen::drawTextInput(const char* title, const char* prompt, const char* prefix, bool fullRedraw) {
    if (fullRedraw) {
        Display::fillRect(0, Theme::CONTENT_Y,
                          Theme::SCREEN_WIDTH, Theme::CONTENT_HEIGHT,
                          Theme::BG_PRIMARY);

        // Title
        Display::drawText(12, Theme::CONTENT_Y + 4, title, Theme::ACCENT, 2);
        Display::drawHLine(12, Theme::CONTENT_Y + 26, Theme::SCREEN_WIDTH - 24, Theme::DIVIDER);

        // Prompt
        Display::drawText(12, Theme::CONTENT_Y + 40, prompt, Theme::TEXT_SECONDARY, 1);
    }

    // Input box background
    int16_t inputY = Theme::CONTENT_Y + 60;
    Display::fillRoundRect(12, inputY, Theme::SCREEN_WIDTH - 24, 36, Theme::RADIUS_SMALL, Theme::BG_SECONDARY);
    Display::drawRoundRect(12, inputY, Theme::SCREEN_WIDTH - 24, 36, Theme::RADIUS_SMALL, Theme::ACCENT);

    // Input text with prefix and cursor
    char displayText[72];
    if (prefix) {
        snprintf(displayText, sizeof(displayText), "%s%s_", prefix, _inputBuffer);
    } else {
        snprintf(displayText, sizeof(displayText), "%s_", _inputBuffer);
    }
    Display::drawText(20, inputY + 12, displayText, Theme::WHITE, 1);

    // Character count
    char countStr[16];
    int maxLen = (_state == STATE_ADD_PSK_KEY) ? 44 : 30;
    snprintf(countStr, sizeof(countStr), "%d/%d", _inputPos, maxLen);
    Display::drawText(Theme::SCREEN_WIDTH - 50, inputY + 44, countStr, Theme::TEXT_SECONDARY, 1);

    // Help text for PSK entry
    if (_state == STATE_ADD_PSK_KEY) {
        Display::drawText(12, Theme::CONTENT_Y + 120, "Example: izOH6cXN6mrJ5e26oRXNcg==", Theme::GRAY_LIGHT, 1);
        Display::drawText(12, Theme::CONTENT_Y + 136, "(Base64 encoded, 16 or 32 bytes)", Theme::GRAY_LIGHT, 1);
    }
}

void ChannelsScreen::drawConfirmDelete(bool fullRedraw) {
    if (fullRedraw) {
        Display::fillRect(0, Theme::CONTENT_Y,
                          Theme::SCREEN_WIDTH, Theme::CONTENT_HEIGHT,
                          Theme::BG_PRIMARY);

        // Title
        Display::drawText(12, Theme::CONTENT_Y + 4, "Delete Channel", Theme::RED, 2);
        Display::drawHLine(12, Theme::CONTENT_Y + 26, Theme::SCREEN_WIDTH - 24, Theme::DIVIDER);

        // Warning message
        Display::drawText(12, Theme::CONTENT_Y + 50, "Are you sure you want to delete:", Theme::TEXT_SECONDARY, 1);

        // Channel name
        ChannelSettings& channels = SettingsManager::getChannelSettings();
        if (_deleteIndex >= 0 && _deleteIndex < (int)channels.numChannels) {
            const ChannelEntry& ch = channels.channels[_deleteIndex];
            char name[40];
            if (ch.isHashtag) {
                snprintf(name, sizeof(name), "#%s", ch.name);
            } else {
                strncpy(name, ch.name, sizeof(name) - 1);
                name[sizeof(name) - 1] = '\0';
            }
            Display::drawText(12, Theme::CONTENT_Y + 80, name, Theme::WHITE, 2);
        }

        Display::drawText(12, Theme::CONTENT_Y + 120, "This action cannot be undone.", Theme::YELLOW, 1);
    }
}

bool ChannelsScreen::handleInput(const InputData& input) {
    switch (_state) {
        case STATE_LIST:
            return handleListInput(input);
        case STATE_ADD_TYPE:
            return handleAddTypeInput(input);
        case STATE_ADD_HASHTAG:
        case STATE_ADD_PSK_NAME:
        case STATE_ADD_PSK_KEY:
            return handleTextInput(input);
        case STATE_CONFIRM_DELETE:
            return handleConfirmInput(input);
    }
    return false;
}

bool ChannelsScreen::handleListInput(const InputData& input) {
    // Left soft key: Add channel
    if (input.event == InputEvent::SOFTKEY_LEFT) {
        startAddChannel();
        return true;
    }

    // Center soft key or trackball click: Open channel
    if (input.event == InputEvent::SOFTKEY_CENTER ||
        input.event == InputEvent::TRACKBALL_CLICK) {
        openSelectedChannel();
        return true;
    }

    // Right soft key or Back: go back
    // Treat backspace as back in list view (not in text entry modes)
    bool isBackKey = (input.event == InputEvent::KEY_PRESS && input.keyCode == KEY_BACKSPACE);
    if (input.event == InputEvent::SOFTKEY_RIGHT ||
        input.event == InputEvent::BACK ||
        isBackKey) {
        Screens.goBack();
        return true;
    }

    // A key: Add channel
    if (input.event == InputEvent::KEY_PRESS &&
        (input.keyChar == 'a' || input.keyChar == 'A')) {
        startAddChannel();
        return true;
    }

    // Let list view handle up/down
    if (_listView.handleTrackball(
            input.event == InputEvent::TRACKBALL_UP,
            input.event == InputEvent::TRACKBALL_DOWN,
            false, false,
            false)) {
        _selectedIndex = _listView.getSelectedIndex();
        requestRedraw();
        return true;
    }

    // Handle touch tap
    if (input.event == InputEvent::TOUCH_TAP) {
        int16_t ty = input.touchY;
        int16_t tx = input.touchX;

        // Soft key bar touch (Y >= 210)
        if (ty >= Theme::SOFTKEY_BAR_Y) {
            if (tx >= 214) {
                // Right soft key = Back
                Screens.goBack();
            } else if (tx >= 107) {
                // Center soft key = Open
                openSelectedChannel();
            } else {
                // Left soft key = Add
                startAddChannel();
            }
            return true;
        }

        // Touch on list area - check if tapping a channel item
        int16_t listStartY = _listView.getY();
        if (ty >= listStartY && ty < Theme::SOFTKEY_BAR_Y) {
            // Calculate which item was tapped (accounting for scroll offset)
            int itemHeight = _listView.getItemHeight();
            int listY = ty - listStartY;
            int visualIdx = listY / itemHeight;
            int tappedIdx = visualIdx + _listView.getScrollOffset();

            Serial.printf("[CHANNELS] Touch at y=%d, listY=%d, visualIdx=%d, tappedIdx=%d\n",
                          ty, listY, visualIdx, tappedIdx);

            if (tappedIdx >= 0 && tappedIdx < _listView.getItemCount()) {
                _selectedIndex = tappedIdx;
                _listView.setSelectedIndex(_selectedIndex);
                openSelectedChannel();
            }
            return true;
        }

        return true;
    }

    return false;
}

bool ChannelsScreen::handleAddTypeInput(const InputData& input) {
    // Up/Down: change selection
    if (input.event == InputEvent::TRACKBALL_UP) {
        if (_addTypeSelection > 0) {
            _addTypeSelection--;
            requestRedraw();
        }
        return true;
    }
    if (input.event == InputEvent::TRACKBALL_DOWN) {
        if (_addTypeSelection < 1) {
            _addTypeSelection++;
            requestRedraw();
        }
        return true;
    }

    // Select
    if (input.event == InputEvent::SOFTKEY_CENTER ||
        input.event == InputEvent::TRACKBALL_CLICK) {
        clearInput();
        if (_addTypeSelection == 0) {
            _state = STATE_ADD_HASHTAG;
        } else {
            _state = STATE_ADD_PSK_NAME;
        }
        configureSoftKeys();
        requestRedraw();
        return true;
    }

    // Cancel
    if (input.event == InputEvent::SOFTKEY_RIGHT ||
        input.event == InputEvent::BACK) {
        cancelAction();
        return true;
    }

    return false;
}

bool ChannelsScreen::handleTextInput(const InputData& input) {
    int maxLen;
    if (_state == STATE_ADD_PSK_KEY) {
        maxLen = 44;  // Base64 for 32-byte key
    } else {
        maxLen = 30;  // Channel names
    }

    // Left soft key: Clear
    if (input.event == InputEvent::SOFTKEY_LEFT) {
        clearInput();
        requestRedraw();
        return true;
    }

    // Right soft key or Back: Cancel
    if (input.event == InputEvent::SOFTKEY_RIGHT ||
        input.event == InputEvent::BACK) {
        cancelAction();
        return true;
    }

    // Center soft key or Enter: Confirm
    if (input.event == InputEvent::SOFTKEY_CENTER) {
        if (_inputPos > 0) {
            if (_state == STATE_ADD_HASHTAG) {
                createHashtagChannel();
            } else if (_state == STATE_ADD_PSK_NAME) {
                // Save name and move to PSK entry
                strncpy(_pendingName, _inputBuffer, sizeof(_pendingName) - 1);
                _pendingName[sizeof(_pendingName) - 1] = '\0';
                clearInput();
                _state = STATE_ADD_PSK_KEY;
                configureSoftKeys();
                requestRedraw();
            } else if (_state == STATE_ADD_PSK_KEY) {
                createPskChannel();
            }
        }
        return true;
    }

    // Handle keyboard input
    if (input.event == InputEvent::KEY_PRESS) {
        char c = input.keyChar;

        // Backspace
        if (input.keyCode == KEY_BACKSPACE && _inputPos > 0) {
            _inputPos--;
            _inputBuffer[_inputPos] = '\0';
            requestRedraw();
            return true;
        }

        // Printable character
        if (c >= 32 && c < 127 && _inputPos < maxLen) {
            // For hashtag mode, don't allow # character
            if (_state == STATE_ADD_HASHTAG && c == '#') {
                return true;
            }
            _inputBuffer[_inputPos++] = c;
            _inputBuffer[_inputPos] = '\0';
            requestRedraw();
            return true;
        }
    }

    return false;
}

bool ChannelsScreen::handleConfirmInput(const InputData& input) {
    // Handle touch tap for soft keys
    if (input.event == InputEvent::TOUCH_TAP) {
        int16_t ty = input.touchY;
        int16_t tx = input.touchX;

        // Soft key bar touch (Y >= 210)
        if (ty >= Theme::SOFTKEY_BAR_Y) {
            if (tx < 107) {
                // Left soft key = No (cancel)
                cancelAction();
            } else if (tx >= 107 && tx < 214) {
                // Center soft key = Yes (confirm delete)
                confirmDelete();
            }
            return true;
        }
        return true;
    }

    // Left soft key or N: No (cancel)
    if (input.event == InputEvent::SOFTKEY_LEFT ||
        (input.event == InputEvent::KEY_PRESS &&
         (input.keyChar == 'n' || input.keyChar == 'N'))) {
        cancelAction();
        return true;
    }

    // Center soft key or Y: Yes (confirm delete)
    if (input.event == InputEvent::SOFTKEY_CENTER ||
        (input.event == InputEvent::KEY_PRESS &&
         (input.keyChar == 'y' || input.keyChar == 'Y'))) {
        confirmDelete();
        return true;
    }

    // Back: cancel
    if (input.event == InputEvent::BACK) {
        cancelAction();
        return true;
    }

    return false;
}

void ChannelsScreen::openSelectedChannel() {
    ChannelSettings& channels = SettingsManager::getChannelSettings();
    if (_selectedIndex >= 0 && _selectedIndex < _listView.getItemCount()) {
        // Get the actual channel index from userData
        int channelIdx = (int)(intptr_t)_channelItems[_selectedIndex].userData;

        // Navigate to ChatScreen for this channel
        extern ChatScreen chatScreen;
        chatScreen.setChannel(channelIdx);
        Screens.navigateTo(ScreenId::CHAT);
    }
}

void ChannelsScreen::startAddChannel() {
    ChannelSettings& channels = SettingsManager::getChannelSettings();
    if (channels.numChannels >= MAX_CHANNELS) {
        Screens.showStatus("Max channels reached", 2000);
        return;
    }

    _addTypeSelection = 0;
    _state = STATE_ADD_TYPE;
    configureSoftKeys();
    requestRedraw();
}

void ChannelsScreen::startDeleteChannel() {
    ChannelSettings& channels = SettingsManager::getChannelSettings();

    // Get the actual channel index
    int channelIdx = (int)(intptr_t)_channelItems[_selectedIndex].userData;

    // Can't delete Public channel (index 0)
    if (channelIdx == 0) {
        Screens.showStatus("Cannot delete Public", 2000);
        return;
    }

    _deleteIndex = channelIdx;
    _state = STATE_CONFIRM_DELETE;
    configureSoftKeys();
    requestRedraw();
}

void ChannelsScreen::createHashtagChannel() {
    ChannelSettings& channels = SettingsManager::getChannelSettings();
    // Add # prefix - addHashtagChannel expects the name to start with #
    char fullName[32];
    snprintf(fullName, sizeof(fullName), "#%s", _inputBuffer);
    int idx = channels.addHashtagChannel(fullName);
    if (idx >= 0) {
        SettingsManager::save();
        buildChannelList();
        _state = STATE_LIST;
        configureSoftKeys();
        Screens.showStatus("Channel created", 1500);
    } else {
        Screens.showStatus("Failed to create", 2000);
        _state = STATE_LIST;
        configureSoftKeys();
    }
    requestRedraw();
}

void ChannelsScreen::createPskChannel() {
    ChannelSettings& channels = SettingsManager::getChannelSettings();
    int idx = channels.addChannel(_pendingName, _inputBuffer);
    if (idx >= 0) {
        SettingsManager::save();
        buildChannelList();
        _state = STATE_LIST;
        configureSoftKeys();
        Screens.showStatus("Channel created", 1500);
    } else {
        Screens.showStatus("Invalid PSK format", 2000);
        _state = STATE_LIST;
        configureSoftKeys();
    }
    requestRedraw();
}

void ChannelsScreen::confirmDelete() {
    ChannelSettings& channels = SettingsManager::getChannelSettings();
    if (channels.removeChannel(_deleteIndex)) {
        SettingsManager::save();
        buildChannelList();
        Screens.showStatus("Channel deleted", 1500);
    } else {
        Screens.showStatus("Delete failed", 2000);
    }
    _deleteIndex = -1;
    _state = STATE_LIST;
    configureSoftKeys();
    requestRedraw();
}

void ChannelsScreen::cancelAction() {
    _state = STATE_LIST;
    _deleteIndex = -1;
    clearInput();
    configureSoftKeys();
    requestRedraw();
}

void ChannelsScreen::clearInput() {
    _inputPos = 0;
    _inputBuffer[0] = '\0';
}
