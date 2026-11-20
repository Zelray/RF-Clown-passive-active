/* ____________________________
   This software is licensed under the MIT License:
   https://github.com/cifertech/rfclown
   ________________________________________ */
   
#include "setting.h"
#include "config.h"
#include "menu_logic.h"     // tested navigation + status rules (test_menu_logic)
#include <esp_task_wdt.h>   // FREEZE FIX (2026-11-19) — see setup()

void IRAM_ATTR handleButton() {
  unsigned long currentTime = millis();
  if (currentTime - lastPressTime > debounceDelay) {
    ChangeRequested = true;
    lastPressTime = currentTime;
  }
}

void IRAM_ATTR handleButton1() {
  unsigned long currentTime = millis();
  if (currentTime - lastPressTime > debounceDelay) {
    ChangeRequested1 = true;
    lastPressTime = currentTime;
  }
}

void IRAM_ATTR handleButton2() {
  unsigned long currentTime = millis();
  if (currentTime - lastPressTime > debounceDelay) {
    // FEATURE (2026-11-19): the center button is now context-aware:
    //   - while the screensaver is showing -> any press WAKES it
    //   - on the Screensaver card          -> press STARTS the animation
    //   - on the LED card                  -> press toggles the LED on/off
    //   - on any jamming card              -> press arms/disarms as before
    if (screensaverActive) {
      wakeRequested = true;
    } else if (current_Mode == SCREENSAVER_MODULE) {
      enterScreensaver = true;
    } else if (current_Mode == LED_MODULE) {
      toggleLedRequested = true;
    } else if (current == DEACTIVE_MODE) {
      current = ACTIVE_MODE;
    } else {
      current = DEACTIVE_MODE;
    }
    lastPressTime = currentTime;
  }
}

void configure_Radio(RF24 &radio, const byte *channels, size_t size) {
  configureNrf(radio);
  radio.printPrettyDetails();
  for (size_t i = 0; i < size; i++) {
    radio.setChannel(channels[i]);
    radio.startConstCarrier(RF24_PA_MAX, channels[i]);
  }
}

void initialize_MultiMode() {
  // BATTERY FIX (2026-11-19): the three radios used to slam into max-power
  // carriers SIMULTANEOUSLY. On battery the v2 power rail can't take that
  // step-load and the chip browns out (looks like "the button did nothing").
  // Now each radio gets a 150 ms settle window before the next one arms.
  if (RadioA.begin()) {
    configure_Radio(RadioA, channelGroup_1, sizeof(channelGroup_1));
    delay(150);
  }
  if (RadioB.begin()) {
    configure_Radio(RadioB, channelGroup_2, sizeof(channelGroup_2));
    delay(150);
  }
  if (RadioC.begin()) {
    configure_Radio(RadioC, channelGroup_3, sizeof(channelGroup_3));
    delay(150);
  }
}

void initialize_Radios() {
  if (current == ACTIVE_MODE) {
    initialize_MultiMode();
  } else if (current == DEACTIVE_MODE) {
    RadioA.powerDown();
    RadioB.powerDown();
    RadioC.powerDown();
    delay(100);
  }
}

static const uint8_t HEADER_H = 12;
static const uint8_t PAD      = 6;
static const uint8_t CARD_H   = 36;
static const uint8_t RADIUS   = 3;
static const uint8_t DOT_Y    = 64 - 6;

static const char* kMenuLabels[] = {
  "WiFi", "Video TX", "RC", "BLE", "Bluetooth", "USB Wireless", "Zigbee", "NRF24",
  "Screensaver", "LED"
};
static const int kMenuCount = sizeof(kMenuLabels)/sizeof(kMenuLabels[0]);

static int menuIndexFromMode(OperationMode m) {
  switch (m) {
    case WiFi_MODULE:         return 0;
    case VIDEO_TX_MODULE:     return 1;
    case RC_MODULE:           return 2;
    case BLE_MODULE:          return 3;
    case Bluetooth_MODULE:    return 4;
    case USB_WIRELESS_MODULE: return 5;
    case ZIGBEE_MODULE:       return 6;
    case NRF24_MODULE:        return 7;
    case SCREENSAVER_MODULE:  return 8;
    case LED_MODULE:          return 9;
    default: return 0;
  }
}
static OperationMode modeFromMenuIndex(int idx) {
  switch (idx) {
    case 0: return WiFi_MODULE;
    case 1: return VIDEO_TX_MODULE;
    case 2: return RC_MODULE;
    case 3: return BLE_MODULE;
    case 4: return Bluetooth_MODULE;
    case 5: return USB_WIRELESS_MODULE;
    case 6: return ZIGBEE_MODULE;
    case 7: return NRF24_MODULE;
    case 8: return SCREENSAVER_MODULE;
    case 9: return LED_MODULE;
    default: return WiFi_MODULE;
  }
}

void spectrum() {
  const uint8_t HDR_H = 10;
  const int TOP = HDR_H + 12;
  const int BOT = SCREEN_H - 2;
  const int H   = BOT - TOP;
  const int STRIDE = 3;
  const int BINS   = SCREEN_W / STRIDE;
  const int BAR_W  = 1;
  static uint8_t h[BINS];
  static int8_t  v[BINS];
  static bool init = false;
  if (!init) {
    for (int i = 0; i < BINS; ++i) {
      h[i] = random(0, H + 1);
      v[i] = random(-2, 3);
    }
    init = true;
  }
  for (int i = 0; i < BINS; ++i) {
    int8_t a = (int8_t)random(-1, 2);
    v[i] += a;
    if (v[i] > 3) v[i] = 3;
    if (v[i] < -3) v[i] = -3;
    int16_t nh = (int16_t)h[i] + v[i];
    if (nh < 0) {
      nh = 0;
      v[i] = -(v[i] * 3) / 4;
      if (v[i] == 0) v[i] = 1;
    } else if (nh > H) {
      nh = H;
      v[i] = -(v[i] * 3) / 4;
      if (v[i] == 0) v[i] = -1;
    }
    h[i] = (uint8_t)nh;
  }
  for (int i = 0; i < BINS; ++i) {
    int x  = i * STRIDE;
    int bh = h[i];
    if (bh <= 0) continue;
    int y  = BOT - bh;
    u8g2.drawVLine(x, y, bh);
  }
  u8g2.sendBuffer();
}

static void drawHeaderBar() {
  const uint8_t W = 128;
  const uint8_t H = HEADER_H;
  const uint8_t TXT_Y = 2;
  u8g2.setDrawColor(1);
  u8g2.drawBox(0, 0, W, H);
  u8g2.setFont(FONT_SMALL);
  u8g2.setDrawColor(0);
  u8g2.drawStr(PAD, TXT_Y, "RF-Clown");
  // ARMED MARKER (2026-11-19): a solid dot at the top-left of the header
  // whenever the device is transmitting, visible from ANY card — belt and
  // suspenders with the per-card truth fix above.
  u8g2.setDrawColor(1);
  if (current == ACTIVE_MODE) u8g2.drawDisc(3, HEADER_H / 2, 2);
  u8g2.setDrawColor(0);
  const char* ver = "v2.0.0";
  int vw = u8g2.getStrWidth(ver);
  u8g2.drawStr(W - vw - 4, TXT_Y, ver);
  u8g2.setDrawColor(1);
  u8g2.drawHLine(0, H - 1, W);
}

static void drawPillOutline(int x,int y,int w,int h){
  if (h & 1) h--; if (y & 1) y--;
  u8g2.drawRFrame(x, y, w, h, h/2);
}
static void drawPillKnob(int x,int y,int w,int h,float pos){
  if (h & 1) h--; if (y & 1) y--;
  int r=h/2, cxL=x+r, cxR=x+w-r-1, cy=y+r;
  int cx=(int)lroundf(cxL + (cxR - cxL)*pos);
  int knob = max(2, r-3);
  u8g2.drawDisc(cx, cy, knob);
}
static void drawPillToggle(int x,int y,int w,int h,bool on){
  // UX FIX (2026-11-19): knob now sits on the RIGHT when ON (standard toggle
  // convention — it used to slide left, which read backwards), and the pill
  // body FILLS when ON. The OLED is monochrome (no green available), so a
  // filled pill is the strongest "this is on" signal the screen can draw.
  drawPillOutline(x,y,w,h);
  if (on) {
    if (h & 1) h--; if (y & 1) y--;
    u8g2.drawRBox(x, y, w, h, h/2);   // solid pill = jamming engaged
    u8g2.setDrawColor(0);             // punch the knob out as a hole
    drawPillKnob(x, y, w, h, 1.0f);   // knob on the right = ON
    u8g2.setDrawColor(1);
  } else {
    drawPillKnob(x, y, w, h, 0.0f);   // outline pill, knob left = OFF
  }
}

static void drawPaginationDots(int active){
  int totalW = (kMenuCount * 6) - 2;
  int startX = (128 - totalW) / 2;
  for (int i=0;i<kMenuCount;i++) {
    int x = startX + i*6;
    if (i == active) u8g2.drawDisc(x, DOT_Y, 2);
    else             u8g2.drawCircle(x, DOT_Y, 2);
  }
}

static void drawCardAtCenter(int centerX, int menuIndex){
  const char* label = kMenuLabels[menuIndex];
  const int W = 128;
  const int w = W - PAD*2;
  const int x = centerX - w/2;
  const int y = HEADER_H + 4;
  u8g2.setDrawColor(0);
  u8g2.drawBox(x+1, y+1, w-2, CARD_H-2);
  u8g2.setDrawColor(1);
  u8g2.drawRFrame(x, y, w, CARD_H, RADIUS);
  u8g2.setFont(FONT_MEDIUM);
  u8g2.setCursor(x + 12, y + 4);
  u8g2.print(label);
  OperationMode cardMode = modeFromMenuIndex(menuIndex);
  bool isThisFocus = (cardMode == current_Mode);
  // UX FIX (2026-11-19): clearer words — "PASSIVE" / "JAMMING" instead of
  // "DEACTIVE" / "ACTIVE". The two settings cards show their OWN state
  // instead of the jam state, so the pill always means "this card's thing".
  const char* status;
  bool isOn;
  if (cardMode == SCREENSAVER_MODULE) {
    status = screensaverActive ? "RUNNING" : "READY";
    isOn   = screensaverActive;
  } else if (cardMode == LED_MODULE) {
    status = ledEnabled ? "ON" : "OFF";
    isOn   = ledEnabled;
  } else {
    // FROZEN TARGET (test-driven cycle 2, 2026-11-19): only the armed
    // TARGET's card shows JAMMING — every other card truthfully shows
    // PASSIVE, because browsing never retargets the transmitter. Verified
    // by test_menu_logic ("armed on WiFi: Zigbee card shows PASSIVE").
    bool deviceArmed = (current == ACTIVE_MODE);
    status = menu::statusForCard(deviceArmed, menuIndex, menuIndexFromMode(armedMode));
    isOn   = deviceArmed && (cardMode == armedMode);
  }
  u8g2.setFont(FONT_SMALL);
  u8g2.setCursor(x + 12, y + 16);
  u8g2.print(status);
  const int pillW=24, pillH=14;
  const int pillY=(y + ((CARD_H-pillH)/2)) & ~1;
  drawPillToggle(x + w - pillW - 10, pillY, pillW, pillH, isOn);
}

static void renderStaticMenu(int focusIndex) {
  u8g2.clearBuffer();
  drawHeaderBar();
  drawCardAtCenter(128 / 2, focusIndex);
  drawPaginationDots(focusIndex);
  u8g2.sendBuffer();
}

static void animateToMenu(int fromIdx, int toIdx) {
  const int W = 128;
  const int STEPS = 14;
  const uint8_t DT = 10;
  const int dir = (toIdx > fromIdx) ? -1 : 1;
  for (int s = 0; s <= STEPS; s++) {
    float t = (float)s / (float)STEPS;
    float e = (t < 0.5f) ? 4.0f*t*t*t : 1.0f - powf(-2.0f*t + 2.0f, 3)/2.0f;
    int shift = (int)(e * W + 0.5f);
    int centerFrom = (W/2) + (-dir * shift);
    int centerTo   = (W/2) + ( dir * (W - shift));
    u8g2.clearBuffer();
    drawHeaderBar();
    drawCardAtCenter(centerFrom, fromIdx);
    drawCardAtCenter(centerTo,   toIdx);
    drawPaginationDots(toIdx);
    u8g2.sendBuffer();
    delay(DT);
  }
  renderStaticMenu(toIdx);
}

static void animateToggleKnobForFocus(int focusIdx, bool fromActive, bool toActive){
  // UX FIX (2026-11-19): animation endpoints flipped to match the corrected
  // toggle convention — knob travels toward the RIGHT when turning on.
  float start = (fromActive ? 1.0f : 0.0f);
  float end   = (toActive   ? 1.0f : 0.0f);
  const int STEPS = 10;
  const uint8_t FRAME_MS = 12;
  for (int s=0; s<=STEPS; s++){
    float t = (float)s / (float)STEPS;
    float e = (t < 0.5f) ? 2.0f*t*t : 1.0f - powf(-2.0f*t + 2.0f, 2)/2.0f;
    float p = start + (end - start) * e;
    u8g2.clearBuffer();
    drawHeaderBar();
    const int W = 128;
    const int w = W - PAD*2;
    const int x = W/2 - w/2;
    const int y = HEADER_H + 4;
    u8g2.setDrawColor(0);
    u8g2.drawBox(x+1, y+1, w-2, CARD_H-2);
    u8g2.setDrawColor(1);
    u8g2.drawRFrame(x, y, w, CARD_H, RADIUS);
    u8g2.setFont(FONT_MEDIUM); u8g2.setCursor(x+12, y+4);  u8g2.print(kMenuLabels[focusIdx]);
    u8g2.setFont(FONT_SMALL);  u8g2.setCursor(x+12, y+16); u8g2.print("----");
    const int pillW=24, pillH=14;
    const int pillY=(y + ((CARD_H-pillH)/2)) & ~1;
    const int pillX=x + w - pillW - 10;
    drawPillOutline(pillX, pillY, pillW, pillH);
    drawPillKnob(pillX, pillY, pillW, pillH, p);
    drawPaginationDots(focusIdx);
    u8g2.sendBuffer();
    delay(FRAME_MS);
  }
  renderStaticMenu(focusIdx);
}

// ---------------------------------------------------------------------------
// Screensaver + channel-hopping helpers (added 2026-11-19)
// ---------------------------------------------------------------------------
// Which animation is currently on screen: false = matrix rain, true = bars.
bool animPhase = false;
unsigned long lastAnimSwitch = 0;

// Menu-state trackers. These used to be locals inside loop(); they moved out
// so the screensaver wake path can re-sync them (otherwise waking after a
// mode change behind the animation triggered a spurious slide).
static Operation     lastActivity = DEACTIVE_MODE;
static OperationMode lastFocus    = WiFi_MODULE;

// Pick a random channel from the given mode's list. Refactor of the old
// 8-way if-chain from loop(), so the Screensaver/LED cards can keep the
// LAST real jamming mode hopping while focus sits on a settings card.
byte randomChannelFor(OperationMode m) {
  const byte* list;
  size_t count;
  switch (m) {
    case BLE_MODULE:          list = ble_channels;              count = sizeof(ble_channels);              break;
    case Bluetooth_MODULE:    list = bluetooth_channels;        count = sizeof(bluetooth_channels);        break;
    case USB_WIRELESS_MODULE: list = usbWireless_channels;      count = sizeof(usbWireless_channels);      break;
    case VIDEO_TX_MODULE:     list = videoTransmitter_channels; count = sizeof(videoTransmitter_channels); break;
    case RC_MODULE:           list = rc_channels;               count = sizeof(rc_channels);               break;
    case ZIGBEE_MODULE:       list = zigbee_channels;           count = sizeof(zigbee_channels);           break;
    case NRF24_MODULE:        list = nrf24_channels;            count = sizeof(nrf24_channels);            break;
    case WiFi_MODULE:
    default:                  list = WiFi_channels;             count = sizeof(WiFi_channels);             break;
  }
  return list[random(0, count / sizeof(byte))];
}

// "Digital rain" screensaver frame — bright dashes falling at different
// speeds. Pure decoration on a monochrome screen; sends no RF data.
static void matrixRain() {
  const int COLS = 42;  // 128 px wide / 3 px per column
  static int  y[COLS];
  static int  spd[COLS];
  static bool seeded = false;
  if (!seeded) {
    for (int i = 0; i < COLS; i++) { y[i] = random(-64, 64); spd[i] = random(2, 6); }
    seeded = true;
  }
  u8g2.clearBuffer();
  for (int i = 0; i < COLS; i++) {
    y[i] += spd[i];
    if (y[i] > SCREEN_H + 6) { y[i] = -random(0, 40); spd[i] = random(2, 6); }
    int head = y[i];
    u8g2.drawBox(i * 3, constrain(head - 4, 0, SCREEN_H - 4), 1, 4);  // bright dash
    if (head - 8 > 0) u8g2.drawPixel(i * 3, head - 8);                // trailing dot
  }
  u8g2.sendBuffer();
}

void update_OLED() {
  int focus = menuIndexFromMode(current_Mode);
  renderStaticMenu(focus);
}

void menuPrev() {
  int fromIdx = menuIndexFromMode(current_Mode);
  int toIdx   = (fromIdx == 0) ? (kMenuCount - 1) : (fromIdx - 1);
  current_Mode = modeFromMenuIndex(toIdx);
  // Keep the "last real jamming mode" fresh when landing on a mode card
  // (settings cards must NOT overwrite it).
  if (current_Mode != SCREENSAVER_MODULE && current_Mode != LED_MODULE) {
    lastJamMode = current_Mode;
  }
  animateToMenu(fromIdx, toIdx);
}
void menuNext() {
  int fromIdx = menuIndexFromMode(current_Mode);
  int toIdx   = (fromIdx == (kMenuCount - 1)) ? 0 : (fromIdx + 1);
  current_Mode = modeFromMenuIndex(toIdx);
  if (current_Mode != SCREENSAVER_MODULE && current_Mode != LED_MODULE) {
    lastJamMode = current_Mode;
  }
  animateToMenu(fromIdx, toIdx);
}
void menuToggleActive() {
  int focus = menuIndexFromMode(current_Mode);
  bool wasActive = (current == ACTIVE_MODE);
  current = wasActive ? DEACTIVE_MODE : ACTIVE_MODE;
  animateToggleKnobForFocus(focus, wasActive, !wasActive);
}

void checkMode() {
  // NAVIGATION (test-driven fix 2026-11-19): LEFT button (GPIO27) goes
  // BACK, RIGHT button (GPIO25) ADVANCES — verified on-device by the
  // test_menu_logic sketch (11/11 PASS). An earlier "fix" this same day
  // had this backwards: it misread a complaint about the slide ANIMATION
  // direction as a complaint about selection direction. The rules now
  // live in menu_logic.h, unit-tested against the hardware.
  if (ChangeRequested) {
    ChangeRequested = false;
    current_Mode = modeFromMenuIndex(menu::prevIndex(menuIndexFromMode(current_Mode)));
  } else if (ChangeRequested1) {
    ChangeRequested1 = false;
    current_Mode = modeFromMenuIndex(menu::nextIndex(menuIndexFromMode(current_Mode)));
  }
  // Remember the last REAL jamming mode, so the Screensaver and LED settings
  // cards can keep it running while focus sits on them.
  if (current_Mode != SCREENSAVER_MODULE && current_Mode != LED_MODULE) {
    lastJamMode = current_Mode;
  }
}

void setup() {
  Serial.begin(115200);
  // BATTERY FIX (2026-11-19): setup() used to fire ALL THREE radios into
  // max-power carriers as its very first act (initialize_MultiMode), then
  // power them down again if passive. On battery that boot spike collapses
  // the rail -> brownout reset loop (GitHub issue #31). Quiet start instead.
  radioQuietStart();
  // LED FIX (2026-11-19): force the WS2812 DARK immediately. It used to be
  // left uninitialized, latched random garbage, and sat blinding-white while
  // draining ~60mA the whole time. Also loads the saved LED preference.
  neopixelSetup();
  // FREEZE FIX (2026-11-19), part 1: the OLED bus runs at the I2C standard's
  // full 100 kHz instead of 400 kHz. At 400 kHz the data windows are 4x
  // shorter, so the rail dip that happens when the radios arm can glitch a
  // transaction; the Wire library in ESP32 core 1.0.5 has NO timeout, and a
  // glitched transaction used to hang the firmware forever (frozen screen,
  // dead buttons, and the radios possibly still transmitting — GitHub #31).
  Wire.begin();
  Wire.setClock(100000);
  u8g2.begin();
  u8g2.setBusClock(100000);

  // FREEZE FIX (2026-11-19), part 2: a 10-second watchdog on the main loop.
  // If anything hangs the loop (I2C wait, SPI stall), the chip resets itself
  // and comes back to a SAFE PASSIVE MENU instead of freezing forever —
  // crucially, this also stops a "jammed silently while the screen says
  // PASSIVE" state (the owner caught the device doing exactly that).
  esp_task_wdt_init(10, true);
  esp_task_wdt_add(NULL);   // watch the loop task
  u8g2.setFont(FONT_SMALL);
  u8g2.setDrawColor(1);
  u8g2.setFontPosTop();
  esp_bt_controller_deinit();
  esp_wifi_stop();
  esp_wifi_deinit();
  esp_wifi_disconnect();
  pinMode(PIN_BTN_L,  INPUT_PULLUP);
  pinMode(PIN_BTN_R, INPUT_PULLUP);
  pinMode(PIN_BTN_S, INPUT_PULLUP);
  attachInterrupt(digitalPinToInterrupt(PIN_BTN_L),  handleButton,  FALLING);
  attachInterrupt(digitalPinToInterrupt(PIN_BTN_R), handleButton1, FALLING);
  attachInterrupt(digitalPinToInterrupt(PIN_BTN_S), handleButton2, FALLING);
  initialize_Radios();
  conf();
  update_OLED();
}

void loop() {
  esp_task_wdt_reset();   // "still alive" pat for the 10 s watchdog
  checkMode();

  // ---- Settings-card actions (flags come from the center-button ISR) ----
  if (wakeRequested) {            // any press while screensaver runs: wake
    wakeRequested = false;
    screensaverActive = false;
    lastFocus    = current_Mode;  // re-sync so no phantom slide plays on wake
    lastActivity = current;
    update_OLED();
  }
  if (toggleLedRequested) {       // LED card: flip preference + persist it
    toggleLedRequested = false;
    ledEnabled = !ledEnabled;
    ledSavePreference();
    ledApply();
    update_OLED();
  }
  if (enterScreensaver) {         // Screensaver card: start the animation
    enterScreensaver = false;
    screensaverActive = true;
    animPhase      = false;       // open with the matrix rain
    lastAnimSwitch = millis();
  }

  if (screensaverActive) {
    // Full-screen animation, alternating rain <-> bars every 5 seconds.
    // Jamming (if armed) keeps hopping below via lastJamMode.
    if (millis() - lastAnimSwitch > 5000) {
      animPhase = !animPhase;
      lastAnimSwitch = millis();
    }
    if (animPhase) spectrum(); else matrixRain();
    delay(30);                    // ~30 fps is plenty for this OLED
  } else {
    if (current_Mode != lastFocus) {
      int fromIdx = menuIndexFromMode(lastFocus);
      int toIdx   = menuIndexFromMode(current_Mode);
      animateToMenu(fromIdx, toIdx);
      lastFocus = current_Mode;
      return;
    }
    if (current != lastActivity) {
      // FROZEN TARGET (test-driven cycle 2, 2026-11-19): capture WHICH mode
      // is being armed, at the moment of arming. From here on the radios
      // hop this mode's channels no matter where the user browses —
      // navigating a live transmitter used to silently retarget it.
      if (current == ACTIVE_MODE) armedMode = current_Mode;
      // ORDER FIX (2026-11-19): draw the toggle animation BEFORE arming the
      // radios. The interrupt flips the armed flag instantly, and arming used
      // to run before any drawing — so when the battery rail dipped and the
      // screen froze mid-toggle, the device could sit there TRANSMITTING at
      // full power while the screen still showed PASSIVE. Now the UI confirms
      // first; if arming then hangs or browns out, the watchdog resets the
      // board to a safe PASSIVE menu.
      int  focus     = menuIndexFromMode(current_Mode);
      bool wasActive = (lastActivity == ACTIVE_MODE);
      bool nowActive = (current      == ACTIVE_MODE);
      animateToggleKnobForFocus(focus, wasActive, nowActive);
      initialize_Radios();
      lastActivity = current;
      return;
    }
  }

  // ---- Status LED (2026-11-19): green while jamming, blue while passive,
  // fully dark if the user turned the LED card off. Re-sent only on change. ----
  static bool ledShown    = false;
  static bool lastLedOn   = false;
  static bool lastLedJam  = false;
  if (!ledShown || lastLedOn != ledEnabled || lastLedJam != (current == ACTIVE_MODE)) {
    ledApply();
    lastLedOn  = ledEnabled;
    lastLedJam = (current == ACTIVE_MODE);
    ledShown   = true;
  }

  // ---- Channel hopping (radios only emit if jamming is armed) ----
  // FROZEN TARGET: while armed, hop the mode captured at arm time — never
  // the card the user happens to be browsing. On settings cards while
  // passive, hop the last real mode (harmless; radios are powered down).
  OperationMode hopMode;
  if (current == ACTIVE_MODE) {
    hopMode = armedMode;
  } else if (current_Mode == SCREENSAVER_MODULE || current_Mode == LED_MODULE) {
    hopMode = lastJamMode;
  } else {
    hopMode = current_Mode;
  }
  byte channel = randomChannelFor(hopMode);
  RadioA.setChannel(channel);
  RadioB.setChannel(channel);
  RadioC.setChannel(channel);
}
