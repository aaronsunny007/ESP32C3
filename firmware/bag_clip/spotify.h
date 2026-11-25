// Small Spotify Web API client: token refresh, now playing, queue, playback
// control and album-art download. Runs only on the network task.
#pragma once
#include <Arduino.h>

struct TrackInfo {
  bool  hasTrack = false;
  bool  playing = false;
  char  id[24] = "";
  char  title[96] = "";
  char  artist[64] = "";
  char  artUrl[128] = "";   // smallest album image (64x64 JPEG)
  long  progressMs = 0;
  long  durationMs = 1;
};

namespace Spotify {
  void begin();
  bool refreshToken();                       // true when we have a valid access token
  bool authFailed();                         // the keys in secrets.h were rejected
  int  nowPlaying(TrackInfo &t);             // 200 playing/paused, 204 nothing, <0 or 4xx/5xx error
  int  nextInQueue(char *title, size_t len); // 200 on success
  int  play();
  int  pause();
  int  next();
  int  previous();
  // Downloads a (small) image into buf. Returns the byte count, or <0 on error.
  int  downloadImage(const char *url, uint8_t *buf, size_t cap);
}

// Replaces non-ASCII (UTF-8) characters with '?' so the built-in font can draw them.
void toDisplayable(const char *in, char *out, size_t outLen);
