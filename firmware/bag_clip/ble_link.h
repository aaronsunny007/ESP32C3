// Bluetooth LE link to the phone: pairing (with a PIN shown on the OLED), Wi-Fi
// setup, settings, playback buttons and a status readout. See PROJECT.md for the
// characteristic list; phone/index.html is a ready-made page that uses it.
#pragma once
#include <Arduino.h>

#define BLE_SVC_UUID      "b4c1a000-6f1a-4a8e-9c3e-2f5d7a0e1c00"
#define BLE_WIFI_UUID     "b4c1a001-6f1a-4a8e-9c3e-2f5d7a0e1c00"  // write "add\nSSID\nPASS" / "del\nSSID"; read = saved SSIDs
#define BLE_SETTINGS_UUID "b4c1a002-6f1a-4a8e-9c3e-2f5d7a0e1c00"  // read/write "brightness=200;dim=8;sleep=5;scroll=3;art=1"
#define BLE_CONTROL_UUID  "b4c1a003-6f1a-4a8e-9c3e-2f5d7a0e1c00"  // write toggle | play | pause | next | prev | refresh
#define BLE_STATUS_UUID   "b4c1a004-6f1a-4a8e-9c3e-2f5d7a0e1c00"  // read/notify key=value lines

void bleBegin();
void bleUpdate();          // call from loop(): refreshes the status characteristic
