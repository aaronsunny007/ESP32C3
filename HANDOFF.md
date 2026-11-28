# Handoff: where we left off (27 Nov 2026, evening)

Paste this into a new chat (or say "read HANDOFF.md and PROJECT.md in aaronsunny007/esp32c3") to continue.
**Scope: the clip and its 0.96" 128x64 one-colour OLED only.** No web pages unless the user asks for one.

## Project
ESP32-C3 "Now Playing" bag clip: 0.96" OLED shows the song playing on the phone; tap pad = play/pause/skip. **v3 = Bluetooth only** + a small Android app (`phone-app/`). Wi-Fi is an optional build flag (off). Source of truth: `PROJECT.md`. Repo: `aaronsunny007/esp32c3`, default branch `claude/project-thread-q67a4a`. PRs #1-#4 merged (#4 = birthday greeting).

## Latest work (27 Nov)
- **Birthday greeting at switch-on** (merged, PR #4): panda in its cap + "Hpy bday Kusuma." -> tap -> the paw takes the cap off -> "From your lovely panda" -> tap -> normal clip. Edit text/animation in `tools/make_oled_panda.py`; `GREETING_ENABLED` in `config.h`.
- **Branch `claude/clip-menu` (NOT merged yet, builds green):**
  - Idle panda redrawn as the birthday panda (`tools/draw_panda.py` -> `gif_to_frames.py --rest 27`).
  - **Menu**: hold 1.5 s opens it; tap = next item, hold = choose, hold inside an item = back; closes after 30 s. Items: Music player, Dino game, Motivation (random line, edit `motivation.h`), Show greeting, Brightness, Info, Power off (deep sleep), Exit. Code: `menu.cpp`, `dino.cpp`, `motivation.h`, drawing in `ui.cpp`.
  - **Music player screen** (redesigned): title + artist on top, panda with headphones (`music_img.h` from `tools/make_music_panda.py`, eyes closed and nodding while playing), spinning record, big buttons, progress line, hint alternating with "hold: back to menu".
  - Next step: user says "merge" -> open PR into `claude/project-thread-q67a4a` and merge when CI is green.
- **Screen previews without hardware:** compile `firmware/bag_clip/ui.cpp` (+ menu/dino) on Linux against Adafruit-GFX with stub Arduino/SSD1306 headers that dump the 128x64 buffer (rebuild the stubs if needed; min/max stubs must return by value).

## Decisions made
- **v3 (Bluetooth-only)** chosen over v2 (Wi-Fi) after a council review. User's phone: **Google Pixel** (stock Android 13+), so v3 works and no background-killer settings are needed.
- **Bench test with a breadboard** (option 1), not the one-part-at-a-time workaround.

## Fixes done (firmware 3.0.1, all pushed)
App update stall after disconnect; PIN-only BLE pairing (unpaired links dropped after 45 s); listener `requestRebind`; `BATT_FITTED` flag (0 until the resistors are fitted); Wi-Fi characteristic created last; stuck-pad sleep loop; settings validation; unknown track length; app connection/MTU/Location fixes; **1N5819 diode** between switch and ESP32 5V (stops USB back-feeding the LiPo); **fixed app signing key** (updates install over the old app); power-LED removal step in docs. Both firmware builds compile (core 3.1.1, NimBLE 2.1.0); GitHub Actions builds firmware + APK green.

## Parts
**Ordered (₹868):** ESP32-C3 SuperMini HW-466AB (headers loose, must solder), blue 0.96" I2C OLED, red TTP223 (leave A/B pads open), TP4056 Type-C (check 6 pads), 450 mAh 602040 LiPo (40 mm long), SPDT slide switch, 4 mm heat-shrink, Walkers 10-in-1 soldering kit.

**Still to buy:** 400-point breadboard; jumper wire set with M-M, M-F, F-F; 1N5819 diodes; 2 × 100 kΩ resistors; metal badge clip; double-sided foam tape; Kapton/electrical tape (if kit has none); flux (if kit has none); optional 100 nF cap and multimeter.

## Guides
- 3D build guide (private): https://claude.ai/artifact/6hmrLLUdS83oHmUgmjvU4m — A bench test, B final soldered wiring, C assembly; app install; soldering basics.
- App install: Chrome on Pixel, sign in to GitHub → Actions → latest green "Build firmware and app" → artifact **bag-clip-app** → Files → Extract → install `app-debug.apk` → Settings → Apps → Bag Clip → ⋮ → Allow restricted settings → in app: Allow Bluetooth, Allow reading what's playing (skip Find my clip for now).

## Bench test wiring (Stage A, 9 wires)
ESP32 pins (USB-C at top) — left: 5V, G, 3.3, 4, 3, 2, 1, 0 · right: 5, 6, 7, 8, 9, 10, 20, 21.

| # | From | To | Wire |
|---|---|---|---|
| 1 | next to ESP32 3.3 | red + strip | M-M |
| 2 | next to ESP32 G | blue − strip | M-M |
| 3 | red + strip | OLED VCC | M-F |
| 4 | blue − strip | OLED GND | M-F |
| 5 | next to ESP32 8 | OLED SDA | M-F |
| 6 | next to ESP32 9 | OLED SCL | M-F |
| 7 | red + strip | TTP223 VCC | M-F |
| 8 | blue − strip | TTP223 GND | M-F |
| 9 | next to ESP32 3 | TTP223 I/O | M-F |

Then flash `firmware/bag_clip` (ESP32C3 Dev Module, USB CDC On Boot Enabled, Minimal SPIFFS, `BATT_FITTED 0`): expect "Bag Clip v3.0.1" → "Set me up!".

## Open / next
1. User still finds the wiring confusing: last offer was a **simple flat top-down 2D drawing** (photo-like breadboard view, big labels) added to the guide. Ask which part is unclear (pin names, breadboard strips, or how wires plug in).
2. User to buy the missing parts and do the **one-hour app test** on the Pixel (song still shown after an hour locked).
3. When parts arrive: solder ESP32 headers → Stage A bench test → pre-flight stages in `PROJECT.md`.
