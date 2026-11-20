/* ____________________________
   This software is licensed under the MIT License:
   https://github.com/cifertech/rfclown
   ________________________________________ */

#ifndef CONFIG_H
#define CONFIG_H

#define SCREEN_W 128
#define SCREEN_H 64

// ---------------- Pins ----------------
#define PIN_BTN_L  27
#define PIN_BTN_R  25
#define PIN_BTN_S  26

// nRF24-specific Pins
#define NRF_CE_PIN_A    5   
#define NRF_CSN_PIN_A   17 
#define NRF_CE_PIN_B    16  
#define NRF_CSN_PIN_B   4  
#define NRF_CE_PIN_C    15  
#define NRF_CSN_PIN_C   2  

// Common dependencies
#include "setting.h"
#include <Arduino.h>
#include <U8g2lib.h>
#include <Wire.h>
#include <SPI.h>
#include <Adafruit_NeoPixel.h>
#include <EEPROM.h>
#include <Preferences.h>
#include <vector>
#include <string>
#include <math.h>

// FIX (2026-11-19): the screen (u8g2) and NeoPixel (pixels) objects used to be
// *created* here in the header — giving every source file its own copy and
// breaking the link. Their "extern" announcements already live in setting.h
// (included above), and the single real copies now live in setting.cpp.

// BLE-specific dependencies
#include <BLEDevice.h>
#include <BLEUtils.h>
#include <BLEServer.h>

// nRF24-specific dependencies
#include <nRF24L01.h>
#include <RF24.h>

// WiFi-specific dependencies
#include <WiFi.h>
#include <esp_wifi.h>
#include <esp_wifi_types.h>
#include <esp_system.h>
#include <esp_event.h>
#include <nvs_flash.h>
#include <string>

// ESP-specific configurations
#include <esp_bt.h>
#include <esp_wifi.h>

// External declarations
extern U8G2_SSD1306_128X64_NONAME_F_HW_I2C u8g2;
extern Adafruit_NeoPixel pixels;

static const uint8_t* FONT_SMALL  = u8g2_font_5x8_tf;
static const uint8_t* FONT_MEDIUM = u8g2_font_6x12_tf;
static const uint8_t* FONT_ICON_FALLBACK = u8g2_font_open_iconic_thing_2x_t;

// SCREENSAVER_MODULE and LED_MODULE (added 2026-11-19) are "settings" cards,
// not jamming modes. When focus is on either, the radios keep running
// whatever the LAST real jamming mode was (tracked in lastJamMode).
enum OperationMode {WiFi_MODULE, VIDEO_TX_MODULE, RC_MODULE, BLE_MODULE, Bluetooth_MODULE, USB_WIRELESS_MODULE, ZIGBEE_MODULE, NRF24_MODULE, SCREENSAVER_MODULE, LED_MODULE};
// FIX (2026-11-19): shared state variables changed from "defined here" to
// "announced here" (extern). The single real copies live in setting.cpp.
extern OperationMode current_Mode;

enum Operation {DEACTIVE_MODE, ACTIVE_MODE};
extern volatile Operation current;

// Channel lists for each radio when all three run together.
// Sizes are spelled out so sizeof() still works in other files.
extern byte channelGroup_1[4];
extern byte channelGroup_2[4];
extern byte channelGroup_3[4];

const byte bluetooth_channels[] =        {32, 34, 46, 48, 50, 52, 0, 1, 2, 4, 6, 8, 22, 24, 26, 28, 30, 74, 76, 78, 80};
const byte ble_channels[]       =        {2, 26, 80};
// FIX (2026-11-19): MAJOR BUG. This list used to be {1,2,3,...,12} — i.e.
// WiFi *channel numbers*. But nRF24 channel numbers are MHz offsets from
// 2400 (setChannel(6) = 2406 MHz), NOT WiFi channels (WiFi ch 6 = 2437 MHz).
// So WiFi mode was transmitting only in 2401-2412 MHz — a dead sliver that
// barely touches the lower edge of WiFi channel 1 and leaves every real
// network untouched. Correct conversion: WiFi channel N centers at
// 2412 + (N-1)*5 MHz, which is nRF24 channel 12 + (N-1)*5.
// Below: WiFi channels 1 through 13, properly translated.
//   WiFi 1 -> 12 (2412 MHz), WiFi 6 -> 37 (2437), WiFi 11 -> 62 (2462), WiFi 13 -> 72 (2472)
const byte WiFi_channels[]      =        {12, 17, 22, 27, 32, 37, 42, 47, 52, 57, 62, 67, 72};
const byte usbWireless_channels[] =      {40, 50, 60};
const byte videoTransmitter_channels[] = {70, 75, 80};
const byte rc_channels[]        =        {1, 3, 5, 7};
const byte zigbee_channels[]    =        {11, 15, 20, 25};
const byte nrf24_channels[]     =        {76, 78, 79};

// Button-press flags set by the interrupt handlers (see RfClown.ino).
// FIX (2026-11-19): announced here, defined once in setting.cpp.
extern volatile bool ChangeRequested;
extern volatile bool ChangeRequested1;
extern volatile bool ChangeRequested2;

// Screensaver + LED settings-card state (added 2026-11-19). The center
// button now also drives these: wake from screensaver, start the screensaver,
// and toggle the LED preference.
extern volatile bool screensaverActive;   // animation currently on screen
extern volatile bool enterScreensaver;    // set by center press on Screensaver card
extern volatile bool wakeRequested;       // set by any center press while animating
extern volatile bool toggleLedRequested;  // set by center press on LED card
extern OperationMode lastJamMode;         // last REAL jamming mode focused — the
                                          // Screensaver/LED cards keep it running
extern bool ledEnabled;                   // user's LED preference (persisted EEPROM)

extern unsigned long lastPressTime;
const unsigned long debounceDelay = 200;

#endif // CONFIG_H
