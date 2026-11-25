/*
 * ESP32-C3 Spotify "Now Playing" Bag Clip — firmware v2
 *
 * Bluetooth LE: phone pairing (PIN on screen), Wi-Fi setup, settings, play controls.
 * Wi-Fi:        Spotify (now playing, album art, queue), settings web page, OTA updates.
 * See PROJECT.md for wiring, libraries and the phone page.
 *
 * Board:     ESP32C3 Dev Module, USB CDC On Boot: Enabled,
 *            Partition Scheme: Minimal SPIFFS (1.9MB APP with OTA/190KB SPIFFS)
 * Libraries: Adafruit SSD1306, Adafruit GFX Library, ArduinoJson 7.x,
 *            NimBLE-Arduino 2.x, TJpg_Decoder
 *
 * Touch pad (TTP223 on GPIO3):
 *   1 tap -> play / pause    2 taps -> next    3 taps -> previous
 *   hold 1.5 s -> info screen (IP address, battery, version)
 *   tap while asleep -> wake
 */

#include <WiFi.h>
#include "esp_sleep.h"
#include "config.h"
#include "settings.h"
#include "state.h"
#include "battery.h"
#include "ui.h"
#include "spotify.h"
#include "art.h"
#include "ble_link.h"
#include "web_portal.h"

#if __has_include("secrets.h")
  #include "secrets.h"
#else
  #error "Missing secrets.h: copy secrets.example.h to secrets.h and fill in your keys."
#endif

// Many ESP32-C3 SuperMini boards fail to join Wi-Fi at full TX power
// (antenna/regulator issue). 8.5 dBm is the common fix; raise it if range is poor.
static const wifi_power_t WIFI_TX_POWER = WIFI_POWER_8_5dBm;

// =====================================================================
// Network task: Wi-Fi and Spotify. Runs on its own so slow HTTPS calls
// never freeze the screen or the touch pad.
// =====================================================================

static void setWifiState(WifiState w, const char *ssid) {   // ssid nullptr = keep
  stateLock();
  AppState &s = stateRef();
  s.wifi = w;
  if (ssid) strlcpy(s.ssid, ssid, sizeof(s.ssid));
  if (w == NET_CONNECTED) strlcpy(s.ip, WiFi.localIP().toString().c_str(), sizeof(s.ip));
  else s.ip[0] = 0;
  stateUnlock();
}

static void toast(const char *msg) {
  stateLock();
  strlcpy(stateRef().toast, msg, sizeof(stateRef().toast));
  stateRef().toastUntil = millis() + 2500;
  stateUnlock();
}

static bool tryJoin(const WifiNet &n, uint32_t timeoutMs) {
  setWifiState(NET_CONNECTING, n.ssid);
  WiFi.begin(n.ssid, n.pass);
  WiFi.setTxPower(WIFI_TX_POWER);
  unsigned long t0 = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - t0 < timeoutMs) vTaskDelay(pdMS_TO_TICKS(200));
  if (WiFi.status() == WL_CONNECTED) return true;
  WiFi.disconnect();
  return false;
}

// Joins the strongest saved network in range. Hidden networks don't show in a scan,
// so if none is seen, each saved one is tried directly.
static bool connectWifi() {
  int count = wifiCount();
  if (count == 0) {
    setWifiState(NET_NEED_SETUP, "");
    return false;
  }
  setWifiState(NET_CONNECTING, "");
  int found = WiFi.scanNetworks();
  int bestRssi = -1000;
  WifiNet best;
  bool haveBest = false;
  for (int i = 0; i < found; i++) {
    for (int k = 0; k < count; k++) {
      WifiNet n;
      if (wifiGet(k, n) && WiFi.SSID(i) == n.ssid && WiFi.RSSI(i) > bestRssi) {
        bestRssi = WiFi.RSSI(i);
        best = n;
        haveBest = true;
      }
    }
  }
  WiFi.scanDelete();

  bool ok = false;
  if (haveBest) {
    ok = tryJoin(best, 15000);
  } else {
    for (int k = 0; k < count && !ok; k++) {
      WifiNet n;
      if (wifiGet(k, n)) ok = tryJoin(n, 8000);
    }
  }
  if (ok) {
    setWifiState(NET_CONNECTED, WiFi.SSID().c_str());
    Serial.printf("[wifi] connected to %s, IP %s\n", WiFi.SSID().c_str(), WiFi.localIP().toString().c_str());
  } else {
    setWifiState(NET_FAILED, "");
  }
  return ok;
}

static uint8_t jpgBuf[12 * 1024];   // Spotify's 64x64 thumbnails are ~2-5 KB

static void fetchArt(const char *trackId, const char *url) {
  int n = Spotify::downloadImage(url, jpgBuf, sizeof(jpgBuf));
  uint8_t bmp[ART_SIZE * ART_SIZE / 8];
  if (n > 0 && artConvertJpeg(jpgBuf, n, bmp)) {
    stateLock();
    if (strcmp(stateRef().trackId, trackId) == 0) {   // still the same song
      memcpy(stateRef().art, bmp, sizeof(bmp));
      stateRef().artValid = true;
    }
    stateUnlock();
  }
}

static void fetchQueue(const char *trackId) {
  char next[96];
  if (Spotify::nextInQueue(next, sizeof(next)) != 200) return;
  stateLock();
  if (strcmp(stateRef().trackId, trackId) == 0) strlcpy(stateRef().nextTitle, next, sizeof(stateRef().nextTitle));
  stateUnlock();
}

static void explainControlError(int code) {
  if (code == 403) toast("Needs Spotify Premium");
  else if (code == 404) toast("No active device");
  else if (code == 401) toast("Spotify login failed");
  else if (code < 0 || code >= 500) toast("Can't reach Spotify");
}

static bool controlOk(int code) { return code >= 200 && code < 300; }

static void runCommand(Command c) {
  AppState st = stateGet();
  int code = 0;
  switch (c) {
    case CMD_TOGGLE:
    case CMD_PLAY:
    case CMD_PAUSE: {
      bool wantPlay = (c == CMD_PLAY) || (c == CMD_TOGGLE && !st.playing);
      // Flip the icon straight away; the next poll confirms it.
      stateLock();
      AppState &s = stateRef();
      if (s.playing) s.progressMs += (long)(millis() - s.progressStamp);
      s.progressStamp = millis();
      s.playing = wantPlay;
      stateUnlock();
      code = wantPlay ? Spotify::play() : Spotify::pause();
      break;
    }
    case CMD_NEXT: code = Spotify::next(); if (controlOk(code)) toast("Next"); break;
    case CMD_PREV: code = Spotify::previous(); if (controlOk(code)) toast("Previous"); break;
    default: return;
  }
  if (!controlOk(code)) explainControlError(code);
}

static void netTask(void *) {
  Spotify::begin();
  unsigned long nextPoll = 0, wifiRetryAt = 0, lastQueueFetch = 0;
  int wifiBackoff = 0, failures = 0;
  bool wasConnected = false;
  char artTriedFor[24] = "";

  for (;;) {
    if (stateGet().otaActive) { vTaskDelay(pdMS_TO_TICKS(500)); continue; }

    // ---- Wi-Fi ----
    if (WiFi.status() != WL_CONNECTED) {
      if (wasConnected) {                          // dropped: give auto-reconnect 10 s first
        wasConnected = false;
        setWifiState(NET_CONNECTING, nullptr);
        wifiRetryAt = millis() + 10000;
      }
      if ((long)(millis() - wifiRetryAt) >= 0) {
        if (connectWifi()) {
          wifiBackoff = 0;
          nextPoll = millis();
        } else {
          wifiBackoff = min(wifiBackoff + 1, 4);   // 20 s, 40 s, 80 s, 160 s
          wifiRetryAt = millis() + 10000UL * (1UL << wifiBackoff);
        }
      }
      Command c;
      if (receiveCommand(c, 1000) && c == CMD_WIFI_CHANGED) wifiRetryAt = millis();  // new network: try now
      continue;
    }
    if (!wasConnected) {
      wasConnected = true;
      setWifiState(NET_CONNECTED, WiFi.SSID().c_str());
    }

    // ---- Commands (taps, phone, web page) wake us up early ----
    long wait = (long)(nextPoll - millis());
    Command c;
    if (receiveCommand(c, wait > 0 ? wait : 0)) {
      if (c == CMD_REFRESH) nextPoll = millis();
      else if (c != CMD_WIFI_CHANGED) {
        runCommand(c);
        nextPoll = millis() + (c == CMD_NEXT || c == CMD_PREV ? 700 : 400);  // let Spotify catch up
      }
      continue;
    }

    // ---- Poll now playing ----
    TrackInfo t;
    int code = Spotify::nowPlaying(t);
    bool newTrack = false;
    stateLock();
    AppState &s = stateRef();
    s.lastHttpStatus = code;
    if (code == 200 || code == 204) {
      failures = 0;
      s.spotify = SPOT_OK;
      newTrack = strcmp(s.trackId, t.id) != 0;
      s.hasTrack = t.hasTrack;
      s.playing = t.playing;
      strlcpy(s.trackId, t.id, sizeof(s.trackId));
      strlcpy(s.title, t.title, sizeof(s.title));
      strlcpy(s.artist, t.artist, sizeof(s.artist));
      s.progressMs = t.progressMs;
      s.durationMs = t.durationMs;
      s.progressStamp = millis();
      if (newTrack) {
        s.trackVersion++;
        s.artValid = false;
        s.nextTitle[0] = 0;
      }
    } else {
      s.spotify = Spotify::authFailed() ? SPOT_AUTH_FAILED : SPOT_ERROR;
      // Without this, a dropped hotspot while playing would keep the clip awake forever.
      if (++failures >= MAX_POLL_FAILURES) s.playing = false;
    }
    bool playing = s.playing;
    stateUnlock();

    // ---- Extras for the current song: album art and what's next ----
    if (t.hasTrack && code == 200) {
      if (settings.showArt && t.artUrl[0] && strcmp(artTriedFor, t.id) != 0) {
        strlcpy(artTriedFor, t.id, sizeof(artTriedFor));
        fetchArt(t.id, t.artUrl);
      }
      if (newTrack || (playing && millis() - lastQueueFetch > 120000UL)) {
        lastQueueFetch = millis();
        fetchQueue(t.id);
      }
    }
    nextPoll = millis() + (playing ? POLL_PLAYING_MS : POLL_IDLE_MS);
  }
}

// =====================================================================
// Main loop: touch, display, battery, Bluetooth status, web page, sleep.
// =====================================================================

static unsigned long lastActivity = 0, idleScreenSince = 0, lastPlayingAt = 0, infoUntil = 0;
static unsigned long pressStart = 0, lastTapTime = 0;
static int tapCount = 0;
static bool lastTouch = false, longFired = false, ignoreUntilRelease = false;
static Screen lastScreen = SCR_CONNECTING;

static void markActivity() {
  lastActivity = millis();
  idleScreenSince = millis();   // also undims the screen
}

static void enterDeepSleep(const char *line1, const char *line2) {
  uiSetContrast(settings.brightness);
  uiMessage(line1, line2);
  delay(1200);
  uiOff();

  // If a finger is still on the pad, a HIGH-level wake would fire instantly.
  unsigned long t0 = millis();
  while (digitalRead(TOUCH_PIN) == HIGH && millis() - t0 < 3000) delay(10);

  WiFi.disconnect(true);
  WiFi.mode(WIFI_OFF);
  esp_deep_sleep_enable_gpio_wakeup(1ULL << TOUCH_PIN, ESP_GPIO_WAKEUP_GPIO_HIGH);
  esp_deep_sleep_start();
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
  if (st.wifi == NET_NEED_SETUP) return SCR_SETUP;
  if (!st.hasTrack && st.wifi == NET_FAILED) return SCR_NO_WIFI;
  if (!st.hasTrack && st.wifi == NET_CONNECTING) return SCR_CONNECTING;
  if (st.playing) return SCR_NOW_PLAYING;
  if (st.hasTrack && lastPlayingAt && now - lastPlayingAt < PAUSED_TO_PANDA_MS) return SCR_NOW_PLAYING;
  if (st.spotify == SPOT_AUTH_FAILED || (st.spotify == SPOT_ERROR && !st.hasTrack)) return SCR_SPOTIFY_ERROR;
  return SCR_PANDA;
}

void setup() {
  Serial.begin(115200);
  pinMode(TOUCH_PIN, INPUT);
  bool wokeFromTouch = esp_sleep_get_wakeup_cause() == ESP_SLEEP_WAKEUP_GPIO;
  ignoreUntilRelease = digitalRead(TOUCH_PIN) == HIGH;

  settingsBegin();
  stateBegin();
  uiBegin();
  uiMessage(wokeFromTouch ? "Waking up..." : "Bag Clip", "v" FW_VERSION);

  batteryBegin();
  if (batteryPresent() && !batteryOnUsb() && batteryMv() < BATT_CRITICAL_MV) {
    enterDeepSleep("Battery low", "Please charge me");
  }

  if (wifiCount() == 0 && strlen(DEFAULT_WIFI_SSID) > 0) wifiAdd(DEFAULT_WIFI_SSID, DEFAULT_WIFI_PASSWORD);
  WiFi.mode(WIFI_STA);
  WiFi.setHostname(MDNS_NAME);
  WiFi.setAutoReconnect(true);
  WiFi.setTxPower(WIFI_TX_POWER);
  // Note: Wi-Fi modem sleep must stay on (the default) while Bluetooth is running.

  bleBegin();
  xTaskCreate(netTask, "net", 16384, nullptr, 1, nullptr);
  markActivity();
}

void loop() {
  handleTouch();
  batteryUpdate();
  bleUpdate();

  AppState st = stateGet();
  if (st.wifi == NET_CONNECTED) webBegin();   // once; safe to call again
  webLoop();

  unsigned long now = millis();
  if (st.playing || st.bleConnected || st.otaActive) lastActivity = now;
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
  uiRender(screen, st, changed);

  if (batteryCritical()) enterDeepSleep("Battery low", "Please charge me");

  unsigned long sleepMs = (st.wifi == NET_NEED_SETUP) ? SETUP_SLEEP_MS : settings.sleepAfterMin * 60000UL;
  if (sleepMs > 0 && !st.playing && !st.bleConnected && !st.otaActive && now - lastActivity > sleepMs) {
    enterDeepSleep("Sleeping...", "Tap to wake");
  }
  delay(5);   // leaves the CPU to the network and Bluetooth tasks
}
