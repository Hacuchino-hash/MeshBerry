/**
 * MeshBerry - Open Source Firmware for LILYGO T-Deck
 *
 * Main entry point
 *
 * See: dev-docs/REQUIREMENTS.md for full specifications
 * See: src/config.h for pin definitions
 */

#include <Arduino.h>
#include <Wire.h>
#include <SPI.h>
#include "config.h"

// Driver includes (to be implemented)
#include "drivers/display.h"
#include "drivers/keyboard.h"
#include "drivers/lora.h"
#include "drivers/gps.h"
#include "drivers/audio.h"

// =============================================================================
// GLOBALS
// =============================================================================

bool gpsPresent = false;    // Runtime GPS detection

// =============================================================================
// FORWARD DECLARATIONS
// =============================================================================

void initHardware();
void initDisplay();
void initKeyboard();
void initTrackball();
void initLoRa();
void initGPS();
void initAudio();
void initSDCard();
bool detectGPS();
float readBatteryVoltage();
uint8_t getBatteryPercent();

// =============================================================================
// SETUP
// =============================================================================

void setup() {
    // Initialize serial for debugging
    Serial.begin(115200);
    delay(1000);

    Serial.println();
    Serial.println("====================================");
    Serial.println("  MeshBerry v" MESHBERRY_VERSION);
    Serial.println("  Open Source T-Deck Firmware");
    Serial.println("====================================");
    Serial.println();

    // Initialize all hardware
    initHardware();

    Serial.println();
    Serial.println("[BOOT] MeshBerry ready!");
    Serial.println();
}

// =============================================================================
// MAIN LOOP
// =============================================================================

void loop() {
    // TODO: Implement main loop
    // - Handle keyboard/trackball input
    // - Process mesh network events
    // - Update UI
    // - Handle notifications
    // - Power management

    delay(10);  // Placeholder - will be replaced with proper task scheduling
}

// =============================================================================
// HARDWARE INITIALIZATION
// =============================================================================

void initHardware() {
    Serial.println("[INIT] Starting hardware initialization...");

    // Initialize I2C for keyboard and other peripherals
    Wire.begin(PIN_KB_SDA, PIN_KB_SCL, KB_I2C_FREQ);
    Serial.println("[INIT] I2C bus initialized");

    // Initialize each subsystem
    initDisplay();
    initKeyboard();
    initTrackball();
    initLoRa();
    initGPS();
    initAudio();
    initSDCard();

    // Read initial battery status
    float voltage = readBatteryVoltage();
    uint8_t percent = getBatteryPercent();
    Serial.printf("[INIT] Battery: %.2fV (%d%%)\n", voltage, percent);
}

void initDisplay() {
    Serial.println("[INIT] Display (ST7789)...");

    // Enable backlight
    pinMode(PIN_TFT_BL, OUTPUT);
    digitalWrite(PIN_TFT_BL, TFT_BL_ON);

    // TODO: Initialize TFT_eSPI
    // tft.init();
    // tft.setRotation(1);
    // tft.fillScreen(COLOR_BACKGROUND);

    Serial.println("[INIT] Display: OK");
}

void initKeyboard() {
    Serial.println("[INIT] Keyboard (I2C @ 0x55)...");

    // TODO: Initialize keyboard driver
    // Check if keyboard controller responds
    Wire.beginTransmission(KB_I2C_ADDR);
    if (Wire.endTransmission() == 0) {
        Serial.println("[INIT] Keyboard: OK");
    } else {
        Serial.println("[INIT] Keyboard: NOT FOUND");
    }
}

void initTrackball() {
    Serial.println("[INIT] Trackball...");

    pinMode(PIN_TRACKBALL_UP, INPUT_PULLUP);
    pinMode(PIN_TRACKBALL_DOWN, INPUT_PULLUP);
    pinMode(PIN_TRACKBALL_LEFT, INPUT_PULLUP);
    pinMode(PIN_TRACKBALL_RIGHT, INPUT_PULLUP);
    pinMode(PIN_TRACKBALL_CLICK, INPUT_PULLUP);

    Serial.println("[INIT] Trackball: OK");
}

void initLoRa() {
    Serial.println("[INIT] LoRa (SX1262)...");

    // TODO: Initialize RadioLib SX1262
    // SPI.begin(PIN_LORA_SCK, PIN_LORA_MISO, PIN_LORA_MOSI, PIN_LORA_CS);
    // radio.begin(LORA_FREQUENCY_US);

    Serial.println("[INIT] LoRa: PLACEHOLDER");
}

void initGPS() {
    Serial.println("[INIT] GPS (u-blox MIA-M10Q)...");

    // Try to detect GPS module (T-Deck Plus only)
    gpsPresent = detectGPS();

    if (gpsPresent) {
        Serial.println("[INIT] GPS: OK (T-Deck Plus detected)");
    } else {
        Serial.println("[INIT] GPS: Not present (T-Deck base model)");
    }
}

bool detectGPS() {
    // Initialize GPS UART
    Serial1.begin(GPS_BAUD, SERIAL_8N1, PIN_GPS_RX, PIN_GPS_TX);

    // Wait for GPS data with timeout
    unsigned long start = millis();
    while (millis() - start < GPS_DETECT_TIMEOUT_MS) {
        if (Serial1.available()) {
            char c = Serial1.read();
            // Check for NMEA sentence start
            if (c == '$') {
                return true;
            }
        }
    }

    // No GPS data received
    Serial1.end();
    return false;
}

void initAudio() {
    Serial.println("[INIT] Audio (MAX98357A)...");

    // TODO: Initialize I2S for speaker output
    // i2s_config_t i2s_config = {...};
    // i2s_driver_install(I2S_NUM_0, &i2s_config, 0, NULL);

    Serial.println("[INIT] Audio: PLACEHOLDER");
}

void initSDCard() {
    Serial.println("[INIT] SD Card...");

    // TODO: Initialize SD card
    // if (SD.begin(PIN_SD_CS)) {
    //     Serial.println("[INIT] SD Card: OK");
    // } else {
    //     Serial.println("[INIT] SD Card: Not inserted");
    // }

    Serial.println("[INIT] SD Card: PLACEHOLDER");
}

// =============================================================================
// BATTERY MONITORING
// =============================================================================

float readBatteryVoltage() {
    // Read ADC value
    int rawValue = analogRead(PIN_BATTERY_ADC);

    // Convert to voltage (ESP32-S3 ADC is 12-bit, 0-3.3V reference)
    float voltage = (rawValue / 4095.0) * 3.3 * BATTERY_ADC_MULTIPLIER;

    return voltage;
}

uint8_t getBatteryPercent() {
    float voltage = readBatteryVoltage();

    // Clamp voltage to valid range
    if (voltage >= BATTERY_VOLTAGE_FULL) return 100;
    if (voltage <= BATTERY_VOLTAGE_EMPTY) return 0;

    // Linear interpolation (simple approximation)
    float percent = (voltage - BATTERY_VOLTAGE_EMPTY) /
                    (BATTERY_VOLTAGE_FULL - BATTERY_VOLTAGE_EMPTY) * 100.0;

    return (uint8_t)percent;
}
