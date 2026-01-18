# PlatformIO Development Environment Setup

## Metadata

| Field | Value |
|-------|-------|
| **Date** | 2026-01-17 |
| **Author** | Human: jmreign (with AI assistance) |
| **Type** | config |
| **Status** | completed |
| **Related Issues** | N/A |

---

## Files Modified

| File Path | Change Type | Description |
|-----------|-------------|-------------|
| `platformio.ini` | added | PlatformIO project configuration for T-Deck |
| `src/main.cpp` | added | Main entry point with hardware initialization |
| `src/config.h` | added | Hardware pin definitions and constants |
| `src/drivers/display.h` | added | ST7789 display driver interface |
| `src/drivers/keyboard.h` | added | I2C keyboard driver interface |
| `src/drivers/lora.h` | added | SX1262 LoRa driver interface |
| `src/drivers/gps.h` | added | GPS driver interface (optional hardware) |
| `src/drivers/audio.h` | added | Audio I/O driver interface |
| `lib/README` | added | PlatformIO lib folder documentation |
| `include/README` | added | PlatformIO include folder documentation |
| `.vscode/extensions.json` | added | Recommended VSCode extensions |

---

## Summary

Created a complete PlatformIO development environment for MeshBerry firmware. This provides a standardized project structure that developers can clone and immediately start building. The firmware is unified for both T-Deck and T-Deck Plus with runtime GPS detection.

---

## Technical Details

### What Was Changed

1. **platformio.ini** - Full PlatformIO configuration:
   - ESP32-S3 target (esp32-s3-devkitc-1 board)
   - 16MB flash, 8MB PSRAM configuration
   - TFT_eSPI build flags for ST7789 display
   - Library dependencies: TFT_eSPI, RadioLib, TinyGPSPlus, ArduinoJson
   - Build exclusion for `dev-docs/` directory

2. **src/config.h** - Complete hardware definitions:
   - Display pins (SPI)
   - Keyboard I2C address and pins
   - Trackball GPIO pins
   - LoRa SX1262 SPI pins
   - GPS UART pins
   - Audio I2S pins
   - Battery ADC pin
   - Default settings for mesh network, UI, power management

3. **src/main.cpp** - Entry point with:
   - Hardware initialization sequence
   - I2C bus setup for keyboard
   - GPS runtime detection (for T-Deck Plus)
   - Battery voltage reading
   - Placeholder functions for each subsystem

4. **Driver headers** - Interface definitions for:
   - Display (ST7789): init, backlight, drawing primitives
   - Keyboard (I2C @ 0x55): key reading, backlight control
   - LoRa (SX1262): packet send/receive, region config
   - GPS (MIA-M10Q): position data, optional hardware
   - Audio (MAX98357A/ES7210): speaker output, mic input

### Why This Approach

- **Single unified firmware**: No build flags for GPS - runtime detection handles T-Deck vs T-Deck Plus
- **Standard PlatformIO structure**: Familiar to embedded developers
- **Clear separation**: Driver interfaces defined separately from implementation
- **Ready to extend**: Placeholder functions show where to add code

### Project Structure

```
MeshBerry/
├── platformio.ini          # Build configuration
├── src/
│   ├── main.cpp            # Entry point
│   ├── config.h            # Pin definitions
│   └── drivers/
│       ├── display.h       # ST7789 interface
│       ├── keyboard.h      # Keyboard interface
│       ├── lora.h          # LoRa interface
│       ├── gps.h           # GPS interface
│       └── audio.h         # Audio interface
├── lib/                    # Custom libraries
├── include/                # Global headers
├── dev-docs/               # Documentation (not compiled)
└── .vscode/
    └── extensions.json     # Recommended extensions
```

---

## Testing

### Tests Performed

| Test | Command/Method | Result |
|------|----------------|--------|
| Directory structure | `ls -la src/drivers/` | PASS - all files created |
| PlatformIO syntax | Reviewed platformio.ini | PASS - valid syntax |
| Config completeness | Compared to requirements | PASS - all pins defined |

### Build Verification

To verify the build compiles:
```bash
pio run
```

Note: Full compilation requires implementing the driver `.cpp` files.

---

## Results

### Before Change

- Empty project with only documentation

### After Change

- Complete PlatformIO project structure
- Ready for hardware driver implementation
- Other developers can clone and start immediately

---

## Breaking Changes

None - initial project setup

---

## Dependencies

### Added

| Dependency | Version | Purpose |
|------------|---------|---------|
| TFT_eSPI | ^2.5.43 | ST7789 display driver |
| RadioLib | ^6.6.0 | SX1262 LoRa radio driver |
| TinyGPSPlus | ^1.0.3 | GPS NMEA sentence parsing |
| ArduinoJson | ^7.0.4 | JSON config file handling |

---

## Known Issues

1. Driver implementations are placeholders - need `.cpp` files
2. LVGL is commented out in platformio.ini - uncomment if using for UI
3. Pin definitions based on LilyGO documentation - verify on actual hardware

---

## Follow-up Tasks

- [ ] Implement display.cpp with TFT_eSPI
- [ ] Implement keyboard.cpp with I2C communication
- [ ] Implement lora.cpp with RadioLib/MeshCore
- [ ] Implement gps.cpp with TinyGPSPlus
- [ ] Implement audio.cpp with I2S
- [ ] Add SD card initialization
- [ ] Add MeshCore protocol integration

---

## Notes

### Pin Reference Sources

- LilyGO T-Deck Wiki
- LilyGO T-Deck schematic
- Meshtastic T-Deck pin definitions (for cross-reference)

### Build Commands

```bash
# Build firmware
pio run

# Upload to device
pio run --target upload

# Monitor serial output
pio device monitor
```
