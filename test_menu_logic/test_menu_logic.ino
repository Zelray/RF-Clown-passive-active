/* ____________________________
   This software is licensed under the MIT License:
   https://github.com/cifertech/rfclown
   ________________________________________ */

// test_menu_logic.ino — on-device unit tests for the menu decision rules
// (menu_logic.h). Flashed by itself to the board; prints PASS/FAIL lines
// at 115200 baud over serial, then a summary. This is the RED/GREEN
// runner for the TDD cycle on 2026-11-19:
//   RED  = flashed against the code as-it-was (direction swapped, status
//          lying on non-focused cards) -> FAIL lines expected.
//   GREEN = after fixing menu_logic.h -> all PASS.
//
// Card order under test (index 0-9):
//   0 WiFi, 1 Video TX, 2 RC, 3 BLE, 4 Bluetooth,
//   5 USB Wireless, 6 Zigbee, 7 NRF24, 8 Screensaver, 9 LED

// menu_logic.h lives in the firmware folder (../RfClown). Arduino builds
// from a temp dir, so the plain filename is used and the real path is fed
// to the compiler via --build-property ...extra_flags=-I<firmware folder>.
// Single source of truth: the test compiles the SAME header the firmware
// ships with.
#include "menu_logic.h"

int failures = 0;

void check(const char* name, bool ok) {
  Serial.print(ok ? "PASS  " : "FAIL  ");
  Serial.println(name);
  if (!ok) failures++;
}

void setup() {
  Serial.begin(115200);
  delay(1500);  // let the serial console attach

  Serial.println("== RF-Clown menu logic tests ==");

  // --- Spec 1: the RIGHT button ADVANCES through the cards -------------
  check("right from WiFi(0) advances to Video TX(1)",        menu::nextIndex(0) == 1);
  check("right from Zigbee(6) advances to NRF24(7)",         menu::nextIndex(6) == 7);
  check("right from Screensaver(8) advances to LED(9)",      menu::nextIndex(8) == 9);
  check("right from LED(9) wraps around to WiFi(0)",         menu::nextIndex(9) == 0);

  // --- Spec 2: the LEFT button goes BACK through the cards -------------
  check("left from Video TX(1) goes back to WiFi(0)",        menu::prevIndex(1) == 0);
  check("left from LED(9) goes back to Screensaver(8)",      menu::prevIndex(9) == 8);
  check("left from WiFi(0) wraps around to LED(9)",          menu::prevIndex(0) == 9);

  // --- Spec 3 (v2, FROZEN TARGET): arming jams the card that was focused
  // at arm time; browsing while armed NEVER retargets the transmitter, and
  // only the armed target's card shows JAMMING. (Cycle 1 made the status
  // global-truth; the owner's acceptance test then exposed that the radios
  // live-follow browsing — the real bug. Cycle 2 freezes the target.)
  // Card indexes: 0 WiFi, 3 BLE, 6 Zigbee, 9 LED.
  check("armed on WiFi: WiFi card shows JAMMING",            strcmp(menu::statusForCard(true, 0, 0), "JAMMING") == 0);
  check("armed on WiFi: Zigbee card shows PASSIVE",          strcmp(menu::statusForCard(true, 6, 0), "PASSIVE") == 0);
  check("armed on WiFi: BLE card shows PASSIVE",             strcmp(menu::statusForCard(true, 3, 0), "PASSIVE") == 0);
  check("armed on Zigbee: Zigbee card shows JAMMING",        strcmp(menu::statusForCard(true, 6, 6), "JAMMING") == 0);
  check("armed on Zigbee: WiFi card shows PASSIVE",          strcmp(menu::statusForCard(true, 0, 6), "PASSIVE") == 0);
  check("disarmed: every card shows PASSIVE",                strcmp(menu::statusForCard(false, 0, 0), "PASSIVE") == 0
                                                            && strcmp(menu::statusForCard(false, 6, 0), "PASSIVE") == 0);

  // --- Spec 3 legacy (device-level truth, kept for the header dot) -----
  check("device armed renders as JAMMING at device level",   strcmp(menu::jamStatusText(true, true), "JAMMING") == 0);
  check("device disarmed renders as PASSIVE at device level", strcmp(menu::jamStatusText(false, true), "PASSIVE") == 0);

  // --- Spec 4 (by construction): navigation never touches the armed ---
  // nextIndex/prevIndex return only a card index; the armed flag is not
  // an input or output of any function in menu_logic.h. (Wiring assertion:
  // checkMode() must keep calling only these when a nav button fires.)

  // --- Spec 5 (battery UI-lock fix, 2026-11-19): each button has its OWN
  // debounce clock, and a press only counts if the pin is STILL low when
  // the handler runs. This is what was supposed to happen on the battery-
  // armed lock: RF noise spikes the lines, ghost edges must be rejected
  // WITHOUT starving a real, held press on any button.
  {
    unsigned long clockL = 0, clockR = 0;         // two buttons' own clocks
    const unsigned long D = 250;                  // debounce window under test

    check("accepts a real press when the pin is held low",
          btn::accept(10000, clockL, D, true));
    check("rejects a repeat press inside the debounce window",
          !btn::accept(10100, clockL, D, true));
    check("accepts again once the window has passed",
          btn::accept(10300, clockL, D, true));
    check("rejects a ghost edge (pin NOT still low)",
          !btn::accept(11000, clockL, D, false));
    check("a rejected ghost edge does not advance the clock",
          btn::accept(11000, clockL, D, true));
    check("one button's noise never starves another button (own clocks)",
          btn::accept(10000, clockR, D, true) && !btn::accept(10100, clockR, D, true));
    // The shared-clock bug itself, expressed as a test: under the OLD rule a
    // spike at t=10400 (say on the RIGHT line) would block a REAL LEFT press
    // at t=10500. Under the new rule the LEFT clock only knows its OWN past.
    check("a spike on one line cannot block a real press on another",
          btn::accept(10400, clockR, D, true) == true &&   // right "press" (noise)
          btn::accept(10500, clockL, D, true) == true);    // left press still lands
  }

  Serial.println("------------------------------");
  Serial.print(failures == 0 ? "ALL TESTS PASSED" : "TESTS FAILED: ");
  if (failures > 0) Serial.println(failures);
  Serial.println();
}

void loop() { /* tests run once at boot */ }
