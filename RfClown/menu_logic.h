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
