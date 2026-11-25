// State shared between the network task (Spotify, Wi-Fi), the Bluetooth task and
// the display loop. Always copy it out with stateGet() / change it inside stateLock().
#pragma once
#include <Arduino.h>

enum WifiState : uint8_t {
  NET_NEED_SETUP,    // no networks saved yet: pair a phone over Bluetooth
  NET_CONNECTING,
  NET_CONNECTED,
  NET_FAILED,        // saved networks not in range / wrong password
};

enum SpotifyState : uint8_t {
  SPOT_IDLE,          // not tried yet
  SPOT_OK,
  SPOT_AUTH_FAILED,   // bad keys in secrets.h
  SPOT_ERROR,         // network / TLS trouble
};

#define ART_SIZE 32   // thumbnail is ART_SIZE x ART_SIZE, 1 bit per pixel

struct AppState {
  // Connectivity
  WifiState    wifi = NET_CONNECTING;
  char         ssid[33] = "";
  char         ip[16] = "";
  bool         bleConnected = false;
  uint32_t     blePasskey = 0;          // non-zero while the phone is asking for the PIN
  SpotifyState spotify = SPOT_IDLE;
  int          lastHttpStatus = 0;

  // Now playing
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

  // Short message over the current screen, e.g. "Needs Premium"
  char toast[22] = "";
  unsigned long toastUntil = 0;

  // Over-the-air update
  bool otaActive = false;
  int  otaPercent = 0;
};

void stateBegin();
void stateLock();
void stateUnlock();
AppState stateGet();     // a consistent copy for drawing
AppState &stateRef();    // only between stateLock() / stateUnlock()

// Things the display loop, Bluetooth or the web page ask the network task to do.
enum Command : uint8_t { CMD_TOGGLE, CMD_PLAY, CMD_PAUSE, CMD_NEXT, CMD_PREV, CMD_REFRESH, CMD_WIFI_CHANGED };
void sendCommand(Command c);
bool receiveCommand(Command &c, uint32_t waitMs);
