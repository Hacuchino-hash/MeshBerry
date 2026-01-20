/**
 * MeshBerry Channels Screen
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2026 NodakMesh (nodakmesh.org)
 *
 * Channel management screen - view, add, switch, and delete channels
 */

#ifndef MESHBERRY_CHANNELSSCREEN_H
#define MESHBERRY_CHANNELSSCREEN_H

#include "Screen.h"
#include "ScreenManager.h"
#include "ListView.h"
#include "../settings/ChannelSettings.h"

class ChannelsScreen : public Screen {
public:
    ChannelsScreen() = default;
    ~ChannelsScreen() override = default;

    ScreenId getId() const override { return ScreenId::CHANNELS; }
    void onEnter() override;
    void onExit() override;
    void draw(bool fullRedraw) override;
    bool handleInput(const InputData& input) override;
    const char* getTitle() const override;
    void configureSoftKeys() override;

private:
    // Screen states
    enum State {
        STATE_LIST,           // Main channel list
        STATE_ADD_TYPE,       // Choose hashtag or custom PSK
        STATE_ADD_HASHTAG,    // Enter hashtag channel name
        STATE_ADD_PSK_NAME,   // Enter custom channel name
        STATE_ADD_PSK_KEY,    // Enter base64 PSK
        STATE_CONFIRM_DELETE  // Confirm channel deletion
    };

    State _state = STATE_LIST;
    int _selectedIndex = 0;
    int _addTypeSelection = 0;  // 0 = hashtag, 1 = custom PSK

    // Text input buffer
    char _inputBuffer[64];
    int _inputPos = 0;

    // For PSK entry (need to store name while entering key)
    char _pendingName[32];

    // For delete confirmation
    int _deleteIndex = -1;

    // List view for channel list
    ListView _listView;
    static const int MAX_CHANNEL_ITEMS = 8;
    ListItem _channelItems[MAX_CHANNEL_ITEMS];
    char _primaryStrings[MAX_CHANNEL_ITEMS][32];
    char _secondaryStrings[MAX_CHANNEL_ITEMS][32];

    // Build channel list from settings
    void buildChannelList();

    // Draw different states
    void drawList(bool fullRedraw);
    void drawAddTypeMenu(bool fullRedraw);
    void drawTextInput(const char* title, const char* prompt, const char* prefix, bool fullRedraw);
    void drawConfirmDelete(bool fullRedraw);

    // Handle input for different states
    bool handleListInput(const InputData& input);
    bool handleAddTypeInput(const InputData& input);
    bool handleTextInput(const InputData& input);
    bool handleConfirmInput(const InputData& input);

    // Actions
    void openSelectedChannel();
    void startAddChannel();
    void startDeleteChannel();
    void createHashtagChannel();
    void createPskChannel();
    void confirmDelete();
    void cancelAction();
    void clearInput();
};

#endif // MESHBERRY_CHANNELSSCREEN_H
