// Wi-Fi mode (WIFI_MODE 1 in config.h): joins a saved network, serves the settings
// page and OTA updates, and reads Spotify directly while the phone app isn't linked.
// In the default Bluetooth-only build these do nothing.
#pragma once
#include <Arduino.h>

void wifiLinkBegin();   // from setup()
void wifiLinkLoop();    // from loop(): web page and OTA
void wifiLinkOff();     // before deep sleep
