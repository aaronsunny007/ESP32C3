# ESP32-C3 Spotify "Now Playing" Bag Clip

A wearable clip that shows the song playing on your phone on a 0.96" OLED, with black-and-white album art, what's next, elapsed/total time and a battery gauge. Tap once to play/pause, twice to skip, three times to go back. When nothing plays it shows a small animated panda (or any GIF you convert with `tools/gif_to_frames.py`), dims, and then sleeps (tap to wake).

**Bluetooth only:** the clip connects to your Android phone over Bluetooth Low Energy. No Wi-Fi, no phone hotspot, no Spotify keys. The Bag Clip app on the phone sends the song (from Spotify or any music app) and handles the play/skip taps. Wi-Fi (OTA updates, web page) is an optional build setting, off by default. iPhone can't run the companion app; see `PROJECT.md`.

Built on an ESP32-C3 SuperMini with a TTP223 touch sensor, powered by a 450 mAh LiPo and a TP4056 USB-C charger.

## What's here

| Path | What it is |
|---|---|
| [`PROJECT.md`](PROJECT.md) | Parts, wiring (including the battery divider), safety rules, how the phone link works, app setup, Bluetooth protocol, settings, pre-flight checklist and status |
| [`firmware/bag_clip/`](firmware/bag_clip/) | The firmware |
| [`phone-app/`](phone-app/) | The Bag Clip Android app (Kotlin) |
| [`phone/index.html`](phone/index.html) | Optional settings page in the browser over Bluetooth |
| [`tools/png_to_bitmap.py`](tools/png_to_bitmap.py) | Turn any picture into the idle-screen bitmap |
| [`docs/Spotify_Bag_Clip_Complete_Guide_v6.pdf`](docs/Spotify_Bag_Clip_Complete_Guide_v6.pdf) | The original build guide |

## Quick start

1. Arduino IDE with the ESP32 core 3.x: board **ESP32C3 Dev Module**, **USB CDC On Boot: Enabled**, Partition Scheme **Minimal SPIFFS (1.9MB APP with OTA)**.
2. Libraries: Adafruit SSD1306, Adafruit GFX, NimBLE-Arduino 2.x. Open `firmware/bag_clip/bag_clip.ino` and upload. No `secrets.h` needed.
3. On GitHub, **Actions → Build firmware and app →** latest run → download **bag-clip-app**, and install `app-debug.apk` on your Android phone.
4. In the app: allow Bluetooth, allow reading what's playing, then **Find my clip** and type the PIN shown on the clip.
5. Play a song.

Pins: OLED SDA → GPIO8, SCL → GPIO9, TTP223 I/O → GPIO3, battery divider (2 × 100 kΩ) → GPIO4. Battery goes to the **5V** pin only (through a 1N5819 diode, stripe towards the ESP32), never 3.3V.

> Every push is built on GitHub (firmware in both modes, plus the APK). Nothing has been tested on real hardware or a phone yet; see the Status table in `PROJECT.md`.
