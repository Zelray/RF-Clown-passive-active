/* ____________________________
   This software is licensed under the MIT License:
   https://github.com/cifertech/rfclown
   ________________________________________ */
   
#include "setting.h"
#include "config.h"

extern Adafruit_NeoPixel pixels;

// ---------------------------------------------------------------------------
// LED FIX (2026-11-19): this module used to be completely orphaned — nothing
// ever called neopixelSetup(), so the WS2812's data pin floated from power-up
// and the LED latched random garbage, classically BLINDING WHITE, forever.
// That also wasted ~60mA continuously, which mattered a lot on battery power.
//
// neopixelSetup() now: (1) always initializes the LED and forces it DARK as
// one of the first acts of boot, and (2) reads the user's saved LED
// preference with a proper "magic number" first-run check. The old code read
// EEPROM byte 0 directly — on a never-written chip that returns 255 (erased
// flash), which it treated as "true" — so the preference could never work.
//
// EEPROM layout: byte 0 = 0xA5 "this chip has been initialized" marker,
//                byte 1 = ledEnabled (1 = show status colors, 0 = keep dark)
// ---------------------------------------------------------------------------
void neopixelSetup() {
  EEPROM.begin(512);

  if (EEPROM.read(0) != 0xA5) {         // first boot on this firmware
    EEPROM.write(0, 0xA5);
    EEPROM.write(1, 1);                 // default: status LED enabled
    EEPROM.commit();
  }
  ledEnabled = EEPROM.read(1);

  pixels.begin();
  pixels.clear();
  pixels.show();                        // force DARK immediately (the old code
                                        // never sent this frame — show() was
                                        // commented out!)
  neoPixelActive = ledEnabled;          // kept for compatibility
}

// Save the user's LED on/off choice so it survives reboots.
void ledSavePreference() {
  EEPROM.write(1, ledEnabled ? 1 : 0);
  EEPROM.commit();
}

// Apply the LED to the current device state:
//   LED preference OFF -> dark, always.
//   ON + jamming armed -> dim green (the dev's intended "active" color).
//   ON + passive       -> dim blue  ("standby" color). Swap for "null" if you
//                          prefer fully dark while passive.
void ledApply() {
  if (!ledEnabled) { setNeoPixelColour("null"); return; }
  setNeoPixelColour(current == ACTIVE_MODE ? "green" : "blue");
}

void neopixelLoop();

// BRIGHTNESS FIX (2026-11-19): the original status colors used level 5 out
// of 255 — effectively invisible to the human eye, so "the LED card does
// nothing." Raised to 40: clearly visible, not blinding (the old full-white
// garbage latch was ~255 per channel — THAT was blinding).
// HARDWARE NOTE: on USB power the WS2812 runs at 5V, where its data-input
// threshold (~3.5V) sits ABOVE the ESP32's 3.3V "high" — one-bits are
// marginal, so color commands may not land on USB. On battery the rail is
// ~3.4V, the threshold drops to ~2.4V, and data lands reliably. "All off"
// frames always work everywhere (zero-bits are 0V, always read). That's why
// force-dark succeeded on both supplies but colors were unreliable.
void setNeoPixelColour(const std::string& colour) {
  uint32_t colorValue = 0;

  if (colour == "red") {
    colorValue = pixels.Color(40, 0, 0);
  } else if (colour == "green") {
    colorValue = pixels.Color(0, 40, 0);
  } else if (colour == "blue") {
    colorValue = pixels.Color(0, 0, 40);
  } else if (colour == "yellow") {
    colorValue = pixels.Color(40, 40, 0);
  } else if (colour == "purple") {
    colorValue = pixels.Color(40, 0, 40);
  } else if (colour == "cyan") {
    colorValue = pixels.Color(0, 40, 40);
  } else if (colour == "white") {
    colorValue = pixels.Color(40, 40, 40);
  } else if (colour == "null") {
    colorValue = pixels.Color(0, 0, 0);
  }

  pixels.setPixelColor(0, colorValue);
  pixels.show();
}

void flash(int numberOfFlashes, const std::vector<std::string>& colors, const std::string& finalColour) {
  if (numberOfFlashes <= 0 || colors.empty()) {
    Serial.println("Invalid parameters for flash: Check numberOfFlashes or colors vector.");
    return;
  }

  for (int i = 0; i < numberOfFlashes; ++i) {
    for (const auto& color : colors) {
      setNeoPixelColour(color);
      delay(500);
    }
  }
  setNeoPixelColour(finalColour);
}
