/*
 * ESP32-C3 "Now Playing" Bag Clip — firmware v3 (Bluetooth only)
 *
 * The clip talks to your phone over Bluetooth LE only. The Bag Clip Android app
 * (phone-app/) reads what's playing on the phone (Spotify or any music app) and sends
 * title, artist, next up, progress and album art; taps on the clip go back to the app,
 * which plays/pauses/skips on the phone. No Wi-Fi, no hotspot, no Spotify keys.
 * Optional Wi-Fi mode (WIFI_MODE 1 in config.h) adds OTA updates and a web page.
 * See PROJECT.md for wiring, libraries and the app.
 *
 * Board:     ESP32C3 Dev Module, USB CDC On Boot: Enabled,
 *            Partition Scheme: Minimal SPIFFS (1.9MB APP with OTA/190KB SPIFFS)
 * Libraries: Adafruit SSD1306, Adafruit GFX Library, NimBLE-Arduino 2.x
 *            (Wi-Fi mode also: ArduinoJson 7.x, TJpg_Decoder)
 *
 * Touch pad (TTP223 on GPIO3):
 *   1 tap -> play / pause    2 taps -> next    3 taps -> previous
 *   hold 1.5 s -> info screen (battery, phone, version)
 *   tap while asleep -> wake
 */

#include "esp_sleep.h"
#include "config.h"
#include "settings.h"
#include "state.h"
#include "battery.h"
#include "ui.h"
#include "ble_link.h"
#include "wifi_link.h"

static unsigned long lastActivity = 0, idleScreenSince = 0, lastPlayingAt = 0, infoUntil = 0;
static unsigned long pressStart = 0, lastTapTime = 0;
static int tapCount = 0;
static bool lastTouch = false, longFired = false, ignoreUntilRelease = false;
static Screen lastScreen = SCR_PANDA;

static void markActivity() {
  lastActivity = millis();
  idleScreenSince = millis();   // also undims the screen
}

// Deep sleep until the pad is tapped. If the pad is still pressed (a finger, or the bag
// squashing it), a "wake when pressed" would fire at once and loop, so instead the clip
// checks again every STUCK_PAD_RECHECK_S seconds until the pad is free.
static void sleepNow() {
  esp_sleep_disable_wakeup_source(ESP_SLEEP_WAKEUP_ALL);
  if (digitalRead(TOUCH_PIN) == HIGH) {
    esp_sleep_enable_timer_wakeup(STUCK_PAD_RECHECK_S * 1000000ULL);
  } else {
    esp_deep_sleep_enable_gpio_wakeup(1ULL << TOUCH_PIN, ESP_GPIO_WAKEUP_GPIO_HIGH);
  }
  esp_deep_sleep_start();
}

static void enterDeepSleep(const char *line1, const char *line2) {
  uiSetContrast(settings.brightness);
  uiMessage(line1, line2);
  delay(strcmp(line1, "Battery low") == 0 ? 4000 : 1200);
  uiOff();

  // Give a finger on the pad a moment to lift.
  unsigned long t0 = millis();
  while (digitalRead(TOUCH_PIN) == HIGH && millis() - t0 < 3000) delay(10);

  wifiLinkOff();
  sleepNow();
}

// A play control from the touch pad, the phone page or the web page.
static void dispatchCommand(Command c) {
  AppState st = stateGet();
  bool viaPhone = bleSendToPhone(c);
#if WIFI_MODE
  bool viaSpotify = !viaPhone && st.spotifyDirect && st.wifi == NET_CONNECTED;
  if (viaSpotify) sendNetCommand(c == CMD_TOGGLE ? (st.playing ? CMD_PAUSE : CMD_PLAY) : c);
#else
  const bool viaSpotify = false;
#endif
  if (!viaPhone && !viaSpotify) {
    stateToast(st.bleConnected ? "Open the Bag Clip app" : "Phone not connected");
    return;
  }
  if (c == CMD_TOGGLE || c == CMD_PLAY || c == CMD_PAUSE) {
    // Flip the icon straight away; the phone (or Spotify) confirms it a moment later.
    bool wantPlay = (c == CMD_PLAY) || (c == CMD_TOGGLE && !st.playing);
    stateLock();
    AppState &s = stateRef();
    if (s.playing) s.progressMs += (long)(millis() - s.progressStamp);
    s.progressStamp = millis();
    s.playing = wantPlay;
    stateUnlock();
  } else if (c == CMD_NEXT && viaPhone) {
    stateToast("Next");
  } else if (c == CMD_PREV && viaPhone) {
    stateToast("Previous");
  }
}

static void handleTouch() {
  bool down = digitalRead(TOUCH_PIN) == HIGH;
  unsigned long now = millis();
  if (ignoreUntilRelease) {          // the touch that woke us up
    if (!down) ignoreUntilRelease = false;
    lastTouch = down;
    return;
  }
  if (down && !lastTouch) {
    pressStart = now;
    longFired = false;
    markActivity();
  }
  if (down && !longFired && now - pressStart > LONG_PRESS_MS) {
    longFired = true;
    tapCount = 0;
    infoUntil = now + INFO_SCREEN_MS;
  }
  if (!down && lastTouch && !longFired && now - pressStart > 30) {   // 30 ms debounce
    tapCount++;
    lastTapTime = now;
  }
  lastTouch = down;

  if (tapCount > 0 && !down && now - lastTapTime > TAP_WINDOW_MS) {
    sendCommand(tapCount == 1 ? CMD_TOGGLE : tapCount == 2 ? CMD_NEXT : CMD_PREV);
    tapCount = 0;
    markActivity();
  }
}

static Screen pickScreen(const AppState &st) {
  unsigned long now = millis();
  if (st.otaActive) return SCR_OTA;
  if (st.blePasskey) return SCR_PASSKEY;
  if ((long)(infoUntil - now) > 0) return SCR_INFO;
  if (st.playing) return SCR_NOW_PLAYING;
  if (st.hasTrack && lastPlayingAt && now - lastPlayingAt < PAUSED_TO_PANDA_MS) return SCR_NOW_PLAYING;
#if WIFI_MODE
  // Reading Spotify directly (the app isn't linked): show Wi-Fi / Spotify trouble.
  if (st.spotifyDirect && !st.appLinked && !st.hasTrack) {
    if (st.wifi == NET_NEED_SETUP) return SCR_SETUP;
    if (st.wifi == NET_FAILED) return SCR_NO_WIFI;
    if (st.wifi == NET_CONNECTING) return SCR_CONNECTING;
    if (st.spotify == SPOT_AUTH_FAILED || st.spotify == SPOT_ERROR) return SCR_SPOTIFY_ERROR;
  }
#endif
  if (!st.bleConnected && !bleHasBonds()) return SCR_SETUP;   // never paired: show how
  return SCR_PANDA;
}

void setup() {
#if !WIFI_MODE
  setCpuFrequencyMhz(CPU_MHZ_BLE_ONLY);
#endif
  Serial.begin(115200);
  pinMode(TOUCH_PIN, INPUT_PULLDOWN);   // reads "not touched" if the TTP223 isn't wired yet
  // Woken by the timer = the pad was stuck down when we went to sleep. Check it again
  // quietly (the screen stays off) and go back to sleep until someone really taps it.
  if (esp_sleep_get_wakeup_cause() == ESP_SLEEP_WAKEUP_TIMER) sleepNow();
  bool wokeFromTouch = esp_sleep_get_wakeup_cause() == ESP_SLEEP_WAKEUP_GPIO;
  ignoreUntilRelease = digitalRead(TOUCH_PIN) == HIGH;

  settingsBegin();
  stateBegin();
  uiBegin();
  uiMessage(wokeFromTouch ? "Waking up..." : "Bag Clip", "v" FW_VERSION);

  batteryBegin();
  if (batteryPresent() && !batteryOnUsb() && batteryMv() < BATT_CRITICAL_MV) {
    enterDeepSleep("Battery low", "Charge or switch off");
  }

  bleBegin();
  wifiLinkBegin();
  markActivity();
}

void loop() {
  handleTouch();
  batteryUpdate();
  bleUpdate();
  wifiLinkLoop();

  Command c;
  while (receiveCommand(c, 0)) dispatchCommand(c);

  AppState st = stateGet();
  unsigned long now = millis();
  if (st.playing || st.otaActive || st.blePasskey) lastActivity = now;
  if (st.playing) lastPlayingAt = now;

  Screen screen = pickScreen(st);
  bool changed = screen != lastScreen;
  if (changed) {
    lastScreen = screen;
    idleScreenSince = now;
  }

  // Dim the idle panda after a few seconds; anything else is shown at full brightness.
  bool dim = screen == SCR_PANDA && settings.dimAfterSec > 0 && now - idleScreenSince > settings.dimAfterSec * 1000UL;
  uiSetContrast(dim ? DIM_BRIGHTNESS : settings.brightness);
  uiPandaRest(dim);   // dimmed: stop animating (no redraws) until something happens
  uiRender(screen, st, changed);

  // Even asleep the board draws a little, so a flat clip left on keeps draining the
  // LiPo: the message asks for the switch to be turned off.
  if (batteryCritical()) enterDeepSleep("Battery low", "Charge or switch off");

  // Sleep when nothing has played for a while, even with the phone connected: the app
  // reconnects by itself a second or two after a tap wakes the clip.
  unsigned long sleepMs = (screen == SCR_SETUP) ? SETUP_SLEEP_MS : settings.sleepAfterMin * 60000UL;
  if (sleepMs > 0 && !st.playing && !st.otaActive && !st.blePasskey && now - lastActivity > sleepMs) {
    enterDeepSleep("Sleeping...", "Tap to wake");
  }
  delay(5);   // leaves the CPU to the Bluetooth (and network) tasks
}
