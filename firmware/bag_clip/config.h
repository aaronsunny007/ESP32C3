// Pins, timings and defaults for the bag clip. Change hardware details here.
#pragma once
#include <Arduino.h>

#define FW_VERSION   "3.0.0"
#define DEVICE_NAME  "BagClip"     // Bluetooth name, and http://bagclip.local on Wi-Fi
#define MDNS_NAME    "bagclip"

// ---------- Connection mode ----------
// 0 (default): Bluetooth only. The Bag Clip app on your Android phone sends the song,
//              album art and progress; taps on the clip go back to the phone. No Wi-Fi,
//              no hotspot, no Spotify keys, no secrets.h.
// 1:           also turn on Wi-Fi for over-the-air updates, the settings web page and
//              (if Spotify keys are in secrets.h) reading Spotify directly when the app
//              isn't connected. Uses much more battery on the clip and needs secrets.h.
#ifndef WIFI_MODE
#define WIFI_MODE 0
#endif

// ---------- Bluetooth power ----------
// After pairing, the clip asks the phone for a slow connection: one radio exchange every
// 100-150 ms, and the clip may skip up to 4 of them when it has nothing to say.
// Taps still reach the phone within ~150 ms; phone -> clip updates within ~0.75 s.
#define BLE_CONN_MIN_INTERVAL   80   // x 1.25 ms = 100 ms
#define BLE_CONN_MAX_INTERVAL  120   // x 1.25 ms = 150 ms
#define BLE_CONN_LATENCY         4   // connection events the clip may skip
#define BLE_CONN_TIMEOUT       600   // x 10 ms = 6 s without contact = disconnected
#define BLE_ADV_MIN_INTERVAL   400   // x 0.625 ms = 250 ms between adverts while waiting for the phone
#define BLE_ADV_MAX_INTERVAL   800   // x 0.625 ms = 500 ms
#define CPU_MHZ_BLE_ONLY        80   // Bluetooth runs fine at 80 MHz and it saves ~10 mA

// ---------- Pins (ESP32-C3 SuperMini) ----------
#define OLED_SDA      8
#define OLED_SCL      9
#define TOUCH_PIN     3            // must be GPIO0-5 to wake the C3 from deep sleep
#define BATT_PIN      4            // ADC1 channel 4. Battery voltage divider midpoint
#define SCREEN_WIDTH  128
#define SCREEN_HEIGHT 64
#define OLED_ADDR     0x3C

// ---------- Battery (see PROJECT.md "Battery level") ----------
// 100k from battery+ to GPIO4, 100k from GPIO4 to GND  ->  ratio (100k+100k)/100k = 2.
#define BATT_DIVIDER     2.0f
// Multiply the reading by this after comparing with a multimeter (e.g. 4.02 V real / 3.95 V shown = 1.018).
#define BATT_CAL         1.000f
#define BATT_MISSING_MV  1500      // below this at the pin*ratio: no divider fitted, hide the icon
#define BATT_USB_MV      4400      // above this we are on USB 5 V, not the LiPo
#define BATT_CRITICAL_MV 3450      // the board's 3.3 V regulator browns out around here
#define BATT_READ_MS     5000UL

// ---------- Timing ----------
#define POLL_PLAYING_MS    3500UL  // Wi-Fi mode only: Spotify polling
#define POLL_IDLE_MS      10000UL
#define TAP_WINDOW_MS       400UL  // gap that ends a tap sequence
#define LONG_PRESS_MS      1500UL  // hold to show the info screen
#define INFO_SCREEN_MS     6000UL
#define PAUSED_TO_PANDA_MS 10000UL // show the paused track this long, then the panda
#define FRAME_MS             25UL  // ~40 fps cap; one full I2C frame takes ~23 ms at 400 kHz
#define MAX_POLL_FAILURES      5   // then treat as "not playing" so the sleep timer runs
#define SETUP_SLEEP_MS (10UL * 60UL * 1000UL) // never paired (or no Wi-Fi saved in Wi-Fi mode): sleep after 10 min

#define MAX_WIFI_NETWORKS 5

// ---------- Defaults for the settings page (stored in flash once changed) ----------
#define DEFAULT_BRIGHTNESS  200    // OLED contrast 1-255
#define DEFAULT_DIM_SEC       8    // dim the idle panda after this many seconds (0 = never)
#define DEFAULT_SLEEP_MIN     5    // deep sleep after this many minutes with nothing playing (0 = never), even with the phone connected
#define DEFAULT_SCROLL        3    // title scroll speed 1 (slow) - 5 (fast)
#define DEFAULT_SHOW_ART   true    // black-and-white album art thumbnail
#define DIM_BRIGHTNESS        1    // contrast used when dimmed
