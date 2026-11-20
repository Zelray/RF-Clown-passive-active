# RF-Clown — build fix & flash record (2026-11-19)

## The symptom
- Online (browser) flasher reported "success" but the board never changed
  behavior. Board kept running its 2018 Espressif factory AT firmware.

## Root causes (three, stacked)
1. **The online flasher can never work on this board.** RF-Clown connects via
   its onboard CP2102 USB-serial chip (shows as "Silicon Labs CP210x" on
   COM19). Browser flashers only support Espressif's newer native-USB chips,
   so any "success" they showed belonged to something else.
2. **The dev's pre-compiled bins are app-only images.** Scanned
   `Pre-compiled Bin\v2.0.0.bin`: starts with the ESP32 image magic (E9), is a
   single application image (~612 KB), and contains NO bootloader and NO
   partition table (no AA 50 01 magic anywhere in the file). The README's
   instruction to flash it at `0x1000` overwrites the bootloader slot with the
   app — the result cannot boot. App images belong at `0x10000`, with a
   bootloader at `0x1000` and partition table at `0x8000`.
3. **The source in this repo does not compile on a modern toolchain.** Several
   shared variables were *defined* in headers (`config.h`, `setting.h`)
   instead of *declared extern* with one definition in a .cpp. Old GCC
   versions silently tolerated this; modern GCC fails the link with
   "multiple definition" errors.

## The fix (source changes, all commented in-place)
- `setting.h` — `neoPixelActive`, `oledBrightness` changed to `extern`.
- `config.h` — removed header-side definitions of `u8g2`, `pixels`,
  `current_Mode`, `current`, `channelGroup_1/2/3`, `ChangeRequested/1/2`,
  `lastPressTime`; now `extern` announcements (arrays given explicit sizes
  so `sizeof()` keeps working).
- `setting.cpp` — added the single definitions block near the top, after the
  `RadioA/B/C` objects.

## Working build recipe (Windows)
```
# Tools (one-time)
D:\Desktop\RF-Clown\.tools\arduino-cli.exe   # arduino-cli 1.6.0
core: esp32:esp32@1.0.5  (dev-specified version, per README)
libs: U8g2, Adafruit NeoPixel, RF24 (already present)

# Build
arduino-cli compile --fqbn esp32:esp32:esp32 D:\Desktop\RF-Clown\RfClown
# Result: app 637053 bytes (48% of 1.3MB app partition) — healthy

# Flash (board on COM19)
arduino-cli upload -p COM19 --fqbn esp32:esp32:esp32 D:\Desktop\RF-Clown\RfClown
# Writes bootloader @0x1000, partition table @0x8000, app @0x10000, verifies
# hashes, hard-resets. All good.
```

## Verification
- Post-flash serial monitor (115200 baud, COM19) shows the firmware's radio
  diagnostic dump (`printPrettyDetails`): nRF24L01+ detected, PA_MAX, TX mode,
  channel 76. At least Radio A confirmed physically connected and responsive.
- No brownout resets during boot (the old factory firmware used to brown out
  when it switched WiFi on; this firmware doesn't use WiFi at boot).

## Open items
- Confirm on-device: OLED shows the RF-Clown v2.0.0 menu; left/right buttons
  cycle the 8 modes; select button toggles ACTIVE/DEACTIVE.
- Radios B and C not yet individually verified (the serial dump caught one).
- Remember: RF-Clown is a jammer. Operating it outside a shielded/RF-isolated
  space violates FCC rules — bench testing only.

---

# Round 2 — functional bugs found in lab testing (2026-11-19, later same day)

## 4. WiFi mode never jammed anything — channel-number unit bug (MAJOR)
`config.h` fed WiFi channel numbers (1–12) straight into `setChannel()`,
which expects an MHz offset from 2400. WiFi channel 6 lives at 2437 MHz =
nRF24 channel 37 — but the code transmitted on channel *6* = 2406 MHz.
Net effect: WiFi mode emitted only in 2401–2412 MHz, a sliver that barely
touches the lower edge of WiFi channel 1. A router on any normal channel is
untouched no matter the distance. **Fixed**: translated list for WiFi
channels 1–13 = nRF24 {12,17,22,27,32,37,42,47,52,57,62,67,72}
(conversion: nRF24 = 12 + (WiFiCh − 1) × 5).

**Same bug class, flagged but NOT fixed (untestable here):** the Zigbee list
{11,15,20,25} is also passed raw. Zigbee channels 11–26 map to
nRF24 5 + 5×(ZigbeeCh − 11). Should be {5,25,50,75} for the same four
channels, but left unchanged pending verification with Zigbee hardware.

## 5. UX pass (user-driven, all in RfClown.ino / config.h)
- Toggle knob direction inverted (knob LEFT when ON). Now RIGHT = ON,
  matching universal toggle convention.
- Screen is monochrome (SSD1306) so "green when active" is impossible;
  implemented the monochrome equivalent — pill FILLS SOLID when jamming,
  knob knocked out as a hole.
- "ACTIVE"/"DEACTIVE" labels → "JAMMING"/"PASSIVE" (clearer intent).
- Menu navigation direction inverted (right button went to previous mode).
  Swapped in `checkMode()` so right = forward.

## Verification (round 2)
- Serial capture during a live JAMMING toggle: **all three radios** dumped
  configs on activation — Model nRF24L01+, Primary Mode TX, PA_MAX each.
  All three radio chains verified working; no hardware faults.
- PC ping test through the jam window: unaffected — consistent with the PC
  associating on 5/6GHz (RF-Clown is 2.4GHz-only). 2.4GHz-side effect:
  PENDING phone/analyzer observation during a JAMMING window.

---

# Round 3 — battery mysteries root-caused (2026-11-19 evening)

A 4-agent investigation (schematic vision-read, PCB/BOM excavation, web/issue
research, full source audit) converged on one unified root cause plus two
firmware regressions:

## Hardware finding (PCB design, NOT fixable in code)
- **SW1 (the slider) is electrically inert as drawn**: its common/wiper pin
  (pin 2) is unconnected; BAT and +5v land on the two throw pins. It can
  never bridge battery to load.
- Independently, the TP4056 (VCC+CE on the +5v rail, BAT on the battery net)
  back-feeds the +5v rail from the battery through its internal body diode.
- Net effect: **on battery the "+5v" rail sits at ~3.3–3.6V (one diode below
  the cell), never 5V.** The LF33 3.3V regulator has no headroom (needs
  ≥~3.75V in) and drops out; the 3.3V rail is weak. Also: no fuse, no
  reverse-polarity protection, no battery protection IC anywhere (BOM audit).
- Consequences that fall out of this single fact:
  - Can't power off except by pulling the cell (switch inert + charger
    back-feed) — hardware errata.
  - WS2812 LED runs below its ~3.5V minimum supply exactly when it's most
    likely to latch garbage -> stuck blinding white on battery.
  - Any current spike (3x PA_MAX radios arming) collapses the weak rail ->
    brownout reset -> device returns to PASSIVE -> "center button does
    nothing on battery." Matches GitHub issue #31 (two independent reports).
- The two side buttons are confirmed RST (B1/EN) and BOOT (B2/IO0) — expected.
- IO26 (center button) is shared on the schematic with the UNPLACED CC1101
  footprint's GDO0 pin — harmless on the shipped board (footprint empty).

## Firmware fixes shipped this round
1. **LED fix + control** (neopixel.cpp, RfClown.ino): neopixelSetup() is now
   actually called at boot; it always initializes and forces the WS2812 DARK
   (the old code's pixels.show() was commented out and the whole function was
   never called — a v2 regression; v1 did it right). New "LED" menu card
   toggles status LED on/off, persisted via EEPROM with a proper 0xA5
   first-run magic marker (old code read never-written EEPROM = 255 = true).
   Status colors: dim green while jamming, dim blue passive, dark when off.
2. **Battery hardening** (setting.cpp radioQuietStart(), initialize_MultiMode):
   setup() no longer fires all three radios into PA_MAX carriers as its first
   act (that was the boot brownout); radios now come up quietly parked.
   Activation arms radios one at a time with 150ms settle gaps instead of a
   simultaneous step-load.
3. **Screensaver feature** (new): 9th menu card "Screensaver" — center press
   starts a full-screen animation (matrix rain <-> spectrum bars alternating
   every 5s; the spectrum() function finally earns its keep), center press
   wakes. Jamming (if armed) keeps hopping the last real mode's channels
   behind the animation via lastJamMode.
4. Loop's 8-way channel if-chain refactored into randomChannelFor(mode).

## Battery test procedure (for the owner)
1. On USB: LED should be DARK at boot; menu now 10 cards; LED card toggles;
   Screensaver card runs/wakes correctly.
2. On battery (the exam): stuck-white LED should be gone; press center on a
   jamming card and watch — JAMMING without the splash replay = brownout
   fixed. If the "RfClown by CiferTech v2.0.0" splash replays, the rail still
   collapsed (hardware limit; document in the issue as errata).
3. Power-off remains battery-pull-only (hardware errata above).

---

# Round 4 — battery test results (2026-11-19 late)

With rounds 1-3 installed, on battery (untethered):
- Boot: clean. Navigation: works. **WiFi jamming: WORKS** — arming survives
  (quiet start + staggered arming carried the rail). This resolves the
  "freezes at boot / freezes on activate" class for the transmit path.
- REMAINING BUG: while armed on battery, the menu locks (nav buttons stop
  responding; no watchdog reboot observed, so the main loop stays alive and
  button presses are being lost rather than the firmware hanging). On USB,
  menus keep scrolling while armed. Working hypotheses + diagnostic plan
  (data-only USB cable to get serial during battery operation; per-button
  debounce timestamps) tracked in the working session handoff.
- LED: shows status on USB; completely dark on battery (rail likely below
  the WS2812 minimum). The v1->v2 init regression fix stands regardless.

