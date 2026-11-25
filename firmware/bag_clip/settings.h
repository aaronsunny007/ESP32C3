// User settings and saved Wi-Fi networks, kept in flash (NVS) so they survive
// sleep, power-off and firmware updates. Changed from the phone (Bluetooth) or
// the web page (Wi-Fi).
#pragma once
#include <Arduino.h>
#include "config.h"

struct Settings {
  uint8_t  brightness;     // OLED contrast 1-255
  uint16_t dimAfterSec;    // dim the idle screen after N s (0 = never)
  uint16_t sleepAfterMin;  // deep sleep after N min with nothing playing (0 = never)
  uint8_t  scrollSpeed;    // 1-5
  bool     showArt;        // album art thumbnail
};

struct WifiNet {
  char ssid[33];
  char pass[65];
};

extern Settings settings;

void   settingsBegin();
void   settingsSave();
// Apply "key=value;key=value" (keys: brightness, dim, sleep, scroll, art).
// Returns false if any key or value was not accepted. Saves on success.
bool   settingsApply(const String &kv);
String settingsToString();

// Saved Wi-Fi networks (thread-safe: used from the Bluetooth, web and network tasks).
int    wifiCount();
bool   wifiGet(int index, WifiNet &out);
bool   wifiAdd(const char *ssid, const char *pass);   // updates the password if the SSID exists
bool   wifiRemove(const char *ssid);
String wifiListSsids(char sep = '\n');
uint32_t wifiListVersion();                           // bumps on every change

// Spotify may hand out a new refresh token; it is kept here.
String storedRefreshToken(const char *fromSecrets);
void   storeRefreshToken(const char *fromSecrets, const String &token);
