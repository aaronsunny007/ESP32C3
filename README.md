# ESP32-C3 Spotify "Now Playing" Bag Clip

A wearable clip that shows the song playing on Spotify on a 0.96" OLED, with black-and-white album art, what's next in the queue, elapsed/total time and a battery gauge. Tap once to play/pause, twice to skip, three times to go back. When nothing plays it shows a panda, dims, and then sleeps (tap to wake). Your phone pairs over Bluetooth to set up Wi-Fi and settings; the clip uses Wi-Fi (home or phone hotspot) to talk to Spotify and for over-the-air updates.

Built on an ESP32-C3 SuperMini with a TTP223 touch sensor, powered by a 500 mAh LiPo and a TP4056 USB-C charger.

## What's here

| Path | What it is |
|---|---|
| [`PROJECT.md`](PROJECT.md) | Parts, wiring (including the battery divider), safety rules, Bluetooth/Wi-Fi split, setup, settings, pre-flight checklist and status |
| [`firmware/bag_clip/`](firmware/bag_clip/) | The firmware |
| [`phone/index.html`](phone/index.html) | Phone setup page over Bluetooth (Chrome on Android, Bluefy on iPhone) |
| [`tools/png_to_bitmap.py`](tools/png_to_bitmap.py) | Turn any picture into the idle-screen bitmap |
| [`docs/Spotify_Bag_Clip_Complete_Guide_v6.pdf`](docs/Spotify_Bag_Clip_Complete_Guide_v6.pdf) | The original build guide |

## Quick start

1. Arduino IDE with the ESP32 core 3.x: board **ESP32C3 Dev Module**, **USB CDC On Boot: Enabled**, Partition Scheme **Minimal SPIFFS (1.9MB APP with OTA)**.
2. Libraries: Adafruit SSD1306, Adafruit GFX, ArduinoJson 7.x, NimBLE-Arduino 2.x, TJpg_Decoder.
3. Copy `firmware/bag_clip/secrets.example.h` to `secrets.h` and fill in your Spotify keys and an admin password. `secrets.h` is gitignored.
4. Flash, open the phone page, connect to **BagClip**, type the PIN shown on the clip, and add your Wi-Fi / hotspot.

Pins: OLED SDA → GPIO8, SCL → GPIO9, TTP223 I/O → GPIO3, battery divider (2 × 100 kΩ) → GPIO4. Battery goes to the **5V** pin only, never 3.3V.

> Every push is compiled on GitHub (Actions → Build firmware). It hasn't been tested on real hardware yet; see the Status table in `PROJECT.md`.
