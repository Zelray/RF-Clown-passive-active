/* ____________________________
   This software is licensed under the MIT License:
   https://github.com/cifertech/rfclown
   ________________________________________ */

// menu_logic.h — the menu's decision rules, extracted (2026-11-19) from
// RfClown.ino into pure functions so they can be unit-tested on-device
// (see ../test_menu_logic/). No globals, no hardware — the sketch wires
// real buttons and the screen to these decisions.
//
// Menu card order (index 0-9): WiFi, Video TX, RC, BLE, Bluetooth,
// USB Wireless, Zigbee, NRF24, Screensaver, LED.
#pragma once

namespace menu {

constexpr int MODE_COUNT = 10;

// "Next" = the card the RIGHT button moves to: ADVANCE, wrapping 9 -> 0.
// (GREEN 2026-11-19: the 2026-11-19 direction swap had this backwards — it
// misread the owner's complaint about the slide ANIMATION as a complaint
// about selection direction. Verified on-device by test_menu_logic.)
inline int nextIndex(int i) { return (i + 1) % MODE_COUNT; }

// "Prev" = the card the LEFT button moves to: BACK, wrapping 0 -> 9.
inline int prevIndex(int i) { return (i == 0) ? (MODE_COUNT - 1) : (i - 1); }

// Status text for a jamming card: THE DEVICE'S armed state, shown on every
// card. (Superseded 2026-11-19 by statusForCard — see below — after the
// owner's acceptance test exposed the live-retarget behavior. Retained for
// the header-bar device-level indicator decision: armed = transmitting.)
inline const char* jamStatusText(bool armed, bool focused) {
  (void)focused;
  return armed ? "JAMMING" : "PASSIVE";
}

// FROZEN-TARGET RULE (test-driven 2026-11-19, second cycle): arming jams
// the card that was focused AT THE MOMENT OF ARMING, and browsing while
// armed NEVER retargets the live transmitter. Only the armed target's card
// shows JAMMING — every other card truthfully shows PASSIVE, because that
// mode genuinely is not transmitting.
// (This replaced the upstream semantic of "the radios follow whichever card
// happens to be on screen," which meant navigating while armed silently
// changed what frequencies were being jammed.)
inline const char* statusForCard(bool armed, int cardIdx, int armedTargetIdx) {
  // GREEN: only the armed TARGET's card is transmitting, so only that card
  // says JAMMING. Browsing onto other cards shows PASSIVE — truthfully.
  return (armed && cardIdx == armedTargetIdx) ? "JAMMING" : "PASSIVE";
}

}  // namespace menu

// ============================================================================
// namespace btn — BUTTON PRESS ACCEPTANCE RULE (added 2026-11-19, battery
// UI-lock fix). Pure function so the on-device test suite can verify it.
//
// THE BUG IT FIXES: all three button interrupts used to share ONE debounce
// clock ("lastPressTime"). When the radios transmit at max power, they induce
// stray voltage spikes on the button traces; every spike — on ANY of the
// three lines — refreshed that shared clock, so a REAL press always looked
// like "too soon after the last one" and was thrown away. On battery (weak
// ~3.4V rail, internal pull-ups only, no filter caps on the v2 board) the
// spikes are frequent enough to starve all three buttons permanently: the
// jamming keeps running, the screen stays normal, but every press is ignored
// until power is pulled. No watchdog reboot happens because the code never
// hangs — it just rejects the input. (Exactly what the owner observed live.)
//
// THE RULE: a press is accepted only if
//   1. the pin is STILL low when the handler runs (a finger holds the line
//      down for 100+ ms; an RF-induced ghost spike is over in microseconds),
//   AND 2. this button's OWN clock says enough time passed since this
//      button's last accepted press (mechanical contact bounce guard).
// Each button keeps its own clock, so noise on one line can never starve
// another button.
// ============================================================================
namespace btn {

// now          = millis() at the moment of the interrupt
// lastPressMs  = this button's OWN last-accepted-press time (updated on accept;
//                declared volatile because interrupt handlers own it)
// debounceMs   = minimum spacing between accepted presses of this button
// pinStillLow  = true if the button line reads LOW right now (i.e. held)
// Returns true (and advances the clock) only for an accepted press.
inline bool accept(unsigned long now,
                   volatile unsigned long& lastPressMs,
                   unsigned long debounceMs,
                   bool pinStillLow) {
  if (!pinStillLow) return false;              // ghost spike: gone already -> reject, clock untouched
  if (now - lastPressMs < debounceMs) return false;  // too soon after this button's last press
  lastPressMs = now;                           // accepted: advance THIS button's clock
  return true;
}

}  // namespace btn
