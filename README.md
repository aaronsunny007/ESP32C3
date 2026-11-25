# ESP32-C3 Spotify "Now Playing" Bag Clip

A wearable clip that shows the song playing on Spotify on a 0.96" OLED. Tap once to play/pause, tap twice to skip, and it sleeps after 5 minutes of silence (tap to wake). Built on an ESP32-C3 SuperMini with a TTP223 touch sensor, powered by a 500 mAh LiPo and a TP4056 USB-C charger.

## What's here

| Path | What it is |
|---|---|
| [`PROJECT.md`](PROJECT.md) | Parts list, wiring, safety rules, pre-flight checklist, build status, and notes on where the firmware differs from the PDF guide |
| [`firmware/spotify_bag_clip_wifi/`](firmware/spotify_bag_clip_wifi/) | Main firmware (Wi-Fi or phone hotspot, works with any phone) |
| [`firmware/spotify_bag_clip_ble/`](firmware/spotify_bag_clip_ble/) | Optional Bluetooth LE version (Android only, no Spotify keys needed) |
| [`docs/Spotify_Bag_Clip_Complete_Guide_v6.pdf`](docs/Spotify_Bag_Clip_Complete_Guide_v6.pdf) | The original build guide |

## Quick start (Wi-Fi firmware)

1. Install the Arduino IDE with the ESP32 board package, and select **ESP32C3 Dev Module** with **USB CDC On Boot: Enabled**.
2. Install the libraries listed at the top of `spotify_bag_clip_wifi.ino`.
3. Copy `firmware/spotify_bag_clip_wifi/secrets.example.h` to `secrets.h` in the same folder and fill in your Spotify client ID, secret and refresh token. `secrets.h` is gitignored, so your keys stay off GitHub.
4. Flash, then work through the pre-flight stages in `PROJECT.md`.

Pins: OLED SDA → GPIO8, SCL → GPIO9, TTP223 I/O → GPIO3. Battery goes to the **5V** pin only, never 3.3V.

> The firmware has been reviewed but not yet compiled on real hardware. See the Status table in `PROJECT.md`.
