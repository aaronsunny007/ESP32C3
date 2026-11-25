// State shared between the Bluetooth task, the display loop and (in Wi-Fi mode) the
// network task. Always copy it out with stateGet() / change it inside stateLock().
#pragma once
#include <Arduino.h>

enum WifiState : uint8_t {   // Wi-Fi mode only
  NET_NEED_SETUP,    // no networks saved yet: add one over Bluetooth
  NET_CONNECTING,
  NET_CONNECTED,
  NET_FAILED,        // saved networks not in range / wrong password
};

enum SpotifyState : uint8_t {   // Wi-Fi mode only
  SPOT_IDLE,          // not tried yet
  SPOT_OK,
  SPOT_AUTH_FAILED,   // bad keys in secrets.h
  SPOT_ERROR,         // network / TLS trouble
};

#define ART_SIZE 32   // thumbnail is ART_SIZE x ART_SIZE, 1 bit per pixel

struct AppState {
  // Bluetooth
  bool     bleConnected = false;          // anything connected (app, phone page, nRF Connect)
  bool     appLinked = false;             // the Bag Clip app is connected and listening for taps
  uint32_t blePasskey = 0;                // non-zero while the phone is asking for the PIN
  int      phoneBattery = -1;             // % sent by the app, -1 = unknown
  char     sourceApp[24] = "";            // which phone app is playing, e.g. "Spotify"

  // Wi-Fi mode only
  WifiState    wifi = NET_CONNECTING;
  char         ssid[33] = "";
  char         ip[16] = "";
  bool         spotifyDirect = false;     // Spotify keys in secrets.h: poll Spotify when the app isn't linked
  SpotifyState spotify = SPOT_IDLE;
  int          lastHttpStatus = 0;

  // Now playing (from the app, or from Spotify in Wi-Fi mode)
  bool  hasTrack = false;
  bool  playing = false;
  char  trackId[24] = "";
  char  title[96] = "";
  char  artist[64] = "";
  char  nextTitle[96] = "";               // next up in the queue ("" if unknown)
  long  progressMs = 0;
  long  durationMs = 1;
  unsigned long progressStamp = 0;        // millis() when progressMs was read
  uint32_t trackVersion = 0;              // bumps on a new song (restarts scrolling)

  // Album art
  bool    artValid = false;
  uint8_t art[ART_SIZE * ART_SIZE / 8];

  // Short message over the current screen, e.g. "Phone not connected"
  char toast[22] = "";
  unsigned long toastUntil = 0;

  // Over-the-air update (Wi-Fi mode)
  bool otaActive = false;
  int  otaPercent = 0;
};

void stateBegin();
void stateLock();
void stateUnlock();
AppState stateGet();     // a consistent copy for drawing
AppState &stateRef();    // only between stateLock() / stateUnlock()
void stateToast(const char *msg);

// Replaces non-ASCII (UTF-8) characters with '?' so the built-in font can draw them.
void toDisplayable(const char *in, char *out, size_t outLen);

// Play controls from the touch pad, the phone page or the web page. The main loop
// sends them to the phone app over Bluetooth (or to Spotify in Wi-Fi mode).
enum Command : uint8_t { CMD_TOGGLE, CMD_PLAY, CMD_PAUSE, CMD_NEXT, CMD_PREV, CMD_REFRESH, CMD_WIFI_CHANGED };
void sendCommand(Command c);
bool receiveCommand(Command &c, uint32_t waitMs);
// Wi-Fi mode: jobs for the network task (Spotify controls, "Wi-Fi list changed").
void sendNetCommand(Command c);
bool receiveNetCommand(Command &c, uint32_t waitMs);
