// Pins, timings and defaults for the bag clip. Change hardware details here.
#pragma once
#include <Arduino.h>

#define FW_VERSION   "2.0.0"
#define DEVICE_NAME  "BagClip"     // Bluetooth name, and http://bagclip.local on Wi-Fi
#define MDNS_NAME    "bagclip"

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
#define POLL_PLAYING_MS    3500UL
#define POLL_IDLE_MS      10000UL
#define TAP_WINDOW_MS       400UL  // gap that ends a tap sequence
#define LONG_PRESS_MS      1500UL  // hold to show the info screen
#define INFO_SCREEN_MS     6000UL
#define PAUSED_TO_PANDA_MS 10000UL // show the paused track this long, then the panda
#define FRAME_MS             25UL  // ~40 fps cap; one full I2C frame takes ~23 ms at 400 kHz
#define MAX_POLL_FAILURES      5   // then treat as "not playing" so the sleep timer runs
#define SETUP_SLEEP_MS (10UL * 60UL * 1000UL) // no Wi-Fi saved and nobody pairs: sleep after 10 min

#define MAX_WIFI_NETWORKS 5

// ---------- Defaults for the settings page (stored in flash once changed) ----------
#define DEFAULT_BRIGHTNESS  200    // OLED contrast 1-255
#define DEFAULT_DIM_SEC       8    // dim the idle panda after this many seconds (0 = never)
#define DEFAULT_SLEEP_MIN     5    // deep sleep after this many minutes with nothing playing (0 = never)
#define DEFAULT_SCROLL        3    // title scroll speed 1 (slow) - 5 (fast)
#define DEFAULT_SHOW_ART   true    // black-and-white album art thumbnail
#define DIM_BRIGHTNESS        1    // contrast used when dimmed
