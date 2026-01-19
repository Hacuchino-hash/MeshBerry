# MeshBerry Project Status

**Version:** 0.1.0
**License:** GPL-3.0-or-later
**Copyright:** (C) 2026 NodakMesh (nodakmesh.org)

## Overview

MeshBerry is open-source firmware for the LILYGO T-Deck, providing MeshCore-compatible mesh networking capabilities. The firmware enables standalone mesh communication without requiring a phone or companion app.

## Current Status: Alpha (Development)

### Working Features

#### Hardware Support
- **Display:** ST7789 320x240 TFT with SPI interface - fully functional
- **Keyboard:** Integrated keyboard via I2C - working with input test mode
- **Trackball:** 5-way navigation (up/down/left/right/click) - working
- **LoRa Radio:** SX1262 with MeshCore-compatible settings - working
- **Battery:** ADC-based monitoring with percentage display - working
- **GPS:** Auto-detection for T-Deck Plus variant

#### Mesh Networking (MeshCore Integration)
- **Advertisements:** Sending and receiving node advertisements
- **Node Discovery:** Automatic tracking of discovered nodes with RSSI/SNR
- **RTC Clock:** Synchronized from received advertisements for valid timestamps
- **Identity:** Ed25519 key pair generation and persistence via SPIFFS
- **Packet Forwarding:** Optional repeater mode

#### Repeater Management (Recently Fixed)
- **Repeater Login:** ECDH-based authentication with password - **WORKING**
- **Login Response Handling:** PATH packet with embedded RESPONSE - **WORKING**
- **CLI Commands:** Send commands to connected repeater
- **Session Management:** Track connected repeater state and permissions
- **Contact Storage:** Save repeater pubKeys and metadata

#### UI Screens
- **Input Test:** Keyboard and trackball testing
- **Status:** Node info, battery, radio settings, mesh stats
- **Settings:** Radio parameter configuration (frequency, SF, BW, CR, TX power)
- **Channels:** Channel management with PSK support
- **Messages:** Basic messaging UI
- **Contacts:** Node/repeater list with admin login

#### Serial CLI
- `help` - Command reference
- `status` - Node and radio status
- `nodes` / `repeaters` - List discovered nodes
- `advert` - Send advertisement
- `name <name>` - Set node name
- `forward on|off` - Toggle packet forwarding
- `login <name> <password>` - Login to repeater
- `logout` - Disconnect from repeater
- `cmd <command>` - Send CLI command to repeater
- `save/forget/saved` - Credential management

### Recent Bug Fixes

1. **Invalid Timestamp (Jan 2025)**
   - Problem: Login packets had timestamp ~22 seconds instead of Unix epoch
   - Cause: ESP32RTCClock started with `_epoch_offset = 0`
   - Fix: Default epoch set to Jan 1, 2025 + sync from advertisement timestamps

2. **Login Response Not Processed (Jan 2025)**
   - Problem: Login succeeded on repeater but timed out on client
   - Cause: Response comes as PATH packet with embedded RESPONSE, not direct RESPONSE
   - Fix: Added `onPeerPathRecv()` override to extract login response from PATH packets

3. **Base64 Decode Bug**
   - Problem: PubKey decode failed silently
   - Cause: Padding not accounted for in length calculation
   - Fix: Proper padding handling in `decodePSK()`

### Architecture

```
src/
├── main.cpp              # Entry point, hardware init, main loop
├── config.h              # Pin definitions, constants
├── board/
│   └── TDeckBoard.*      # Board abstraction layer
├── drivers/
│   ├── display.*         # ST7789 TFT driver
│   ├── keyboard.*        # I2C keyboard driver
│   ├── lora.*            # SX1262 LoRa driver
│   ├── gps.h             # GPS interface (T-Deck Plus)
│   └── audio.h           # Audio interface (future)
├── mesh/
│   ├── MeshBerryMesh.*   # MeshCore mesh application
│   └── MeshBerrySX1262Wrapper.h  # RadioLib 7.x compatibility
├── crypto/
│   └── ChannelCrypto.*   # PSK encoding/decoding
├── settings/
│   ├── SettingsManager.* # SPIFFS persistence
│   ├── RadioSettings.h   # LoRa radio configuration
│   ├── ChannelSettings.* # Channel/PSK management
│   └── ContactSettings.* # Node/repeater contacts
└── ui/
    ├── ChannelUI.*       # Channel management screen
    ├── MessagingUI.*     # Message display/compose
    └── ContactUI.*       # Contact/repeater management
```

### Dependencies

- **MeshCore** - Mesh networking library
- **RadioLib** - SX1262 radio driver (v7.x)
- **TFT_eSPI** - Display driver
- **ArduinoJson** - Settings persistence
- **Crypto** - Ed25519/AES/SHA256

### Known Limitations

- Channel messaging not fully tested
- GPS integration incomplete
- No audio/speaker support yet
- UI needs polish and better navigation

### Next Steps

1. Test CLI commands to repeater post-login
2. Implement channel message send/receive
3. Add more repeater admin features
4. Improve UI flow and visual design
5. GPS integration for location sharing
6. Clean up debug output for release builds

## Building

```bash
# Using PlatformIO
pio run -e tdeck

# Upload to device
pio run -e tdeck -t upload

# Monitor serial output
pio device monitor -b 115200
```

## Hardware

- **Target:** LILYGO T-Deck (ESP32-S3)
- **Display:** ST7789 320x240
- **Radio:** SX1262 LoRa
- **Keyboard:** Integrated QWERTY
- **Optional:** GPS module (T-Deck Plus variant)
