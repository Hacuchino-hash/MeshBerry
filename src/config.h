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
// Pin definitions verified from MeshCore lilygo_tdeck variant
// =============================================================================

#define PIN_LORA_MOSI   41      // LoRa MOSI (shared SPI bus)
#define PIN_LORA_MISO   38      // LoRa MISO
#define PIN_LORA_SCK    40      // LoRa SCLK (shared SPI bus)
#define PIN_LORA_CS     9       // LoRa NSS/CS
#define PIN_LORA_RST    17      // LoRa RESET
#define PIN_LORA_BUSY   13      // LoRa BUSY
#define PIN_LORA_DIO1   45      // LoRa DIO1 interrupt pin

// LoRa SPI settings
#define LORA_SPI_FREQ   8000000 // 8MHz

// =============================================================================
// REGIONAL FREQUENCY CONFIGURATION
// Selected at build time via platformio.ini (tdeck_us, tdeck_eu, tdeck_au)
// =============================================================================

// Default frequency if not specified by build
#ifndef LORA_FREQ
  #define LORA_FREQ  915.0      // Default to US band
#endif

// Region display name for UI
#if defined(LORA_REGION_EU)
  #define LORA_REGION_NAME "EU"
  #define LORA_REGION_BAND "868 MHz"
#elif defined(LORA_REGION_AU)
  #define LORA_REGION_NAME "AU"
  #define LORA_REGION_BAND "915 MHz"
#else
  // Default: US/FCC
  #define LORA_REGION_NAME "US"
  #define LORA_REGION_BAND "915 MHz"
#endif

// =============================================================================
// SX1262 HARDWARE SETTINGS (T-Deck specific, from MeshCore)
// =============================================================================

#ifndef SX126X_DIO2_AS_RF_SWITCH
  #define SX126X_DIO2_AS_RF_SWITCH    false
#endif

#ifndef SX126X_DIO3_TCXO_VOLTAGE
  #define SX126X_DIO3_TCXO_VOLTAGE    1.8f
#endif

#ifndef SX126X_CURRENT_LIMIT
  #define SX126X_CURRENT_LIMIT        140     // mA
#endif

#ifndef SX126X_RX_BOOSTED_GAIN
  #define SX126X_RX_BOOSTED_GAIN      1
#endif

// =============================================================================
// LORA PARAMETERS (MeshCore compatible defaults)
// =============================================================================

#ifndef LORA_BW
  #define LORA_BW             250     // kHz bandwidth
#endif

#ifndef LORA_SF
  #define LORA_SF             10      // Spreading factor
#endif

#ifndef LORA_CR
  #define LORA_CR             5       // Coding rate (4/5)
#endif

#ifndef LORA_TX_POWER
  #define LORA_TX_POWER       22      // dBm (max for SX1262)
#endif

#define LORA_PREAMBLE_LEN     16      // symbols

// =============================================================================
// GPS - u-blox MIA-M10Q (T-Deck Plus only, runtime detected)
// =============================================================================

#define PIN_GPS_TX      43      // ESP32 TX -> GPS RX
#define PIN_GPS_RX      44      // ESP32 RX <- GPS TX

// GPS baud rates - T-Deck Plus has two GPS variants:
// - Quectel L76K uses 9600 baud
// - u-blox M10Q uses 38400 baud
#define GPS_BAUD_L76K   9600    // Quectel L76K
#define GPS_BAUD_M10Q   38400   // u-blox MIA-M10Q

// GPS detection timeout per baud rate attempt
#define GPS_DETECT_TIMEOUT_MS   2500

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
// PERIPHERAL POWER CONTROL
// =============================================================================

#define PIN_PERIPHERAL_POWER    10  // Controls power to peripherals (LoRa, etc.)

// =============================================================================
// SD CARD
// =============================================================================

#define PIN_SD_MOSI     41      // Shared with display/LoRa SPI bus
#define PIN_SD_MISO     38      // Shared with LoRa
#define PIN_SD_SCK      40      // Shared with display/LoRa SPI bus
#define PIN_SD_CS       39      // SD card chip select

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
