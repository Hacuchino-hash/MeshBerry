/**
 * MeshBerry Messages Screen Implementation
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2026 NodakMesh (nodakmesh.org)
 */

#include "MessagesScreen.h"
#include "ChatScreen.h"
#include "SoftKeyBar.h"
#include "Icons.h"
#include "../drivers/display.h"
#include "../drivers/keyboard.h"
#include "../settings/SettingsManager.h"
#include "../settings/MessageArchive.h"
#include <Arduino.h>
#include <stdio.h>
#include <string.h>

// External ChatScreen instance for navigation
extern ChatScreen chatScreen;

// Static member initialization
MessagesScreen::Conversation MessagesScreen::_conversations[MAX_CONVERSATIONS];
int MessagesScreen::_conversationCount = 0;

MessagesScreen::MessagesScreen() {
    _listView.setBounds(0, Theme::CONTENT_Y + 30, Theme::SCREEN_WIDTH, Theme::CONTENT_HEIGHT - 30);
    _listView.setItemHeight(48);
}

void MessagesScreen::onEnter() {
    buildConversationList();
    requestRedraw();
}

void MessagesScreen::configureSoftKeys() {
    SoftKeyBar::setLabels(nullptr, "Open", "Back");
}

void MessagesScreen::formatTimeAgo(uint32_t timestamp, char* buf, size_t bufSize) {
    if (timestamp == 0) {
        snprintf(buf, bufSize, "");
        return;
    }

    uint32_t now = millis() / 1000;  // Convert to seconds
    if (timestamp > now) {
        // Timestamp is in the future (clock sync issue) or unix timestamp
        // Treat as "just now"
        snprintf(buf, bufSize, "now");
        return;
    }

    uint32_t diff = now - timestamp;

    if (diff < 60) {
        snprintf(buf, bufSize, "%ds", (int)diff);
    } else if (diff < 3600) {
        snprintf(buf, bufSize, "%dm", (int)(diff / 60));
    } else if (diff < 86400) {
        snprintf(buf, bufSize, "%dh", (int)(diff / 3600));
    } else {
        snprintf(buf, bufSize, "%dd", (int)(diff / 86400));
    }
}

void MessagesScreen::buildConversationList() {
    ChannelSettings& channels = SettingsManager::getChannelSettings();
    _conversationCount = 0;

    // Add each active channel as a conversation
    for (int i = 0; i < channels.numChannels && _conversationCount < MAX_CONVERSATIONS - 1; i++) {
        const ChannelEntry& ch = channels.channels[i];
        if (!ch.isActive) continue;

        Conversation& conv = _conversations[_conversationCount];
        strncpy(conv.name, ch.name, sizeof(conv.name) - 1);
        conv.name[sizeof(conv.name) - 1] = '\0';
        conv.channelIdx = i;

        // Copy name to display string
        strncpy(_primaryStrings[_conversationCount], conv.name, sizeof(_primaryStrings[0]) - 1);
        _primaryStrings[_conversationCount][sizeof(_primaryStrings[0]) - 1] = '\0';

        // Build secondary string with more space for preview
        if (conv.lastTimestamp > 0) {
            char timeStr[12];
            formatTimeAgo(conv.lastTimestamp, timeStr, sizeof(timeStr));

            // Use full secondary string buffer for preview
            snprintf(_secondaryStrings[_conversationCount], sizeof(_secondaryStrings[0]),
                     "%s | %s", timeStr, conv.lastMessage);
        } else {
            strncpy(_secondaryStrings[_conversationCount], "No messages yet",
                    sizeof(_secondaryStrings[0]) - 1);
        }
        _secondaryStrings[_conversationCount][sizeof(_secondaryStrings[0]) - 1] = '\0';

        // Build list item - all channels shown equally
        _listItems[_conversationCount] = {
            _primaryStrings[_conversationCount],
            _secondaryStrings[_conversationCount],
            Icons::CHANNEL_ICON,
            Theme::ACCENT,
            conv.unreadCount > 0,
            Theme::RED,
            (void*)(intptr_t)_conversationCount
        };

        _conversationCount++;
    }

    // Add "Manage Channels" item at the end
    if (_conversationCount < MAX_CONVERSATIONS) {
        Conversation& mgmt = _conversations[_conversationCount];
        strcpy(mgmt.name, "Manage Channels");
        mgmt.channelIdx = -1;  // Special marker
        mgmt.lastTimestamp = 0;
        mgmt.unreadCount = 0;

        strcpy(_primaryStrings[_conversationCount], "+ Manage Channels");
        strcpy(_secondaryStrings[_conversationCount], "Add or delete channels");

        _listItems[_conversationCount] = {
            _primaryStrings[_conversationCount],
            _secondaryStrings[_conversationCount],
            Icons::SETTINGS_ICON,
            Theme::GRAY_LIGHT,
            false,
            0,
            (void*)(intptr_t)_conversationCount
        };

        _conversationCount++;
    }

    _listView.setItems(_listItems, _conversationCount);
}

void MessagesScreen::draw(bool fullRedraw) {
    if (fullRedraw) {
        Display::fillRect(0, Theme::CONTENT_Y,
                          Theme::SCREEN_WIDTH, Theme::CONTENT_HEIGHT,
                          Theme::BG_PRIMARY);

        // Modern title card with gradient
        const int16_t titleCardH = 28;
        Display::drawCard(4, Theme::CONTENT_Y + 2, Theme::SCREEN_WIDTH - 8, titleCardH,
                         Theme::COLOR_BG_ELEVATED, Theme::CARD_RADIUS, false);
        Display::fillGradient(4, Theme::CONTENT_Y + 2, Theme::SCREEN_WIDTH - 8, titleCardH,
                             Theme::COLOR_PRIMARY_DARK, Theme::COLOR_BG_ELEVATED);
        Display::drawText(12, Theme::CONTENT_Y + 8, "Messages", Theme::WHITE, 2);

        // Unread count badge (modern style)
        int totalUnread = getUnreadCount();
        if (totalUnread > 0) {
            int16_t badgeX = Theme::SCREEN_WIDTH - 24;
            int16_t badgeY = Theme::CONTENT_Y + 10;
            Display::drawBadge(badgeX, badgeY, totalUnread, COLOR_ERROR);
        }
    }

    // Adjust list bounds below title
    _listView.setBounds(0, Theme::CONTENT_Y + 32, Theme::SCREEN_WIDTH, Theme::CONTENT_HEIGHT - 32);

    if (_conversationCount == 0) {
        Display::drawTextCentered(0, Theme::CONTENT_Y + 80,
                                  Theme::SCREEN_WIDTH,
                                  "No channels", Theme::COLOR_TEXT_SECONDARY, 1);
    } else {
        // Draw items manually with card styling
        int16_t startIdx = _listView.getScrollOffset();
        int16_t listHeight = Theme::CONTENT_HEIGHT - 32;  // Height below title
        int16_t visibleCount = (listHeight / 60) + 1;  // 60px per item

        for (int i = 0; i < visibleCount && (startIdx + i) < _conversationCount; i++) {
            int idx = startIdx + i;
            bool selected = (idx == _listView.getSelectedIndex());
            drawConversationItem(idx, i, selected);
        }
    }
}

void MessagesScreen::drawConversationItem(int idx, int visibleIdx, bool selected) {
    Conversation& conv = _conversations[idx];

    // Calculate position (60px item height, 32px offset for title)
    int16_t y = Theme::CONTENT_Y + 32 + (visibleIdx * 60);
    int16_t x = 4;
    int16_t w = Theme::SCREEN_WIDTH - 8;
    int16_t h = 56;

    // Draw card background
    uint16_t bgColor = selected ? Theme::COLOR_BG_ELEVATED : Theme::COLOR_BG_CARD;
    Display::drawCard(x, y, w, h, bgColor, Theme::CARD_RADIUS, false);

    if (selected) {
        Display::drawRoundRect(x, y, w, h, Theme::CARD_RADIUS, Theme::COLOR_PRIMARY);
    }

    // Icon (channel hash # or management icon for last item)
    int16_t iconX = x + 10;
    int16_t iconY = y + 12;
    uint16_t iconColor = selected ? Theme::COLOR_PRIMARY : Theme::COLOR_TEXT_SECONDARY;

    // Use gear icon for "Manage Channels" item, channel icon for channels
    const uint8_t* icon = (conv.channelIdx == -1) ? Icons::SETTINGS_ICON : Icons::CHANNEL_ICON;
    Display::drawBitmap(iconX, iconY, icon, 16, 16, iconColor);

    // Channel name
    int16_t nameX = iconX + 20;
    int16_t nameY = y + 8;
    Display::drawText(nameX, nameY, conv.name, Theme::COLOR_TEXT_PRIMARY, 1);

    // Time ago (right-aligned) - only for channels, not management item
    if (conv.channelIdx != -1 && conv.lastTimestamp > 0) {
        char timeStr[16];
        uint32_t age = (millis() / 1000) - conv.lastTimestamp;
        if (age < 60) {
            snprintf(timeStr, 16, "%ds", (int)age);
        } else if (age < 3600) {
            snprintf(timeStr, 16, "%dm", (int)(age / 60));
        } else if (age < 86400) {
            snprintf(timeStr, 16, "%dh", (int)(age / 3600));
        } else {
            snprintf(timeStr, 16, "%dd", (int)(age / 86400));
        }
        int16_t timeX = x + w - 30;
        Display::drawText(timeX, nameY, timeStr, Theme::COLOR_TEXT_HINT, 1);
    }

    // Preview text (truncated) - only for channels
    if (conv.channelIdx != -1 && conv.lastMessage[0] != '\0') {
        int16_t previewY = y + 24;
        const char* preview = Theme::truncateText(conv.lastMessage, w - 40, 1);
        Display::drawText(nameX, previewY, preview, Theme::COLOR_TEXT_SECONDARY, 1);
    } else if (conv.channelIdx == -1) {
        // For management item, show description
        int16_t previewY = y + 24;
        Display::drawText(nameX, previewY, "Configure channels", Theme::COLOR_TEXT_SECONDARY, 1);
    }

    // Unread badge (if any) - only for channels
    if (conv.channelIdx != -1 && conv.unreadCount > 0) {
        int16_t badgeX = x + w - 16;
        int16_t badgeY = y + h - 16;
        Display::drawBadge(badgeX, badgeY, conv.unreadCount, COLOR_ERROR);
    }
}

bool MessagesScreen::handleInput(const InputData& input) {
    // Center soft key: "Open" - open selected conversation
    if (input.event == InputEvent::SOFTKEY_CENTER ||
        input.event == InputEvent::TRACKBALL_CLICK) {
        if (_conversationCount > 0) {
            onConversationSelected(_listView.getSelectedIndex());
        }
        return true;
    }

    // Right soft key or Back: go back
    // Treat backspace as back since this screen has no text input
    bool isBackKey = (input.event == InputEvent::KEY_PRESS && input.keyCode == KEY_BACKSPACE);
    if (input.event == InputEvent::SOFTKEY_RIGHT ||
        input.event == InputEvent::BACK ||
        input.event == InputEvent::TRACKBALL_LEFT ||
        isBackKey) {
        Screens.goBack();
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
                if (_conversationCount > 0) {
                    onConversationSelected(_listView.getSelectedIndex());
                }
            }
            // Left soft key has no action on this screen
            return true;
        }

        // List touch - just select, don't open (use soft key or trackball click to open)
        int16_t listTop = Theme::CONTENT_Y + 30;
        if (ty >= listTop && _conversationCount > 0) {
            int itemIndex = _listView.getScrollOffset() + (ty - listTop) / 48;
            if (itemIndex >= 0 && itemIndex < _conversationCount) {
                _listView.setSelectedIndex(itemIndex);
                requestRedraw();
                // Don't auto-open - user taps center soft key or trackball click
            }
        }
        return true;
    }

    return false;
}

void MessagesScreen::onConversationSelected(int index) {
    if (index < 0 || index >= _conversationCount) return;

    const Conversation& conv = _conversations[index];

    if (conv.channelIdx == -1) {
        // "Manage Channels" selected - go to ChannelsScreen
        Screens.navigateTo(ScreenId::CHANNELS);
    } else {
        // Channel selected - go to ChatScreen
        chatScreen.setChannel(conv.channelIdx);
        Screens.navigateTo(ScreenId::CHAT);

        // Clear unread count for this channel
        _conversations[index].unreadCount = 0;

        // Update status bar notification badge
        int unread = getUnreadCount();
        Screens.setNotificationCount(unread);
    }
}

void MessagesScreen::onChannelMessage(int channelIdx, const char* senderAndText, uint32_t timestamp, uint8_t hops) {
    // CRITICAL FIX: Save message to persistent storage IMMEDIATELY
    // This ensures messages are saved regardless of which screen is active
    // Parse sender and text from combined format ("Sender: message")
    char sender[ARCHIVE_SENDER_LEN];
    char text[ARCHIVE_TEXT_LEN];

    const char* colon = strchr(senderAndText, ':');
    if (colon && colon > senderAndText) {
        size_t nameLen = colon - senderAndText;
        if (nameLen >= sizeof(sender)) nameLen = sizeof(sender) - 1;
        strncpy(sender, senderAndText, nameLen);
        sender[nameLen] = '\0';

        // Skip ": " after colon
        const char* msgStart = colon + 1;
        while (*msgStart == ' ') msgStart++;
        strncpy(text, msgStart, sizeof(text) - 1);
        text[sizeof(text) - 1] = '\0';
    } else {
        // No colon found, treat whole thing as text from unknown sender
        strcpy(sender, "Unknown");
        strncpy(text, senderAndText, sizeof(text) - 1);
        text[sizeof(text) - 1] = '\0';
    }

    // Build archived message and save immediately
    ArchivedMessage archived;
    archived.clear();
    archived.timestamp = timestamp;
    strncpy(archived.sender, sender, ARCHIVE_SENDER_LEN - 1);
    archived.sender[ARCHIVE_SENDER_LEN - 1] = '\0';
    strncpy(archived.text, text, ARCHIVE_TEXT_LEN - 1);
    archived.text[ARCHIVE_TEXT_LEN - 1] = '\0';
    archived.isOutgoing = 0;  // Incoming message

    MessageArchive::saveChannelMessage(channelIdx, archived);
    Serial.printf("[MESSAGES] Saved channel message: ch=%d, sender=%s\n", channelIdx, sender);

    // Find the conversation for this channel
    for (int i = 0; i < _conversationCount; i++) {
        if (_conversations[i].channelIdx == channelIdx) {
            // Update last message preview
            strncpy(_conversations[i].lastMessage, senderAndText, sizeof(_conversations[i].lastMessage) - 1);
            _conversations[i].lastMessage[sizeof(_conversations[i].lastMessage) - 1] = '\0';
            _conversations[i].lastTimestamp = timestamp;
            _conversations[i].unreadCount++;

            // Forward to ChatScreen with hop info if it's showing this channel
            ChatScreen::addToCurrentChat(channelIdx, senderAndText, timestamp, hops);
            return;
        }
    }

    // Channel not found in conversation list yet
    // This can happen if a message arrives for a newly added channel
    // The conversation list will be rebuilt on next onEnter()
}

int MessagesScreen::getUnreadCount() {
    int total = 0;
    for (int i = 0; i < _conversationCount; i++) {
        if (_conversations[i].channelIdx >= 0) {
            total += _conversations[i].unreadCount;
        }
    }
    return total;
}
