# MeshBerry Firmware Requirements Specification

> **Target Device:** LILYGO T-Deck (with mini keyboard and trackball)
> **License:** MIT
> **Version:** 1.0.0-draft

---

## Table of Contents

1. [Project Overview](#project-overview)
2. [Goals and Key Features](#goals-and-key-features)
3. [Hardware Platform Details](#hardware-platform-details)
4. [Development Environment](#development-environment)
5. [Sources](#sources)

---

## Project Overview

MeshBerry is a new open-source firmware (MIT-licensed) for the LILYGO T-Deck series of handheld LoRa communicators. It is designed to transform the T-Deck into an off-grid messaging device with a modern, smartphone-like user experience – but without any paywalls or proprietary restrictions.

MeshBerry will leverage the **MeshCore mesh networking protocol** to enable long-range, decentralized communication between devices, similar in spirit to Meshtastic or MeshOS, but fully open and free.

The firmware will incorporate all the key features of the existing MeshOS T-Deck off-grid messenger (fluid chat UI, mapping, repeater tools, etc.) without requiring any paid license, and will emphasize intuitive design, multilingual support, and efficient battery usage.

---

## Goals and Key Features

### Open Source & Licensing

- Released under the **MIT License**
- All code, schematics, and documentation will be open for the community
- **No feature paywalls** – every capability available to all users without fees
- Development will be collaborative, welcoming contributions for improvements and translations

### Hardware Target – T-Deck Series

| Model | Description |
|-------|-------------|
| **T-Deck** | Base model - pocket-sized, BlackBerry-style device with ESP32-S3 SoC, 2.8-inch color display, mini QWERTY keyboard, and LoRa radio |
| **T-Deck Plus** | Adds GPS (u-blox MIA-M10Q) and larger battery (2000 mAh) |
| **T-Deck Pro** | Future variant support can be added later |

### Mesh Networking (MeshCore)

- Implements the **MeshCore protocol stack** for LoRa (Semtech SX1262) radios
- Enables decentralized mesh communication
- Capabilities:
  - Send text messages
  - Share location
  - Relay messages for others (multi-hop routing)
- **LoRa-dependent** operation on sub-GHz ISM bands:
  - 433 MHz
  - 868 MHz
  - 915 MHz
- Nodes can operate as:
  - Regular end-user communicators
  - Dedicated repeaters to strengthen the network
- Optional configuration via Bluetooth or Wi-Fi

---

### Intuitive User Interface

#### Chat-Centric Design

- Conversation threads rendered as **speech bubbles**
- Color-coded usernames
- Delivery/read status indicators
- Support for multiple channels and direct messages
- Tabbed interface for easy switching between group chats and DMs

#### Navigation Methods

| Input Method | Description |
|--------------|-------------|
| **Trackball** | Scrolling and menu navigation, click to select |
| **Touchscreen** | Swipe to scroll, tap to select (if hardware supports) |
| **Keyboard** | Quick shortcuts for common actions |

> **Note:** Either input method can be used exclusively – user can operate solely via keyboard/trackball or solely via touch.

#### Keyboard Shortcuts

- Toggle backlight via key combos
- Adjust brightness
- Modifier keys (Alt/Ctrl) enable extra functions
- Similar to Meshtastic's `Alt+key` shortcuts

#### Multilingual UI

**Supported Languages:**
- English (primary)
- Spanish
- French
- Dutch
- Others used by MeshCore community

**Implementation:**
- Externalize all UI strings for localization
- Font/rendering supports accented characters and common symbols

---

### Mesh Messaging Features

#### Easy Device Setup

On first boot, a guided setup (3 steps) allows user to set:
1. Region (LoRa band)
2. Device name
3. Encryption keys

#### Rich Chat Experience

| Feature | Description |
|---------|-------------|
| **Threaded chats** | Conversations with timestamps and user nicknames |
| **Delivery receipts** | Checkmark or color change when message is delivered/acknowledged |
| **Message actions** | Select via trackball click or long-press for "Reply" (quote reply) or copy |
| **URL QR codes** | Generate QR code on-screen for URLs so user can scan with phone |

#### Emoji Support

- At least **30 common emojis** built-in and renderable
- Lightweight emoji font or bitmap images
- Extensible set for future additions

#### Multiple Channels & DMs

- Join multiple channels (public or encrypted)
- One-to-one chats
- Tabs or menu to switch between conversations
- Example: "Public" channel + "Team" channel running concurrently

#### Notifications and Alerts

| Notification Type | Description |
|-------------------|-------------|
| **On-screen** | Popup or icon indicator, unread count on chat tab |
| **Visual** | Screen and keyboard backlight briefly turn on (configurable) |
| **Audio** | Notification tone plays, multiple sounds available |
| **Custom sounds** | Load WAV files from SD card for personalized tones |
| **Quiet mode** | Disable sound/light for stealth or nighttime use |

#### Power Management Features

| Feature | Description |
|---------|-------------|
| **Auto-dim** | Screen dims or turns off after inactivity timeout |
| **Always-on lock screen** | Minimal display showing time, battery, mesh signal icon |
| **Power save mode** | Low-power operation while still receiving messages |

#### Message Status Indicators

| Status | Visual |
|--------|--------|
| **Pending** | Outlined in yellow (hopping through mesh) |
| **Delivered** | Green (recipient acknowledged) |
| **Failed** | Red (delivery failed) |

#### Last-Heard List

Displays recently received nodes:
- Node name/ID
- Signal strength of last packet
- Approximate distance (if location known)
- Nearest city or area (if location shared)

#### Mesh Signal Meter

- 0-4 bars indicator (like cellphone signal)
- Based on neighbor nodes/repeaters heard recently
- Shows ID of last mesh signal source
- At-a-glance confidence of connectivity

---

### Geolocation and Mapping

#### GPS Support (T-Deck Plus)

- Utilizes **u-blox MIA-M10Q** GPS module
- Obtains device location (latitude/longitude)
- Location sharing over mesh (user-configurable)

#### Offline Maps

| Feature | Description |
|---------|-------------|
| **Built-in world map** | Dark theme with city name labels, no external files needed |
| **Multiple zoom levels** | World view to regional view |
| **Position display** | Own location (GPS) or fixed home location |
| **Node plotting** | All known node locations and repeaters with different icons |

#### Map Controls

| Control | Trackball | Touch |
|---------|-----------|-------|
| **Pan** | Move cursor | Swipe |
| **Zoom** | Arrow controls | Pinch or +/- buttons |
| **Center** | GPS centering button | GPS centering button |
| **Home** | Set/view manual location | Set/view manual location |

---

### Repeater Administration

#### Graphical Interface

| Feature | Description |
|---------|-------------|
| **Repeater List** | Dedicated screen showing ID, alias, last heard signal |
| **Scanner** | Listen for repeater beacons, use MeshCore discovery |
| **Whitelist/Favorites** | Add repeaters to prevent cluttering main contact list |

#### Repeater Management UI

For each repeater (authorized users):
- Get Stats
- List Neighbors
- Sync Time
- Send Beacon

#### MeshCore Terminal (CLI)

Full-screen text terminal providing direct command-line access:

| Capability | Description |
|------------|-------------|
| **REPL/Console** | Direct MeshCore network stack access |
| **Remote login** | Log into repeater via mesh with credentials |
| **Live events** | Color-coded display of mesh events and packets |
| **USB mirroring** | Serial console mirrors terminal for PC access |

---

### Firmware Update Mechanism

| Method | Description |
|--------|-------------|
| **USB flashing** | Traditional method via USB connection |
| **SD card update** | Download binary to microSD, device self-flashes |

---

## Hardware Platform Details

### Microcontroller (ESP32-S3)

| Specification | Value |
|---------------|-------|
| **Model** | ESP32-S3FN16R8 |
| **Cores** | Dual-core Tensilica LX7 |
| **Clock** | 240 MHz |
| **SRAM** | 512KB |
| **PSRAM** | 8MB |
| **Flash** | 16MB |
| **Connectivity** | Wi-Fi 4, Bluetooth 5 LE |

### Co-Processor (ESP32-C3 for Keyboard)

| Specification | Value |
|---------------|-------|
| **Purpose** | Keyboard and trackball interface |
| **Communication** | I2C slave at address `0x55` |
| **Key read register** | `0x00` (returns next key code) |
| **Backlight control** | Write `0x01` with data |

**Driver Requirements:**
- Initialize I2C as master on ESP32-S3
- Poll or interrupt on keypress availability
- Map key codes to characters
- Implement modifier key handling (Alt, Sym)

### Display

| Specification | Value |
|---------------|-------|
| **Size** | 2.8-inch IPS LCD |
| **Resolution** | 320×240 (QVGA) |
| **Controller** | ST7789 over SPI |
| **Color depth** | 16-bit |
| **Touch** | Not included on stock T-Deck |

**Implementation Notes:**
- Use TFT_eSPI or ESP-IDF LCD drivers
- Design UI for readability (8-10pt fonts)
- Backlight control via GPIO
- Consider LVGL for rapid UI development

### Trackball Navigation Module

| Feature | Description |
|---------|-------------|
| **Type** | 5-way input (up/down/left/right + click) |
| **Center press** | Doubles as BOOT button (ESP32-S3 IO0) |
| **Interface** | GPIO inputs for directions, IO0 for click |

**Implementation Notes:**
- Read directional GPIOs for navigation
- Translate movement to UI scroll/navigation
- Handle click as select/enter action

### Mini Keyboard (T-Keyboard)

| Specification | Value |
|---------------|-------|
| **Keys** | ~39 keys QWERTY |
| **Interface** | Via ESP32-C3 over I2C |
| **Backlight** | Controllable via I2C command |

**Features to Implement:**
- Keymap for raw codes to characters
- Text input for message composition
- Menu shortcuts
- Backlight toggle (similar to Meshtastic `Alt+B`)

### LoRa Radio

| Specification | Value |
|---------------|-------|
| **Chip** | Semtech SX1262 |
| **Interface** | SPI |
| **Control pins** | NRST, BUSY, DIO1 |
| **Max Tx power** | +22 dBm |

**Frequency Regions:**
| Region | Frequency |
|--------|-----------|
| US | 915 MHz |
| EU | 868 MHz |
| Other | 433 MHz |

**Implementation Notes:**
- Use MeshCore library for radio abstraction
- Configure region presets
- Use DIO1 interrupt for RX done
- Implement light sleep with radio wake

### Bluetooth & Wi-Fi

| Feature | Purpose |
|---------|---------|
| **BLE** | Smartphone app pairing, device configuration |
| **Wi-Fi** | Web-based config portal, potential gateway functionality |
| **USB Serial** | MeshCore Terminal mirror, advanced configuration |

### GPS (T-Deck Plus Only)

| Specification | Value |
|---------------|-------|
| **Module** | u-blox MIA-M10Q GNSS |
| **Interface** | UART (likely 9600 baud) or I2C |
| **Data format** | NMEA sentences (GGA, RMC) |

**Features:**
- Display current coordinates
- Timestamp messages
- Location tags in messages (optional)
- On/off setting for power/privacy
- Toggle shortcut (like Meshtastic `Alt+C+G`)

### MicroSD Storage

| Usage | Description |
|-------|-------------|
| **Map tiles** | Offline map data storage |
| **Log files** | Message and debug logs |
| **Firmware updates** | `meshberry_firmware.bin` for self-update |
| **Custom media** | Notification sounds (WAV), icons |

**Implementation Notes:**
- FAT32 filesystem
- SDMMC or SPI SD driver
- Card must be inserted before boot
- Menu option to check SD status

### Audio Output (Speaker)

| Specification | Value |
|---------------|-------|
| **Amplifier** | MAX98357A Class-D |
| **Interface** | I2S from ESP32-S3 |
| **Format** | 16-bit mono PCM (e.g., 16 kHz) |

**Features:**
- Notification tones
- Keypress sounds (optional)
- Volume control via digital signal scaling

### Audio Input (Microphones)

| Specification | Value |
|---------------|-------|
| **Type** | Two MEMS microphones |
| **Codec** | ES7210 audio ADC |
| **Interface** | I2S for audio, I2C for configuration |

**Implementation Notes:**
- Initialize ES7210 in low-power standby by default
- Future: voice notes feature (record, compress, send)
- Configure gain and sample rate via I2C

### Battery and Power Management

| Specification | T-Deck | T-Deck Plus |
|---------------|--------|-------------|
| **Battery** | External via JST | Built-in 2000 mAh Li-Po |
| **Voltage monitoring** | ADC on IO04 | ADC on IO04 |
| **Charging** | USB-C (TP4056 or similar) | USB-C |

#### Battery Measurement

| Voltage | Percentage |
|---------|------------|
| 4.2V | 100% (fully charged) |
| 3.7V | ~50% |
| 3.0V | 0% (empty) |

#### Power Saving Strategies

| Strategy | Description |
|----------|-------------|
| **Screen timeout** | Backlight off after inactivity |
| **CPU scaling** | Lower frequency when idle |
| **Light sleep** | Wake on LoRa interrupt |
| **Radio management** | Disable Wi-Fi/BT when not in use |
| **GPS duty cycle** | On-demand fixes, power-save modes |

### Pin Reference

| Component | Interface | Notes |
|-----------|-----------|-------|
| Display (ST7789) | SPI | Check LilyGO pin diagram |
| Keyboard (ESP32-C3) | I2C @ 0x55 | Master on S3 |
| Trackball | GPIO | Directions + IO0 click |
| LoRa (SX1262) | SPI | NRST, BUSY, DIO1 control pins |
| GPS (MIA-M10Q) | UART | T-Deck Plus only |
| SD Card | SDMMC/SPI | Check for shared SPI lines |
| Speaker (MAX98357A) | I2S | Mono output |
| Microphones (ES7210) | I2S + I2C | Dual MEMS |
| Battery ADC | IO04 | Resistor divider |

---

## Development Environment

### Toolchain

| Tool | Choice |
|------|--------|
| **Framework** | PlatformIO with Arduino |
| **Language** | C/C++ |
| **IDE** | VSCode + PlatformIO extension |

### Architecture

#### Dual-Core Task Distribution

| Core | Responsibilities |
|------|------------------|
| **Core 0** | MeshCore networking, LoRa handling, mesh routing, BT/WiFi stack |
| **Core 1** | UI rendering, input handling, display updates |

#### RTOS Configuration

- FreeRTOS task management
- Radio interrupt task: **high priority**
- UI task: **medium priority**
- Background tasks: **low priority**

### External Libraries

| Library | Purpose | License |
|---------|---------|---------|
| **MeshCore** | Mesh networking protocol | MIT |
| **TFT_eSPI** or **LovyanGFX** | Display driver | MIT |
| **LVGL** | GUI framework (optional) | MIT |
| **RadioLib** | SX1262 driver (if needed) | MIT |
| **TinyGPS++** | NMEA parsing | LGPL |

### MeshCore Integration

| Event | Action |
|-------|--------|
| User sends message | Call MeshCore API to transmit |
| Packet received | Process: display message, update map, update node list |
| Network event | Update repeater/node lists |

### Localization

| Aspect | Implementation |
|--------|----------------|
| **Format** | JSON or CSV key-value pairs |
| **Encoding** | UTF-8 throughout |
| **Fonts** | Noto Sans subset, Noto Emoji subset |
| **Switching** | Settings menu, triggers UI text reload |

---

## Development Phases

### Phase 1: Research & Prototyping

- [ ] Verify keyboard I2C communication
- [ ] Test LCD drawing with ST7789
- [ ] LoRa packet send/receive between devices
- [ ] MeshCore library integration test

### Phase 2: Core Framework

- [ ] Set up PlatformIO project structure
- [ ] Implement dual-task event loop
- [ ] Basic MeshCore send/receive in console

### Phase 3: UI Development

- [ ] Design screen layouts (Home, Chat, Nodes, Maps, Settings)
- [ ] Implement menu navigation with trackball
- [ ] Choose and integrate GUI framework

### Phase 4: Feature Implementation

- [ ] Messaging UI with send/receive
- [ ] Notification system and backlight control
- [ ] Map view with sample data
- [ ] Repeater management screens
- [ ] Terminal mode with MeshCore hook
- [ ] Settings screens

### Phase 5: Testing & Optimization

- [ ] Hardware testing on actual devices
- [ ] Power consumption measurement
- [ ] Performance optimization
- [ ] Usability testing

### Phase 6: Localization

- [ ] Prepare string externalization
- [ ] Community translation contributions
- [ ] Font and rendering verification

### Phase 7: Documentation & Release

- [ ] Build and flash documentation
- [ ] User manual
- [ ] Pre-built firmware binaries
- [ ] GitHub release

---

## Sources

| Source | Description |
|--------|-------------|
| LILYGO T-Deck Wiki | Hardware specs and documentation |
| MeshOS | MeshCore T-Deck firmware feature reference |
| Meshtastic | T-Deck input methods, shortcuts reference |
| LILYGO T-Deck Plus Specs | GPS and battery specifications |
| T-Deck Keyboard Firmware | I2C protocol documentation |
| MeshCore GitHub | meshcore-dev/MeshCore project |

---

## Document History

| Version | Date | Author | Changes |
|---------|------|--------|---------|
| 1.0.0-draft | 2026-01-17 | Initial | Initial requirements specification |
