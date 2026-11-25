#include "wifi_link.h"
#include "config.h"

#if !WIFI_MODE

void wifiLinkBegin() {}
void wifiLinkLoop() {}
void wifiLinkOff() {}

#else

#include <WiFi.h>
#include "settings.h"
#include "state.h"
#include "spotify.h"
#include "art.h"
#include "web_portal.h"

#if __has_include("secrets.h")
  #include "secrets.h"
#else
  #error "Wi-Fi mode needs secrets.h: copy secrets.example.h to secrets.h and fill it in (or set WIFI_MODE 0)."
#endif

// Many ESP32-C3 SuperMini boards fail to join Wi-Fi at full TX power
// (antenna/regulator issue). 8.5 dBm is the common fix; raise it if range is poor.
static const wifi_power_t WIFI_TX_POWER = WIFI_POWER_8_5dBm;

static void setWifiState(WifiState w, const char *ssid) {   // ssid nullptr = keep
  stateLock();
  AppState &s = stateRef();
  s.wifi = w;
  if (ssid) strlcpy(s.ssid, ssid, sizeof(s.ssid));
  if (w == NET_CONNECTED) strlcpy(s.ip, WiFi.localIP().toString().c_str(), sizeof(s.ip));
  else s.ip[0] = 0;
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
  if (code == 403) stateToast("Needs Spotify Premium");
  else if (code == 404) stateToast("No active device");
  else if (code == 401) stateToast("Spotify login failed");
  else if (code < 0 || code >= 500) stateToast("Can't reach Spotify");
}

static bool controlOk(int code) { return code >= 200 && code < 300; }

// The main loop has already flipped the play icon; this tells Spotify.
static void runCommand(Command c) {
  int code = 0;
  switch (c) {
    case CMD_PLAY:  code = Spotify::play(); break;
    case CMD_PAUSE: code = Spotify::pause(); break;
    case CMD_NEXT:  code = Spotify::next(); if (controlOk(code)) stateToast("Next"); break;
    case CMD_PREV:  code = Spotify::previous(); if (controlOk(code)) stateToast("Previous"); break;
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
    AppState st = stateGet();
    if (st.otaActive) { vTaskDelay(pdMS_TO_TICKS(500)); continue; }

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
      if (receiveNetCommand(c, 1000) && c == CMD_WIFI_CHANGED) wifiRetryAt = millis();  // new network: try now
      continue;
    }
    if (!wasConnected) {
      wasConnected = true;
      setWifiState(NET_CONNECTED, WiFi.SSID().c_str());
    }

    // ---- Commands wake us up early ----
    long wait = (long)(nextPoll - millis());
    Command c;
    if (receiveNetCommand(c, wait > 0 ? wait : 0)) {
      if (c == CMD_REFRESH) nextPoll = millis();
      else if (c != CMD_WIFI_CHANGED) {
        runCommand(c);
        nextPoll = millis() + (c == CMD_NEXT || c == CMD_PREV ? 700 : 400);  // let Spotify catch up
      }
      continue;
    }

    // While the phone app is linked it is the source of truth: no need to poll Spotify.
    if (!st.spotifyDirect || st.appLinked) {
      nextPoll = millis() + 2000;
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

void wifiLinkBegin() {
  if (wifiCount() == 0 && strlen(DEFAULT_WIFI_SSID) > 0) wifiAdd(DEFAULT_WIFI_SSID, DEFAULT_WIFI_PASSWORD);
  stateLock();
  stateRef().spotifyDirect = Spotify::configured();
  stateUnlock();
  WiFi.mode(WIFI_STA);
  WiFi.setHostname(MDNS_NAME);
  WiFi.setAutoReconnect(true);
  WiFi.setTxPower(WIFI_TX_POWER);
  // Note: Wi-Fi modem sleep must stay on (the default) while Bluetooth is running.
  xTaskCreate(netTask, "net", 16384, nullptr, 1, nullptr);
}

void wifiLinkLoop() {
  if (stateGet().wifi == NET_CONNECTED) webBegin();   // once; safe to call again
  webLoop();
}

void wifiLinkOff() {
  WiFi.disconnect(true);
  WiFi.mode(WIFI_OFF);
}

#endif  // WIFI_MODE
