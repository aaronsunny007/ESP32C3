// Everything drawn on the 128x64 SSD1306.
#pragma once
#include <Arduino.h>
#include "state.h"

enum Screen : uint8_t {
  SCR_NOW_PLAYING,
  SCR_PANDA,         // idle: nothing playing
  SCR_SETUP,         // no Wi-Fi saved: pair a phone
  SCR_NO_WIFI,
  SCR_CONNECTING,
  SCR_SPOTIFY_ERROR,
  SCR_PASSKEY,       // Bluetooth pairing PIN
  SCR_INFO,          // long press: IP, battery, version
  SCR_OTA,
};

bool uiBegin();
void uiSetContrast(uint8_t contrast);
void uiOff();
void uiMessage(const char *line1, const char *line2 = nullptr, const char *line3 = nullptr);
// Idle panda: true freezes the animation on its resting picture (screen dimmed),
// false restarts it from the beginning.
void uiPandaRest(bool rest);
// Draws the screen if a new frame is due (frame rate is capped inside).
void uiRender(Screen screen, const AppState &st, bool force = false);
