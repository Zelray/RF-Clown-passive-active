/* ____________________________
   This software is licensed under the MIT License:
   https://github.com/cifertech/rfclown
   ________________________________________ */

#include "setting.h"
#include "config.h"

RF24 RadioA(NRF_CE_PIN_A, NRF_CSN_PIN_A);
RF24 RadioB(NRF_CE_PIN_B, NRF_CSN_PIN_B);
RF24 RadioC(NRF_CE_PIN_C, NRF_CSN_PIN_C);

// ---------------------------------------------------------------------------
// FIX (2026-11-19): single home for all shared variables.
// These used to be created inside config.h / setting.h, so every source file
// got its own copy and the linker failed with "multiple definition" errors.
// The headers now only *announce* them (extern); this block is the one copy.
// ---------------------------------------------------------------------------
bool neoPixelActive = false;       // saved LED preference, loaded from EEPROM
uint8_t oledBrightness = 100;      // screen brightness setting (0-255 scale)

U8G2_SSD1306_128X64_NONAME_F_HW_I2C u8g2(U8G2_R0, /* reset=*/ U8X8_PIN_NONE);
Adafruit_NeoPixel pixels(1, 14, NEO_GRB + NEO_KHZ800);  // one LED on GPIO 14

OperationMode current_Mode = WiFi_MODULE;  // which of the 8 modes is selected
volatile Operation current = DEACTIVE_MODE; // jamming OFF until the user toggles it

// Channel lists each radio sweeps when all three run together.
byte channelGroup_1[] = {2, 5, 8, 11};
byte channelGroup_2[] = {26, 29, 32, 35};
byte channelGroup_3[] = {80, 83, 86, 89};

// Button flags set by interrupts in RfClown.ino, read in the main loop.
volatile bool ChangeRequested  = false;
volatile bool ChangeRequested1 = false;
volatile bool ChangeRequested2 = false;

// Screensaver + LED card state (2026-11-19) — see config.h for the story.
volatile bool screensaverActive  = false;
volatile bool enterScreensaver   = false;
volatile bool wakeRequested      = false;
volatile bool toggleLedRequested = false;
OperationMode lastJamMode        = WiFi_MODULE;
OperationMode armedMode          = WiFi_MODULE;  // frozen jam target (see config.h)
bool ledEnabled                  = true;

// BUTTON FIX (2026-11-19): one debounce clock PER BUTTON (used to be one
// shared clock — RF noise on any button line starved all three; see
// menu_logic.h namespace btn for the full story).
volatile unsigned long lastPressL = 0;
volatile unsigned long lastPressR = 0;
volatile unsigned long lastPressS = 0;
// ---------------------------------------------------------------------------

// ---------------------------------------------------------------------------
// BATTERY FIX (2026-11-19): setup() used to call initialize_MultiMode() as
// its very first act — firing ALL THREE radios into max-power constant
// carriers, then powering them back down if passive. That boot spike collapses
// the battery rail (the v2 power design leaves only ~3.3-3.6 V there on
// battery — see BUILD-FIX-NOTES.md), causing brownout resets at boot and on
// every activation (matches GitHub issue #31, "freezes on battery"). This
// gentle start configures each radio and immediately parks it powered-down:
// no transmission, no spike.
// ---------------------------------------------------------------------------
void radioQuietStart() {
  RadioA.begin();  RadioA.powerDown();  digitalWrite(NRF_CE_PIN_A, LOW);
  RadioB.begin();  RadioB.powerDown();  digitalWrite(NRF_CE_PIN_B, LOW);
  RadioC.begin();  RadioC.powerDown();  digitalWrite(NRF_CE_PIN_C, LOW);
}

void setRadiosNeutralState() {
  RadioA.stopListening();
  RadioA.setAutoAck(false);
  RadioA.setRetries(0, 0);
  RadioA.powerDown(); 
  digitalWrite(NRF_CE_PIN_A, LOW); 

  RadioB.stopListening();
  RadioB.setAutoAck(false);
  RadioB.setRetries(0, 0);
  RadioB.powerDown(); 
  digitalWrite(NRF_CE_PIN_B, LOW); 

  RadioC.stopListening();
  RadioC.setAutoAck(false);
  RadioC.setRetries(0, 0);
  RadioC.powerDown(); 
  digitalWrite(NRF_CE_PIN_C, LOW); 
}

void configureNrf(RF24 &radio) {
  radio.begin();
  radio.setAutoAck(false);
  radio.stopListening();
  radio.setRetries(0, 0);
  radio.setPALevel(RF24_PA_MAX, true);
  radio.setDataRate(RF24_2MBPS);
  radio.setCRCLength(RF24_CRC_DISABLED);
}

void setupRadioA() {
  configureNrf(RadioA);
}

void setupRadioB() {
  configureNrf(RadioB);
}

void setupRadioC() {
  configureNrf(RadioC);
}

void initAllRadios() {
  setupRadioA();
  setupRadioB();
  setupRadioC();
}

void Str(uint8_t x, uint8_t y, const uint8_t* asciiArray, size_t len) {
  char buf[64]; 
  for (size_t i = 0; i < len && i < sizeof(buf) - 1; i++) {
    buf[i] = (char)asciiArray[i];
  }
  buf[len] = '\0';

  u8g2.drawStr(x, y, buf);
}

void CenteredStr(uint8_t screenWidth, uint8_t y, const uint8_t* asciiArray, size_t len, const uint8_t* font) {
  char buf[64];
  for (size_t i = 0; i < len && i < sizeof(buf) - 1; i++) {
    buf[i] = (char)asciiArray[i];
  }
  buf[len] = '\0';

  u8g2.setFont((const uint8_t*)font);
  int16_t w = u8g2.getUTF8Width(buf);
  u8g2.setCursor((screenWidth - w) / 2, y);
  u8g2.print(buf);
}

void conf() {
  u8g2.setBitmapMode(1);
  u8g2.clearBuffer();
  CenteredStr(128, 15, txt_n, sizeof(txt_n), u8g2_font_ncenB14_tr);
  CenteredStr(120, 35, txt_c, sizeof(txt_c), u8g2_font_ncenB08_tr);
  CenteredStr(128, 50, txt_v, sizeof(txt_v), u8g2_font_6x10_tf);
  u8g2.sendBuffer();
  delay(3000);
  u8g2.clearBuffer();
  u8g2.drawXBMP(0, 0, 128, 64, cred); 
  u8g2.sendBuffer();
  delay(250);
}

 
