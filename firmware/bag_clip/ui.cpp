#include "ui.h"
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include "config.h"
#include "battery.h"
#include "settings.h"
#include "panda_anim.h"
#include "greeting_img.h"
#include "ble_link.h"
#if WIFI_MODE
#include <WiFi.h>
#endif

// 400 kHz I2C during and after a frame. A full frame then takes ~23 ms.
static Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, -1, 400000UL, 400000UL);
static unsigned long lastFrame = 0;
static uint32_t shownTrackVersion = 0;
static unsigned long scrollStart = 0;

static const unsigned long SCROLL_PAUSE_MS = 1500;   // hold the start of a long title this long
static const int SCROLL_GAP_PX = 30;                 // gap before the title repeats
static const uint8_t SCROLL_PX_PER_S[] = {0, 12, 20, 30, 42, 60};

// ---------- 7x7 icons ----------
static const uint8_t ICON_PLAY[]  PROGMEM = {0x40, 0x60, 0x70, 0x78, 0x70, 0x60, 0x40};
static const uint8_t ICON_PAUSE[] PROGMEM = {0x6C, 0x6C, 0x6C, 0x6C, 0x6C, 0x6C, 0x6C};
static const uint8_t ICON_BT[]    PROGMEM = {0x30, 0x28, 0xA8, 0x70, 0xA8, 0x28, 0x30};
static const uint8_t ICON_NOTE[]  PROGMEM = {0x1E, 0x12, 0x12, 0x12, 0x76, 0xF6, 0x60};

bool uiBegin() {
  Wire.begin(OLED_SDA, OLED_SCL);
  if (!display.begin(SSD1306_SWITCHCAPVCC, OLED_ADDR)) {
    Serial.println("SSD1306 not found: check wiring / address (0x3C or 0x3D)");
    return false;
  }
  display.setTextColor(SSD1306_WHITE);
  display.setTextWrap(false);
  display.cp437(true);
  uiSetContrast(settings.brightness);
  return true;
}

void uiSetContrast(uint8_t contrast) {
  static int current = -1;
  if (current == contrast) return;
  current = contrast;
  display.ssd1306_command(SSD1306_SETCONTRAST);
  display.ssd1306_command(contrast);
}

void uiOff() { display.ssd1306_command(SSD1306_DISPLAYOFF); }

static void centered(const char *s, int y, int size = 1) {
  display.setTextSize(size);
  int w = strlen(s) * 6 * size;
  display.setCursor(max(0, (SCREEN_WIDTH - w) / 2), y);
  display.print(s);
  display.setTextSize(1);
}

void uiMessage(const char *l1, const char *l2, const char *l3) {
  display.clearDisplay();
  int lines = 1 + (l2 ? 1 : 0) + (l3 ? 1 : 0);
  int y = (SCREEN_HEIGHT - lines * 12) / 2;
  centered(l1, y);
  if (l2) centered(l2, y + 12);
  if (l3) centered(l3, y + 24);
  display.display();
}

// Power-on greeting: 0 = "Hpy bday Kusuma." (panda in its cap), 1 = "From your lovely panda".
// The pictures and the animation come from tools/make_oled_panda.py (greeting_img.h):
// change the text or the animation there, not here.
void uiGreeting(uint8_t stage) {
  display.clearDisplay();
  display.drawBitmap(0, 0, stage == 0 ? GREET_BDAY : GREET_FROM, GREET_BDAY_W, GREET_BDAY_H, SSD1306_WHITE);
  display.display();
}

// Between the two screens: the panda's paw takes the cap off (about 1.2 s). Only the
// panda's square is redrawn; screen 1's text stays until screen 2 replaces it.
void uiGreetingHatOff() {
  for (uint8_t i = 0; i < GREET_ANIM_COUNT; i++) {
    display.fillRect(0, 0, GREET_ANIM_W, GREET_ANIM_H, SSD1306_BLACK);
    display.drawBitmap(0, 0, GREET_ANIM_DATA[i], GREET_ANIM_W, GREET_ANIM_H, SSD1306_WHITE);
    display.display();
    delay(pgm_read_word(&GREET_ANIM_MS[i]));
  }
}

// ---------- small pieces ----------
static void formatTime(long ms, char *out, size_t len) {
  long s = max(0L, ms / 1000);
  if (s >= 3600) snprintf(out, len, "%ld:%02ld:%02ld", s / 3600, (s / 60) % 60, s % 60);
  else snprintf(out, len, "%ld:%02ld", s / 60, s % 60);
}

static void drawBattery(int xRight, int y) {
  int pct = batteryPercent();
  if (pct < 0) return;                         // no divider fitted (USB bench test)
  int x = xRight - 13;
  display.drawRect(x, y, 12, 7, SSD1306_WHITE);
  display.fillRect(x + 12, y + 2, 1, 3, SSD1306_WHITE);
  char txt[6];
  if (batteryOnUsb()) {
    // lightning bolt instead of a level
    display.drawLine(x + 7, y + 1, x + 4, y + 3, SSD1306_WHITE);
    display.drawLine(x + 4, y + 3, x + 7, y + 3, SSD1306_WHITE);
    display.drawLine(x + 7, y + 3, x + 4, y + 5, SSD1306_WHITE);
    strcpy(txt, "USB");
  } else {
    int fill = (pct * 8 + 50) / 100;
    if (fill) display.fillRect(x + 2, y + 2, fill, 3, SSD1306_WHITE);
    snprintf(txt, sizeof(txt), "%d%%", pct);
  }
  display.setCursor(x - 2 - strlen(txt) * 6, y);
  display.print(txt);
}

#if WIFI_MODE
static void drawWifi(int x, int y, bool connected) {
  int rssi = connected ? WiFi.RSSI() : -100;
  int bars = rssi > -60 ? 3 : rssi > -70 ? 2 : rssi > -82 ? 1 : 0;
  for (int i = 0; i < 3; i++) {
    int h = 2 + i * 2;
    if (i < bars) display.fillRect(x + i * 3, y + 7 - h, 2, h, SSD1306_WHITE);
    else display.drawPixel(x + i * 3, y + 6, SSD1306_WHITE);
  }
  if (!connected) {                            // small x
    display.drawLine(x + 6, y, x + 8, y + 2, SSD1306_WHITE);
    display.drawLine(x + 8, y, x + 6, y + 2, SSD1306_WHITE);
  }
}
#endif

// Bluetooth rune; a small x next to it while the phone isn't connected.
static void drawBt(int x, int y, bool connected) {
  display.drawBitmap(x, y, ICON_BT, 7, 7, SSD1306_WHITE);
  if (!connected) {
    display.drawLine(x + 7, y, x + 9, y + 2, SSD1306_WHITE);
    display.drawLine(x + 9, y, x + 7, y + 2, SSD1306_WHITE);
  }
}

static void drawPhoneBattery(int x, int y, int pct) {   // tiny phone outline + %
  if (pct < 0) return;
  display.drawRect(x, y, 5, 7, SSD1306_WHITE);
  display.drawPixel(x + 2, y + 5, SSD1306_WHITE);
  display.setCursor(x + 7, y);
  display.print(pct);
  display.print('%');
}

static void drawStatusBar(const AppState &st) {
  display.drawBitmap(0, 0, st.playing ? ICON_PLAY : ICON_PAUSE, 7, 7, SSD1306_WHITE);
  drawBt(11, 0, st.bleConnected);
#if WIFI_MODE
  drawWifi(24, 0, st.wifi == NET_CONNECTED);
#endif
  drawBattery(SCREEN_WIDTH, 0);
}

// Draws text from x0 to the right edge; scrolls it (time-based, so speed stays even
// however long a frame takes) when it doesn't fit.
static void drawMarquee(const char *text, int x0, int y, int speedPx) {
  int w = strlen(text) * 6;
  int area = SCREEN_WIDTH - x0;
  if (w <= area || speedPx <= 0) {
    display.setCursor(x0, y);
    display.print(text);
    return;
  }
  int loopPx = w + SCROLL_GAP_PX;
  unsigned long scrollMs = (unsigned long)loopPx * 1000UL / speedPx;
  unsigned long t = (millis() - scrollStart) % (SCROLL_PAUSE_MS + scrollMs);
  int off = t < SCROLL_PAUSE_MS ? 0 : (int)((t - SCROLL_PAUSE_MS) * speedPx / 1000UL);
  display.setCursor(x0 - off, y);
  display.print(text);
  display.setCursor(x0 - off + loopPx, y);   // second copy for a seamless loop
  display.print(text);
}

// ---------- screens ----------
static void drawNowPlaying(const AppState &st) {
  if (st.trackVersion != shownTrackVersion) {
    shownTrackVersion = st.trackVersion;
    scrollStart = millis();
  }
  int speed = SCROLL_PX_PER_S[constrain(settings.scrollSpeed, 1, 5)];
  bool art = settings.showArt && st.artValid;
  int x0 = art ? ART_SIZE + 4 : 0;

  drawMarquee(st.title, x0, 11, speed);
  drawMarquee(st.artist, x0, 22, speed);
  if (st.nextTitle[0]) {
    char next[104];
    snprintf(next, sizeof(next), "\x10 %s", st.nextTitle);   // cp437 right-pointing triangle
    drawMarquee(next, x0, 33, speed);
  }
  // Mask the scrolled text where the thumbnail goes, then draw the thumbnail.
  if (art) {
    display.fillRect(0, 9, x0, 34, SSD1306_BLACK);
    display.drawBitmap(0, 10, st.art, ART_SIZE, ART_SIZE, SSD1306_WHITE);
  }
  drawStatusBar(st);   // drawn last so long text never covers it

  // Progress: interpolated between polls so the bar and clock tick every second.
  // A length of 0 means the app doesn't know it (live streams, some podcasts): the
  // clock still counts up, the total shows "--:--" and the bar stays empty.
  bool knownLength = st.durationMs > 1;
  long shown = st.progressMs;
  if (st.playing) shown += (long)(millis() - st.progressStamp);
  shown = knownLength ? constrain(shown, 0L, st.durationMs) : max(0L, shown);
  char el[10], tot[10];
  formatTime(shown, el, sizeof(el));
  if (knownLength) formatTime(st.durationMs, tot, sizeof(tot));
  else strlcpy(tot, "--:--", sizeof(tot));
  display.fillRect(0, 45, SCREEN_WIDTH, 19, SSD1306_BLACK);
  display.setCursor(0, 46);
  display.print(el);
  display.setCursor(SCREEN_WIDTH - strlen(tot) * 6, 46);
  display.print(tot);
  int barW = knownLength ? (int)((int64_t)shown * (SCREEN_WIDTH - 4) / st.durationMs) : 0;
  display.drawRoundRect(0, 56, SCREEN_WIDTH, 8, 3, SSD1306_WHITE);
  display.fillRect(2, 58, barW, 4, SSD1306_WHITE);
}

// ---------- idle animation ----------
// The pictures and their timing live in flash (panda_anim.h, made by tools/gif_to_frames.py);
// the only RAM used is the step number and when it started.
static uint8_t pandaStep = 0;
static unsigned long pandaStepAt = 0;
static bool pandaResting = false;
static bool pandaDirty = false;            // picture switched outside pandaAdvance(): redraw now

void uiPandaRest(bool rest) {
  if (rest == pandaResting) return;
  pandaResting = rest;
  pandaStep = rest ? PANDA_ANIM_REST : 0;   // dozing picture while dim; start over when woken
  pandaStepAt = millis();
  pandaDirty = true;
}

// Moves to the next step when its time is up. True when the picture changed.
static bool pandaAdvance() {
  if (pandaResting || PANDA_ANIM_STEPS < 2) return false;
  unsigned long now = millis();
  uint16_t ms = pgm_read_word(&PANDA_ANIM_SEQ[pandaStep].ms);
  if (now - pandaStepAt < ms) return false;
  // Catch up if a long frame (or the info screen) kept us away, without replaying every step.
  pandaStepAt = (now - pandaStepAt > 2000UL) ? now : pandaStepAt + ms;
  uint8_t before = pgm_read_byte(&PANDA_ANIM_SEQ[pandaStep].frame);
  pandaStep = (pandaStep + 1) % PANDA_ANIM_STEPS;
  return pgm_read_byte(&PANDA_ANIM_SEQ[pandaStep].frame) != before;
}

static void drawPanda(const AppState &st) {
  uint8_t frame = pgm_read_byte(&PANDA_ANIM_SEQ[pandaStep].frame);
  display.drawBitmap(0, (SCREEN_HEIGHT - PANDA_ANIM_H) / 2,
                     PANDA_ANIM_FRAMES_DATA[frame], PANDA_ANIM_W, PANDA_ANIM_H, SSD1306_WHITE);
  int x = PANDA_ANIM_W + 4;   // text to the right of the animation
  if (!st.bleConnected && !st.spotifyDirect) {
    display.setCursor(x, 6);  display.print("Phone not");
    display.setCursor(x, 16); display.print("connected");
    display.setCursor(x, 28); display.print("zzz");
  } else if (!st.appLinked && !st.spotifyDirect) {
    display.setCursor(x, 6);  display.print("Open the");
    display.setCursor(x, 16); display.print("Bag Clip");
    display.setCursor(x, 26); display.print("app");
  } else if (st.hasTrack && !st.playing) {
    display.setCursor(x, 6);  display.print("Paused");
    display.setCursor(x, 18); display.print("tap to");
    display.setCursor(x, 28); display.print("play");
  } else {
    display.setCursor(x, 6);  display.print("Nothing");
    display.setCursor(x, 16); display.print("playing");
    display.drawBitmap(x + 46, 26, ICON_NOTE, 7, 7, SSD1306_WHITE);
    display.setCursor(x, 28); display.print("zzz");
  }
  drawBt(x, 44, st.bleConnected);
#if WIFI_MODE
  drawWifi(x + 13, 44, st.wifi == NET_CONNECTED);
#endif
  drawBattery(SCREEN_WIDTH, 55);
}

static void drawInfo(const AppState &st) {
  char line[32];
  display.setCursor(0, 0);
  display.print("Bag Clip v" FW_VERSION);
  display.setCursor(0, 12);
  display.print(st.appLinked ? "Phone: app linked" : st.bleConnected ? "Phone: connected" : "Phone: not connected");
  display.setCursor(0, 22);
  if (st.sourceApp[0]) { display.print("From "); display.print(st.sourceApp); }
  drawPhoneBattery(SCREEN_WIDTH - 36, 22, st.phoneBattery);
  display.setCursor(0, 34);
#if WIFI_MODE
  if (st.wifi == NET_CONNECTED) display.print(st.ip);
  else display.print("Wi-Fi not connected");
#else
  display.print("BT: " DEVICE_NAME);
#endif
  display.setCursor(0, 46);
  if (batteryPresent()) snprintf(line, sizeof(line), "Batt %d.%02d V %s", batteryMv() / 1000, (batteryMv() % 1000) / 10,
                                 batteryOnUsb() ? "USB" : "");
  else snprintf(line, sizeof(line), BATT_FITTED ? "Batt: switched off" : "Batt: off (config)");
  display.print(line);
#if WIFI_MODE
  display.setCursor(0, 56);
  snprintf(line, sizeof(line), "Spotify %d", st.lastHttpStatus);
  display.print(line);
#endif
}

void uiRender(Screen screen, const AppState &st, bool force) {
  // Moving screens get ~40 fps, static ones 4 fps to save power. The idle panda is only
  // redrawn when its picture changes (a few times a second at most) or once a second for
  // the battery / status text; while dimmed it stays on one picture.
  bool due;
  if (screen == SCR_PANDA) {
    bool newPicture = pandaAdvance() || pandaDirty;
    pandaDirty = false;
    bool hasToast = st.toast[0] && (long)(millis() - st.toastUntil) < 300;   // show and clear it promptly
    unsigned long refresh = hasToast ? 250UL : pandaResting ? 5000UL : 1000UL;
    due = newPicture || millis() - lastFrame >= refresh;
  } else {
    unsigned long interval = (screen == SCR_NOW_PLAYING || screen == SCR_OTA) ? FRAME_MS : 250;
    due = millis() - lastFrame >= interval;
  }
  if (!force && !due) return;
  lastFrame = millis();

  display.clearDisplay();
  display.setTextSize(1);
  char buf[24];
  switch (screen) {
    case SCR_NOW_PLAYING: drawNowPlaying(st); break;
    case SCR_PANDA:       drawPanda(st); break;
    case SCR_SETUP:
      display.drawBitmap(0, 0, ICON_BT, 7, 7, SSD1306_WHITE);
      centered("Set me up!", 4);
      if (st.wifi == NET_NEED_SETUP && st.spotifyDirect) {   // Wi-Fi mode, no network saved
        centered("Add Wi-Fi from the", 22);
        centered("phone page over", 32);
        centered("Bluetooth:", 42);
      } else {
        centered("Open the Bag Clip", 22);
        centered("app, tap Find clip,", 32);
        centered("then type the PIN:", 42);
      }
      centered(DEVICE_NAME, 54);
      break;
    case SCR_CONNECTING:
      centered("Connecting to", 14);
      centered(st.ssid[0] ? st.ssid : "Wi-Fi...", 26);
      drawBattery(SCREEN_WIDTH, 56);
      break;
    case SCR_NO_WIFI:
      centered("No saved Wi-Fi", 8);
      centered("in range.", 18);
      centered("Retrying. Add one", 36);
      centered("over Bluetooth.", 46);
      break;
    case SCR_SPOTIFY_ERROR:
      if (st.spotify == SPOT_AUTH_FAILED) {
        centered("Spotify login failed", 14);
        centered("Check secrets.h", 28);
      } else {
        centered("Can't reach Spotify", 14);
        snprintf(buf, sizeof(buf), "error %d, retrying", st.lastHttpStatus);
        centered(buf, 28);
      }
      drawStatusBar(st);
      break;
    case SCR_PASSKEY:
      centered("Bluetooth PIN", 6);
      snprintf(buf, sizeof(buf), "%06lu", (unsigned long)st.blePasskey);
      centered(buf, 24, 2);
      centered("type it on your phone", 50);
      break;
    case SCR_INFO: drawInfo(st); break;
    case SCR_OTA:
      centered("Updating firmware", 14);
      display.drawRect(4, 32, SCREEN_WIDTH - 8, 10, SSD1306_WHITE);
      display.fillRect(6, 34, (SCREEN_WIDTH - 12) * st.otaPercent / 100, 6, SSD1306_WHITE);
      centered("Don't switch off", 50);
      break;
  }
  if (st.toast[0] && (long)(millis() - st.toastUntil) < 0) {
    display.fillRoundRect(4, 22, SCREEN_WIDTH - 8, 20, 4, SSD1306_BLACK);
    display.drawRoundRect(4, 22, SCREEN_WIDTH - 8, 20, 4, SSD1306_WHITE);
    centered(st.toast, 28);
  }
  display.display();
}

// ---------- menu screens (menu.cpp says when to draw them) ----------
// Every menu screen has the same title bar: white, the name on the left, a hint or a
// count on the right in black.
static void menuHeader(const char *left, const char *right) {
  display.fillRect(0, 0, SCREEN_WIDTH, 11, SSD1306_WHITE);
  display.setTextColor(SSD1306_BLACK);
  display.setCursor(3, 2);
  display.print(left);
  if (right) {
    display.setCursor(SCREEN_WIDTH - 3 - strlen(right) * 6, 2);
    display.print(right);
  }
  display.setTextColor(SSD1306_WHITE);
}

static void drawToast(const AppState &st) {
  if (st.toast[0] && (long)(millis() - st.toastUntil) < 0) {
    display.fillRoundRect(4, 22, SCREEN_WIDTH - 8, 20, 4, SSD1306_BLACK);
    display.drawRoundRect(4, 22, SCREEN_WIDTH - 8, 20, 4, SSD1306_WHITE);
    centered(st.toast, 28);
  }
}

void uiMenuList(const char *const *labels, int count, int selected) {
  display.clearDisplay();
  menuHeader("Menu", "hold = OK");
  const int rows = 4, rowH = 13, top = 13;
  int first = constrain(selected - 1, 0, max(0, count - rows));
  for (int r = 0; r < rows && first + r < count; r++) {
    int i = first + r, y = top + r * rowH;
    if (i == selected) {
      display.fillRoundRect(0, y, SCREEN_WIDTH - 6, rowH - 1, 3, SSD1306_WHITE);
      display.setTextColor(SSD1306_BLACK);
    }
    display.setCursor(5, y + 2);
    display.print(labels[i]);
    display.setTextColor(SSD1306_WHITE);
  }
  // scroll bar: where the four rows sit in the whole list
  int trackH = rows * rowH - 1, thumbH = max(6, trackH * rows / count);
  int thumbY = top + (trackH - thumbH) * first / max(1, count - rows);
  for (int y = top; y < top + trackH; y += 2) display.drawPixel(SCREEN_WIDTH - 2, y, SSD1306_WHITE);
  display.fillRect(SCREEN_WIDTH - 3, thumbY, 3, thumbH, SSD1306_WHITE);
  display.display();
}

void uiMusic(const AppState &st) {
  display.clearDisplay();
  menuHeader("Music", "hold = back");
  if (!st.hasTrack) {
    centered("Nothing playing", 20);
    centered("Play a song on", 34);
    centered("your phone", 44);
  } else {
    if (st.trackVersion != shownTrackVersion) { shownTrackVersion = st.trackVersion; scrollStart = millis(); }
    int speed = SCROLL_PX_PER_S[constrain(settings.scrollSpeed, 1, 5)];
    drawMarquee(st.title, 0, 14, speed);
    drawMarquee(st.artist, 0, 24, speed);
    // previous | play or pause | next, the middle one larger
    display.fillRect(22, 36, 2, 9, SSD1306_WHITE);
    display.fillTriangle(33, 36, 33, 44, 25, 40, SSD1306_WHITE);
    if (st.playing) {
      display.fillRect(58, 35, 4, 11, SSD1306_WHITE);
      display.fillRect(66, 35, 4, 11, SSD1306_WHITE);
    } else {
      display.fillTriangle(59, 34, 59, 46, 70, 40, SSD1306_WHITE);
    }
    display.fillTriangle(95, 36, 95, 44, 103, 40, SSD1306_WHITE);
    display.fillRect(104, 36, 2, 9, SSD1306_WHITE);
    // progress as a thin line
    bool knownLength = st.durationMs > 1;
    long shown = st.progressMs + (st.playing ? (long)(millis() - st.progressStamp) : 0);
    if (knownLength) {
      int w = (int)((int64_t)constrain(shown, 0L, st.durationMs) * SCREEN_WIDTH / st.durationMs);
      display.drawFastHLine(0, 50, SCREEN_WIDTH, SSD1306_WHITE);
      display.fillRect(0, 49, w, 3, SSD1306_WHITE);
    }
  }
  display.setCursor(4, 56);
  display.print("1:play 2:next 3:prev");
  drawToast(st);
  display.display();
}

// The dino, 16x16, two leg pictures for running.
static const char *const DINO_ROWS[16] = {
  "........#######.", ".......##.######", ".......#########", ".......#########",
  ".......#####....", ".......#######..", "#.....######....", "#....#######....",
  "##..##########..", "###########.#...", ".#########......", "..########......",
  "...######.......", "....##.##.......", "....#...#.......", "....##..##......",
};
static const char *const DINO_LEGS_B[3] = {"....##.##.......", "....##..#.......", ".......###......"};

static void drawDino(int x, int y, bool legsB) {
  for (int r = 0; r < 16; r++) {
    const char *row = (legsB && r >= 13) ? DINO_LEGS_B[r - 13] : DINO_ROWS[r];
    for (int c = 0; c < 16; c++)
      if (row[c] == '#') display.drawPixel(x + c, y + r, SSD1306_WHITE);
  }
}

void uiDino(const DinoGame &g) {
  const int ground = 60;
  display.clearDisplay();
  int run = (int)g.distance;
  for (int x = 0; x < SCREEN_WIDTH; x++)   // the ground, with small stones moving past
    if (((x + run) % 9) < 7) display.drawPixel(x, ground, SSD1306_WHITE);
  for (int i = 0; i < 4; i++) display.drawPixel((SCREEN_WIDTH - ((run / 2 + i * 37) % SCREEN_WIDTH)), ground + 2, SSD1306_WHITE);
  for (const auto &c : g.cacti) {
    if (!c.used) continue;
    int x = (int)c.x, trunk = max(3, (int)c.w - 4), tx = x + (c.w - trunk) / 2;
    display.fillRect(tx, ground - c.h, trunk, c.h, SSD1306_WHITE);
    display.fillRect(x, ground - c.h + 3, 2, 5, SSD1306_WHITE);            // left arm
    display.fillRect(x, ground - c.h + 7, tx - x, 2, SSD1306_WHITE);
    display.fillRect(x + c.w - 2, ground - c.h + 5, 2, 4, SSD1306_WHITE);  // right arm
    display.fillRect(tx + trunk, ground - c.h + 8, x + c.w - tx - trunk, 2, SSD1306_WHITE);
  }
  bool onGround = g.y < 0.5f;
  drawDino(8, ground - 16 - (int)g.y, onGround && g.phase == DinoGame::RUNNING && (run / 6) % 2);
  char buf[24];
  snprintf(buf, sizeof(buf), "HI %05lu %05lu", (unsigned long)dinoHighScore, (unsigned long)g.score());
  display.setCursor(SCREEN_WIDTH - strlen(buf) * 6, 0);
  display.print(buf);
  if (g.phase == DinoGame::READY) {
    centered("Tap to jump", 18);
    centered("hold = back", 30);
  } else if (g.phase == DinoGame::OVER) {
    display.fillRoundRect(14, 14, SCREEN_WIDTH - 28, 26, 4, SSD1306_BLACK);
    display.drawRoundRect(14, 14, SCREEN_WIDTH - 28, 26, 4, SSD1306_WHITE);
    centered("Game over", 18);
    centered("tap: again", 29);
  }
  display.display();
}

// Breaks text into lines of at most 21 characters at spaces; returns how many lines.
static int wrapLines(const char *text, char lines[][22], int maxLines) {
  int n = 0;
  const char *p = text;
  while (*p && n < maxLines) {
    while (*p == ' ') p++;
    int len = strlen(p);
    int take = len <= 21 ? len : 21;
    if (len > 21) {
      int cut = take;
      while (cut > 0 && p[cut] != ' ') cut--;
      if (cut > 0) take = cut;
    }
    memcpy(lines[n], p, take);
    lines[n][take] = 0;
    n++;
    p += take;
  }
  return n;
}

void uiMotivation(const char *text, int number, int count) {
  display.clearDisplay();
  char right[8];
  snprintf(right, sizeof(right), "%d/%d", number, count);
  menuHeader("Motivation", right);
  char lines[4][22];
  int n = wrapLines(text, lines, 4);
  int y = 14 + (40 - n * 10) / 2;
  for (int i = 0; i < n; i++) centered(lines[i], y + i * 10);
  display.setCursor(0, 56);
  display.print("tap: next hold: back");
  display.display();
}

void uiBrightness(const char *const *names, int count, int selected) {
  display.clearDisplay();
  menuHeader("Brightness", nullptr);
  int w = 38, gap = 4, x0 = (SCREEN_WIDTH - count * w - (count - 1) * gap) / 2;
  for (int i = 0; i < count; i++) {
    int x = x0 + i * (w + gap);
    if (i == selected) {
      display.fillRoundRect(x, 20, w, 18, 3, SSD1306_WHITE);
      display.setTextColor(SSD1306_BLACK);
    } else {
      display.drawRoundRect(x, 20, w, 18, 3, SSD1306_WHITE);
    }
    display.setCursor(x + (w - strlen(names[i]) * 6) / 2, 25);
    display.print(names[i]);
    display.setTextColor(SSD1306_WHITE);
  }
  display.setCursor(1, 52);
  display.print("tap change  hold save");
  display.display();
}
