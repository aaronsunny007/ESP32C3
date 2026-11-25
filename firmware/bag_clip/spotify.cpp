#include "spotify.h"
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <base64.h>
#include "certs.h"
#include "settings.h"

#if __has_include("secrets.h")
  #include "secrets.h"
#else
  #error "Missing secrets.h: copy secrets.example.h to secrets.h and fill in your Spotify keys."
#endif

static WiFiClientSecure apiClient;     // kept open between polls (saves a TLS handshake each time)
static WiFiClientSecure otherClient;   // short-lived: token, queue, album art
static HTTPClient apiHttp;
static String accessToken;
static unsigned long tokenExpiresAt = 0;
static bool badKeys = false;

static void setupTls(WiFiClientSecure &c) {
#if SPOTIFY_TLS_INSECURE
  c.setInsecure();
#else
  c.setCACert(ROOT_CA_BUNDLE);
#endif
}

void toDisplayable(const char *in, char *out, size_t outLen) {
  size_t o = 0;
  if (!in || outLen == 0) return;
  for (const unsigned char *p = (const unsigned char *)in; *p && o + 1 < outLen; p++) {
    if (*p < 0x80) out[o++] = (char)*p;
    else if ((*p & 0xC0) == 0xC0) out[o++] = '?';   // first byte of a multi-byte character
    // continuation bytes (10xxxxxx) are skipped
  }
  out[o] = 0;
}

// Only one TLS session at a time: each one needs ~40 KB of RAM, and Bluetooth + Wi-Fi
// already use a good share of the C3's memory.
static void closeApi() {
  apiHttp.end();
  apiClient.stop();
}

void Spotify::begin() {
  setupTls(apiClient);
  setupTls(otherClient);
}

bool Spotify::authFailed() { return badKeys; }

bool Spotify::refreshToken() {
  closeApi();
  HTTPClient http;
  http.setTimeout(10000);
  http.setConnectTimeout(10000);
  http.useHTTP10(true);
  if (!http.begin(otherClient, "https://accounts.spotify.com/api/token")) return false;
  http.addHeader("Content-Type", "application/x-www-form-urlencoded");
  http.addHeader("Authorization", "Basic " + base64::encode(String(SPOTIFY_CLIENT_ID ":" SPOTIFY_CLIENT_SECRET)));
  String rt = storedRefreshToken(SPOTIFY_REFRESH_TOKEN);
  int code = http.POST("grant_type=refresh_token&refresh_token=" + rt);   // tokens are URL-safe

  bool ok = false;
  if (code == 200) {
    JsonDocument filter;
    filter["access_token"] = true;
    filter["expires_in"] = true;
    filter["refresh_token"] = true;
    JsonDocument doc;
    if (!deserializeJson(doc, http.getStream(), DeserializationOption::Filter(filter))) {
      accessToken = doc["access_token"] | "";
      long expires = doc["expires_in"] | 3600;
      tokenExpiresAt = millis() + (unsigned long)(max(expires - 120L, 60L)) * 1000UL;
      const char *newRt = doc["refresh_token"];
      if (newRt && rt != newRt) storeRefreshToken(SPOTIFY_REFRESH_TOKEN, String(newRt));
      ok = accessToken.length() > 0;
      badKeys = false;
    }
  } else {
    Serial.printf("[spotify] token refresh failed: %d %s\n", code, code > 0 ? http.getString().c_str() : "");
    badKeys = (code == 400 || code == 401);
  }
  http.end();
  otherClient.stop();
  return ok;
}

static bool ensureToken() {
  if (accessToken.length() && (long)(millis() - tokenExpiresAt) < 0) return true;
  accessToken = "";
  return Spotify::refreshToken();
}

// One call on the kept-alive api.spotify.com connection. Retries once on a stale
// connection or an expired token.
static int apiCall(const char *method, const char *path, String *body = nullptr) {
  for (int attempt = 0; attempt < 2; attempt++) {
    if (!ensureToken()) return badKeys ? 401 : -1;
    apiHttp.setReuse(true);
    apiHttp.setTimeout(8000);
    apiHttp.setConnectTimeout(8000);
    if (!apiHttp.begin(apiClient, "api.spotify.com", 443, path, true)) return -1;
    apiHttp.addHeader("Authorization", "Bearer " + accessToken);
    bool hasBody = strcmp(method, "GET") != 0;
    if (hasBody) apiHttp.addHeader("Content-Length", "0");   // Spotify answers 411 without it
    int code = apiHttp.sendRequest(method, (uint8_t *)nullptr, 0);

    if (code < 0) {                    // connection dropped (server closed keep-alive): reconnect once
      closeApi();
      if (attempt == 0) continue;
      return code;
    }
    // Always read the body so the kept-alive connection is clean for the next call.
    String b = (code == 204) ? String() : apiHttp.getString();
    apiHttp.end();
    if (code == 401 && attempt == 0) {  // token expired early
      accessToken = "";
      continue;
    }
    if (code >= 400) Serial.printf("[spotify] %s %s -> %d %s\n", method, path, code, b.c_str());
    if (body) *body = b;
    return code;
  }
  return -1;
}

int Spotify::nowPlaying(TrackInfo &t) {
  String body;
  int code = apiCall("GET", "/v1/me/player/currently-playing?market=" SPOTIFY_MARKET, &body);
  if (code == 204) {          // nothing playing on any device
    t = TrackInfo();
    return 204;
  }
  if (code != 200) return code;

  JsonDocument filter;
  filter["is_playing"] = true;
  filter["progress_ms"] = true;
  filter["currently_playing_type"] = true;
  filter["item"]["id"] = true;
  filter["item"]["name"] = true;
  filter["item"]["duration_ms"] = true;
  filter["item"]["artists"][0]["name"] = true;
  filter["item"]["album"]["images"][0]["url"] = true;
  filter["item"]["album"]["images"][0]["width"] = true;
  JsonDocument doc;
  if (deserializeJson(doc, body, DeserializationOption::Filter(filter))) return -2;
  body = String();

  TrackInfo n;
  n.hasTrack = true;
  n.playing = doc["is_playing"] | false;
  n.progressMs = doc["progress_ms"] | 0;
  JsonObject item = doc["item"];
  if (item.isNull()) {        // adverts and unknown items have no "item"
    const char *type = doc["currently_playing_type"] | "unknown";
    strlcpy(n.id, type, sizeof(n.id));
    strlcpy(n.title, strcmp(type, "ad") == 0 ? "Advert" : "Something", sizeof(n.title));
    n.durationMs = 1;
  } else {
    strlcpy(n.id, item["id"] | "local", sizeof(n.id));
    toDisplayable(item["name"] | "", n.title, sizeof(n.title));
    toDisplayable(item["artists"][0]["name"] | "", n.artist, sizeof(n.artist));
    n.durationMs = max(1L, (long)(item["duration_ms"] | 1));
    int best = 100000;
    for (JsonObject img : item["album"]["images"].as<JsonArray>()) {
      int w = img["width"] | 0;
      const char *url = img["url"];
      if (url && w > 0 && w < best) {
        best = w;
        strlcpy(n.artUrl, url, sizeof(n.artUrl));
      }
    }
  }
  t = n;
  return 200;
}

int Spotify::nextInQueue(char *title, size_t len) {
  title[0] = 0;
  if (!ensureToken()) return badKeys ? 401 : -1;
  closeApi();
  HTTPClient http;
  http.useHTTP10(true);          // no chunked encoding, so the reply can be parsed as it streams in
  http.setTimeout(10000);
  if (!http.begin(otherClient, "api.spotify.com", 443, "/v1/me/player/queue", true)) return -1;
  http.addHeader("Authorization", "Bearer " + accessToken);
  int code = http.GET();
  if (code == 200) {
    // The reply lists up to 20 full tracks (tens of KB); keep only names.
    JsonDocument filter;
    filter["queue"][0]["name"] = true;
    filter["queue"][0]["artists"][0]["name"] = true;
    JsonDocument doc;
    if (!deserializeJson(doc, http.getStream(), DeserializationOption::Filter(filter))) {
      const char *name = doc["queue"][0]["name"];
      const char *artist = doc["queue"][0]["artists"][0]["name"];
      if (name) {
        String s = name;
        if (artist) s += String(" - ") + artist;
        toDisplayable(s.c_str(), title, len);
      }
    } else {
      code = -2;
    }
  } else {
    Serial.printf("[spotify] queue -> %d\n", code);
  }
  http.end();
  otherClient.stop();
  return code;
}

int Spotify::play()     { return apiCall("PUT", "/v1/me/player/play"); }
int Spotify::pause()    { return apiCall("PUT", "/v1/me/player/pause"); }
int Spotify::next()     { return apiCall("POST", "/v1/me/player/next"); }
int Spotify::previous() { return apiCall("POST", "/v1/me/player/previous"); }

int Spotify::downloadImage(const char *url, uint8_t *buf, size_t cap) {
  closeApi();
  HTTPClient http;
  http.setTimeout(8000);
  if (!http.begin(otherClient, url)) return -1;
  int result = -1;
  int code = http.GET();
  if (code == 200) {
    int len = http.getSize();
    if (len > 0 && (size_t)len <= cap) {
      auto *stream = http.getStreamPtr();
      int got = 0;
      unsigned long t0 = millis();
      while (got < len && millis() - t0 < 8000) {
        int avail = stream->available();
        if (avail > 0) got += stream->read(buf + got, min(avail, len - got));
        else delay(5);
      }
      if (got == len) result = len;
    } else {
      Serial.printf("[spotify] art size %d doesn't fit buffer\n", len);
    }
  } else {
    Serial.printf("[spotify] art download -> %d\n", code);
  }
  http.end();
  otherClient.stop();
  return result;
}
