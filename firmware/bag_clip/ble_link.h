// Bluetooth LE link to the phone. This is the clip's only connection in the default
// build: the Bag Clip Android app (phone-app/) writes the song, progress and album art,
// and listens for taps on the clip so it can play/pause/skip on the phone.
// Pairing needs the 6-digit PIN shown on the OLED, so everything is encrypted.
// See PROJECT.md ("Bluetooth protocol") for the formats.
#pragma once
#include <Arduino.h>
#include "state.h"

#define BLE_SVC_UUID      "b4c1a000-6f1a-4a8e-9c3e-2f5d7a0e1c00"
#define BLE_WIFI_UUID     "b4c1a001-6f1a-4a8e-9c3e-2f5d7a0e1c00"  // Wi-Fi mode only: write "add\nSSID\nPASS" / "del\nSSID"; read = saved SSIDs
#define BLE_SETTINGS_UUID "b4c1a002-6f1a-4a8e-9c3e-2f5d7a0e1c00"  // read/write "brightness=200;dim=8;sleep=5;scroll=3;art=1"
#define BLE_CONTROL_UUID  "b4c1a003-6f1a-4a8e-9c3e-2f5d7a0e1c00"  // write toggle | play | pause | next | prev | refresh
#define BLE_STATUS_UUID   "b4c1a004-6f1a-4a8e-9c3e-2f5d7a0e1c00"  // read/notify key=value lines
#define BLE_NOW_UUID      "b4c1a005-6f1a-4a8e-9c3e-2f5d7a0e1c00"  // app writes now-playing key=value lines
#define BLE_ART_UUID      "b4c1a006-6f1a-4a8e-9c3e-2f5d7a0e1c00"  // app writes 128-byte 32x32 1-bit album art
#define BLE_BUTTONS_UUID  "b4c1a007-6f1a-4a8e-9c3e-2f5d7a0e1c00"  // clip notifies toggle | play | pause | next | prev | refresh

void bleBegin();
void bleUpdate();                    // call from loop(): refreshes the status characteristic
bool bleHasBonds();                  // a phone has paired before
bool bleSendToPhone(Command c);      // tell the app to play/pause/skip; false if the app isn't listening
