/**
 * MeshBerry Configuration
 *
 * Hardware pin definitions and constants for LILYGO T-Deck / T-Deck Plus
 * See: dev-docs/REQUIREMENTS.md for full hardware specifications
 */

#ifndef MESHBERRY_CONFIG_H
#define MESHBERRY_CONFIG_H

#include <Arduino.h>

// =============================================================================
// VERSION
// =============================================================================

#define MESHBERRY_VERSION_MAJOR 0
#define MESHBERRY_VERSION_MINOR 1
#define MESHBERRY_VERSION_PATCH 0
#define MESHBERRY_VERSION "0.1.0-dev"

// =============================================================================
// DISPLAY - ST7789 (2.8" 320x240 IPS LCD)
// =============================================================================

#define TFT_WIDTH       240
#define TFT_HEIGHT      320

// SPI pins (directly used by TFT_eSPI via build flags)
#define PIN_TFT_MOSI    41
#define PIN_TFT_SCLK    40
#define PIN_TFT_CS      12
#define PIN_TFT_DC      11
#define PIN_TFT_BL      42      // Backlight control

// Backlight settings
#define TFT_BL_ON       HIGH
#define TFT_BL_OFF      LOW
#define TFT_BL_PWM_CHANNEL  0
#define TFT_BL_PWM_FREQ     5000
#define TFT_BL_PWM_RES      8   // 8-bit resolution (0-255)

// =============================================================================
// KEYBOARD - ESP32-C3 via I2C
// =============================================================================

#define PIN_KB_SDA      18
#define PIN_KB_SCL      8
#define KB_I2C_ADDR     0x55    // Keyboard controller I2C address
#define KB_REG_READ     0x00    // Register to read key codes
#define KB_REG_BL       0x01    // Register for backlight control

// I2C settings
#define KB_I2C_FREQ     400000  // 400kHz

// =============================================================================
// TRACKBALL - GPIO inputs
// =============================================================================

#define PIN_TRACKBALL_UP        3
#define PIN_TRACKBALL_DOWN      15
#define PIN_TRACKBALL_LEFT      1
#define PIN_TRACKBALL_RIGHT     2
#define PIN_TRACKBALL_CLICK     0   // Also ESP32-S3 BOOT button (IO0)

// =============================================================================
// LORA RADIO - Semtech SX1262
// =============================================================================

#define PIN_LORA_MOSI   35
#define PIN_LORA_MISO   37
#define PIN_LORA_SCK    36
#define PIN_LORA_CS     39
#define PIN_LORA_RST    17
#define PIN_LORA_BUSY   13
#define PIN_LORA_DIO1   45      // Interrupt pin

// LoRa SPI settings
#define LORA_SPI_FREQ   8000000 // 8MHz

// Default LoRa settings (can be changed at runtime)
#define LORA_FREQUENCY_US   915.0   // MHz - US ISM band
#define LORA_FREQUENCY_EU   868.0   // MHz - EU ISM band
#define LORA_FREQUENCY_433  433.0   // MHz - 433 ISM band
#define LORA_BANDWIDTH      125.0   // kHz
#define LORA_SPREADING_FACTOR 12
#define LORA_CODING_RATE    8       // 4/8
#define LORA_TX_POWER       22      // dBm (max for SX1262)
#define LORA_PREAMBLE_LEN   8

// =============================================================================
// GPS - u-blox MIA-M10Q (T-Deck Plus only, runtime detected)
// =============================================================================

#define PIN_GPS_TX      43      // ESP32 TX -> GPS RX
#define PIN_GPS_RX      44      // ESP32 RX <- GPS TX
#define GPS_BAUD        9600

// GPS detection timeout
#define GPS_DETECT_TIMEOUT_MS   2000

// =============================================================================
// AUDIO OUTPUT - MAX98357A I2S Amplifier
// =============================================================================

#define PIN_I2S_BCLK    7
#define PIN_I2S_LRCLK   5
#define PIN_I2S_DOUT    6

// Audio settings
#define AUDIO_SAMPLE_RATE   16000
#define AUDIO_BITS          16

// =============================================================================
// AUDIO INPUT - ES7210 ADC (dual MEMS microphones)
// =============================================================================

#define PIN_MIC_I2S_BCLK    47
#define PIN_MIC_I2S_LRCLK   21
#define PIN_MIC_I2S_DIN     14

// ES7210 I2C configuration (shares bus with keyboard)
#define ES7210_I2C_ADDR     0x40

// =============================================================================
// BATTERY - ADC monitoring
// =============================================================================

#define PIN_BATTERY_ADC     4   // IO04

// Battery voltage calibration
// The ADC reads through a voltage divider
// Adjust these based on actual measurements
#define BATTERY_ADC_MULTIPLIER  2.0     // Voltage divider ratio
#define BATTERY_VOLTAGE_FULL    4.2     // 100% charge
#define BATTERY_VOLTAGE_EMPTY   3.0     // 0% charge

// =============================================================================
// SD CARD
// =============================================================================

#define PIN_SD_MOSI     41      // Shared with display
#define PIN_SD_MISO     38
#define PIN_SD_SCK      40      // Shared with display
#define PIN_SD_CS       39

// =============================================================================
// POWER MANAGEMENT
// =============================================================================

// Screen timeout (milliseconds)
#define SCREEN_TIMEOUT_MS       30000   // 30 seconds
#define SCREEN_DIM_TIMEOUT_MS   15000   // Dim at 15 seconds

// Sleep settings
#define DEEP_SLEEP_TIMEOUT_MS   300000  // 5 minutes of inactivity

// =============================================================================
// MESH NETWORK
// =============================================================================

// Maximum nodes to track
#define MAX_NODES           64
#define MAX_REPEATERS       32
#define MAX_CHANNELS        8

// Message settings
#define MAX_MESSAGE_LENGTH  200
#define MESSAGE_HISTORY     50

// =============================================================================
// UI SETTINGS
// =============================================================================

// Default language
#define DEFAULT_LANGUAGE    "en"

// Font sizes
#define FONT_SIZE_SMALL     1
#define FONT_SIZE_MEDIUM    2
#define FONT_SIZE_LARGE     4

// Colors (RGB565)
#define COLOR_BACKGROUND    0x0000  // Black
#define COLOR_TEXT          0xFFFF  // White
#define COLOR_ACCENT        0x07E0  // Green
#define COLOR_WARNING       0xFBE0  // Yellow
#define COLOR_ERROR         0xF800  // Red

#endif // MESHBERRY_CONFIG_H
