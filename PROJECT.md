# ESP32-C3 Spotify "Now Playing" Bag Clip

A wearable clip that shows the song playing on your phone on a small OLED screen, with album art, what's playing next and a battery gauge. Tap once to play/pause, twice to skip, three times to go back. When nothing is playing it shows a little animated panda (blinks, waves, eats bamboo, dozes off), dims, and then sleeps (tap to wake). Runs on a rechargeable LiPo.

**Since v3 the clip talks to your phone over Bluetooth only.** No Wi-Fi, no phone hotspot, no Spotify developer keys. A small Android app (`phone-app/`) sends the song to the clip and plays/pauses/skips when you tap the clip. Wi-Fi is still in the firmware as an optional build setting, off by default.

Source of truth for the hardware: *Spotify Bag Clip Complete Guide v6* (in `docs/`). This file summarises it, tracks progress, and records where the firmware differs from the PDF.

## Status

| Stage | State |
|---|---|
| Project file + firmware v1 saved | Done (25 Nov 2026) |
| Firmware v2 (Bluetooth setup, battery, panda, album art, queue, settings, OTA) | Written 25 Nov 2026, superseded by v3 |
| Firmware v3: Bluetooth only + Android app (Wi-Fi optional, off) | Written 25 Nov 2026. Firmware and APK build on GitHub (see "Build check"), **not yet tested on hardware or a phone** |
| Animated idle panda + GIF converter (`tools/gif_to_frames.py`) | Written 25 Nov 2026. Compile checked on GitHub, **not yet seen on a real OLED** |
| Parts ordered | Main parts ordered 24–25 Nov 2026 (see "Parts"). **Still to buy:** breadboard, jumper wires (M-M + M-F), 1N5819 diode, 2 × 100 kΩ resistors, badge clip, foam tape |
| Pre-flight stage 1: USB & board | To do |
| Pre-flight stage 2: OLED & touch | To do |
| Pre-flight stage 3: install app, pair, song shows on the clip | To do |
| Pre-flight stage 4: touch, dim & sleep | To do |
| Pre-flight stage 5: battery smoke test + battery level | To do |
| Enclosure + final assembly | To do |

**Next step:** buy the missing parts (breadboard, jumper wires, diode, resistors, clip, foam tape; see "Parts → Still to buy"). Meanwhile, install the Android app from the latest GitHub build on the Pixel and check it still shows the song after an hour with the phone locked. When the parts arrive, solder the SuperMini's headers and flash `firmware/bag_clip` for pre-flight stages 1–2.

## How the clip talks to your phone (Bluetooth only)

v2 needed Wi-Fi (usually your phone's hotspot) because the clip asked Spotify's servers for the song itself. A hotspot keeps the phone's Wi-Fi radio and mobile data busy all day, which drains the phone. v3 turns it around: **the phone already knows what's playing, so it tells the clip over Bluetooth Low Energy**, which uses a tiny fraction of the power.

```
 Spotify / YT Music / any player          Bag Clip app (Android)                 Clip (ESP32-C3)
 ───────────────────────────────          ──────────────────────                 ───────────────
 media session: title, artist,  ───────►  reads it (notification access) ──BLE──► OLED: song, art,
 position, cover, queue                   shrinks cover to 32×32 1-bit            progress, next up
                                ◄───────  play / pause / next / previous ◄──BLE── taps on the pad
```

| Job | How |
|---|---|
| Song title, artist, next up, progress, play/pause state | App → clip over BLE, on every change, plus a position re-sync every 30 s while playing (the clip counts along in between) |
| Album art | App shrinks the cover to a 32×32 black-and-white thumbnail (128 bytes) and sends it once per song |
| Play / pause / next / previous | Tap the clip → BLE notification → app presses the button on whatever is playing |
| Settings (brightness, dim, sleep, scroll speed, album art) | App (or the phone page) → clip over BLE |
| Phone battery % | App → clip, shown on the info screen |
| Pairing | Once, with the 6-digit PIN shown on the clip. Everything after that is encrypted |
| Firmware updates | USB cable (or Wi-Fi mode, below) |

What you gain: no hotspot, no Spotify developer app or keys, no Premium needed for the controls (they go through the Spotify app on your phone, so Free works with its usual skip limits), works with **any** music app, and both the phone and the clip use less battery.

What you give up: the phone needs the Bag Clip app, and it has to be within Bluetooth range (a few metres; fine in a bag).

**Battery on the clip.** With Wi-Fi off the CPU runs at 80 MHz, and after pairing the clip asks for a slow connection (an exchange every 100–150 ms; the clip may skip up to 4 of them when idle). The OLED is now one of the bigger loads. When nothing has played for the sleep time (default 5 min) the clip goes into deep sleep **even if the phone is connected**; tap it and the app reconnects by itself within a second or two. Real numbers still need measuring on the hardware.

### iPhone

**iPhone can't do this.** iOS doesn't let an app read what other apps (Spotify, Apple Music) are playing, and doesn't let it press their buttons, so there is no way to write the companion app for iPhone. (Apple's own protocol for this, AMS, works for Bluetooth accessories, but it's a different design; noted as a possible future step.) On an iPhone the options are Wi-Fi mode with Spotify keys (below), or the v2 firmware from git history.

### Optional Wi-Fi mode (off by default)

Set `WIFI_MODE 1` at the top of `firmware/bag_clip/config.h` to compile Wi-Fi back in. It then:
- joins a saved Wi-Fi network (added from the phone page over Bluetooth, up to 5),
- serves the settings page at http://bagclip.local and accepts over-the-air firmware updates,
- if you fill in Spotify keys in `secrets.h`, reads Spotify directly whenever the phone app isn't connected (the v2 behaviour: album art from Spotify, queue, Premium needed for controls). With the keys left empty, Wi-Fi is only used for OTA and the web page.

Wi-Fi mode needs `secrets.h` (copy `secrets.example.h`; at least change `ADMIN_PASSWORD`) and the ArduinoJson and TJpg_Decoder libraries. It uses a lot more battery on the clip, so turn it on only when you need it.

## Files

```
esp32c3/
├── README.md                 ← quick start
├── PROJECT.md                ← this file
├── .gitignore                ← keeps secrets.h and build output out of git
├── .github/workflows/build.yml ← builds the firmware (both modes) and the Android app on every push
├── docs/
│   ├── Spotify_Bag_Clip_Complete_Guide_v6.pdf
│   └── images/panda_idle.gif, panda_idle_preview.gif, panda_poses.png
├── firmware/bag_clip/        ← the firmware (one Arduino sketch, several tabs)
│   ├── bag_clip.ino          ← setup/loop, touch, sleep, sending taps to the phone
│   ├── config.h              ← pins, timings, defaults, WIFI_MODE switch  (edit for hardware changes)
│   ├── ble_link.cpp/.h       ← Bluetooth service: song in, taps out, settings, status
│   ├── ui.cpp/.h             ← every screen, icons, scrolling
│   ├── panda_anim.h          ← idle-screen animation frames, generated (swap for your own GIF)
│   ├── battery.cpp/.h        ← battery voltage and %
│   ├── settings.cpp/.h       ← settings (and Wi-Fi list) stored in flash
│   ├── state.cpp/.h          ← shared state between tasks
│   │   — only compiled in Wi-Fi mode —
│   ├── wifi_link.cpp/.h      ← Wi-Fi connection and the Spotify polling task
│   ├── spotify.cpp/.h        ← Spotify Web API (now playing, queue, controls, art)
│   ├── art.cpp/.h            ← album art JPEG → black-and-white thumbnail
│   ├── web_portal.cpp/.h     ← settings web page + OTA
│   ├── certs.h               ← root certificates for Spotify's HTTPS
│   └── secrets.example.h     ← copy to secrets.h (Wi-Fi mode only)
├── phone-app/                ← the Bag Clip Android app (Kotlin, no extra libraries)
│   └── app/src/main/java/com/bagclip/app/
│       ├── MainActivity.kt   ← setup steps, status, clip settings
│       ├── MediaListener.kt  ← reads what's playing, handles taps from the clip
│       ├── ClipLink.kt       ← Bluetooth connection to the clip
│       └── ArtDither.kt      ← cover → 32×32 black-and-white thumbnail
├── phone/index.html          ← optional settings page in the browser (Web Bluetooth)
└── tools/
    ├── gif_to_frames.py      ← turn any GIF into panda_anim.h (the idle animation)
    ├── png_to_bitmap.py      ← turn a still picture into an OLED bitmap
    └── draw_panda.py         ← draws the default animated panda (docs/images/panda_idle.gif)
```

The two v1 sketches (`spotify_bag_clip_wifi`, `spotify_bag_clip_ble`) and the Wi-Fi-first v2 are in git history.

## Parts

### Ordered (24–25 Nov 2026, not yet delivered)

| Part | What was ordered | ₹ | Check when it arrives |
|---|---|---|---|
| ESP32-C3 SuperMini | ESP32-C3 Super Mini Development Board, HW-466AB | 248 | The pin headers come loose: solder them on (short side into the board) before the jumper-wire bench test |
| 0.96" OLED | OLED Display 0.96 inch I2C / 4 pin, blue, SSD1306 | 144 | 4-pin I2C. Wire by the pin labels printed on *your* board (GND/VCC order varies). Shows everything in blue |
| TTP223 touch sensor | Red TTP223 1-channel capacitive touch sensor module | 9 | Leave the **A** and **B** solder pads open (the default). Bridging them changes the output mode and breaks tap detection and wake-from-sleep |
| TP4056 charger | TP4056 battery charging protection module (Type-C) | 18 | Must have **6 pads** (B+, B−, OUT+, OUT− plus USB). A 4-pad board has no battery protection: don't use it with the LiPo |
| LiPo battery | 3.7 V 450 mAh Li-Po, 602040 | 177 | 602040 = about 6 × 20 × 40 mm: 10 mm longer than planned and longer than the OLED board (~27 mm), so the case gets a little longer or the cell sits under the ESP32. Runtime about the same as 500 mAh |
| Slide switch | SPDT miniature slide switch | 6 | 3 pins |
| Heat-shrink | Heat shrink sleeve tube, 4 mm, black, 1 m | 15 | |
| Soldering kit | Walkers Beginners 10-in-1 mobile soldering kit | 251 | Check it has solder wire and flux; if there's no flux, buy a small tin of flux paste |

Ordered so far: **₹868**.

### Still to buy

| Part | Search for | Qty | Why | ₹ |
|---|---|---|---|---|
| **Schottky diode 1N5819** (or SS14 / SS34 if you're happy with SMD) | "1N5819 Schottky diode" | pack of 5–10 | Stops USB on the ESP32 back-feeding the battery (Step B). 1 A, through-hole; the stripe marks the cathode | 5–10 |
| **2 × 100 kΩ resistors** | "100K ohm 1/4W resistor" (1% metal film if possible) | 2 (a pack) | Battery level reading (Step B2) | 5–10 |
| Breadboard | "400 point half size breadboard" | 1 | The ESP32 has one `3.3` and one `G` pin but the OLED and touch sensor both need them: the breadboard's + / − strips let them share (Step A) | 50–80 |
| Jumper wires | "jumper wires 3 types male male female 40 pin" (one set each of M-M, M-F, F-F) | 1 set | M-M from ESP32 to the + / − strips, M-F from the breadboard to the OLED and sensor pins. Female-to-female alone can't do the bench test | 80–150 |
| Badge clip | "metal alligator badge clip" | 1 | Attaches the clip to the bag | 10–30 |
| Double-sided foam tape | "double sided foam tape" | 1 | Holds the sandwich stack together (Step C) | 20–50 |
| Kapton or electrical tape | "kapton tape" / "electrical insulation tape" | 1 | Wraps the LiPo (skip if the soldering kit has tape) | 20–60 |
| Flux paste | "soldering flux paste" | 1 | Only if the soldering kit has none | 30–60 |
| 100 nF capacitor (optional) | "104 ceramic capacitor" | 1 | Steadier battery reading | 2 |
| Multimeter (optional, recommended) | "digital multimeter DT830" | 1 | Battery calibration (`BATT_CAL`) and checking sleep current after removing the power LED (Step B3) | 150–250 |

Stages 1–2 of the pre-flight checklist only need the board, OLED, touch sensor, breadboard and jumper wires. Step-by-step 3D build guide (bench test, soldered wiring, assembly, soldering basics): https://claude.ai/artifact/6hmrLLUdS83oHmUgmjvU4m (private to the repo owner unless shared). The diode and resistors aren't needed until stage 5.

## Safety rules (read before wiring)

1. **Battery goes to the 5V pin only.** TP4056 OUT+ (through the switch and the diode) → ESP32-C3 **5V**. Never 3.3V: a full LiPo is 4.2 V and will damage the 3.3 V rail.
2. **Never wire the battery straight to GPIO4.** It must go through the two resistors; 4.2 V on a GPIO pin will damage the chip.
3. **Check the OLED pin labels.** Some print GND, VCC, SCL, SDA and others VCC, GND, SCL, SDA. Wire by the text on *your* board.
4. **Fit the diode (Step B).** It stops USB on the ESP32 from feeding back into the LiPo. Without it, the switch must be OFF before plugging USB-C into the ESP32-C3. Check the stripe direction before soldering: stripe towards the ESP32.

## Wiring

### Step A: bench test with jumper wires (USB powered, no solder)

| ESP32-C3 | OLED | TTP223 | Purpose |
|---|---|---|---|
| 3.3V | VCC | VCC | Power (share via breadboard / Y-splice) |
| GND | GND | GND | Common ground |
| GPIO 8 | SDA | — | I2C data |
| GPIO 9 | SCL | — | I2C clock |
| GPIO 3 | — | I/O (SIG) | Touch input + deep-sleep wake (must be GPIO 0–5) |

For this bench test leave `BATT_FITTED 0` in `config.h` (the default): with no battery resistors fitted the clip hides the battery icon instead of reading the empty pin as a flat battery.

### Step B: battery and charging (10 solder joints)

| From | To | Notes |
|---|---|---|
| LiPo red (+) | TP4056 B+ | |
| LiPo black (−) | TP4056 B− | |
| TP4056 OUT+ | Switch centre pin | |
| Switch outer pin | Diode **anode** (the end *without* the stripe) | |
| Diode **cathode** (the end *with* the stripe) | ESP32-C3 **5V** | Feeds the onboard 3.3 V regulator |
| TP4056 OUT− | ESP32-C3 GND | |

```
 LiPo ── TP4056 ── OUT+ ── switch ──┬──►|── ESP32-C3 5V pin ── (USB-C on the ESP32 also lands here)
                                    │  1N5819
                                    └── battery divider (Step B2)
```

**Why the diode:** the ESP32's 5V pin is wired straight to its USB-C socket. Without the diode, plugging a cable into the ESP32 while the switch is on pushes USB's 5 V backwards into the TP4056 output and the LiPo. The Schottky diode only lets current flow from the battery to the board, never back, so it's safe to plug USB into either socket with the switch in any position. Put heat-shrink over it.

The price is ~0.3 V lost across the diode, so the board sees about 3.4 V from a half-empty battery instead of 3.7 V. That's still enough for the regulator; the clip now stops at 3.5 V battery instead of 3.45 V.

### Step B2: battery level (2 resistors, 4 solder joints)

A voltage divider halves the battery voltage (max 4.2 V → 2.1 V) so the ESP32-C3's ADC can read it. It goes on the **battery side of the diode** (the switch's outer pin), so it measures the real battery voltage, not the diode's output or USB.

```
 Switch outer pin (= battery + when switched on; same pad as the diode's anode)
        │
      [R1 100 kΩ]
        │
        ├──────────── GPIO 4   (optional: 100 nF capacitor from GPIO 4 to GND)
        │
      [R2 100 kΩ]
        │
       GND
```

| From | To |
|---|---|
| Switch outer pin (same pad as the diode's anode) | R1, one leg |
| R1, other leg | **GPIO 4** and R2, one leg |
| R2, other leg | ESP32-C3 **GND** |

- Taking it after the switch means the divider draws nothing when the clip is switched off. While on, it draws ~20 µA, which is small next to the clip itself.
- Switch off and USB in the ESP32: the divider has no power, so the clip simply hides the battery icon. Switch on and USB in: the board runs from USB (the diode blocks the battery) and the icon shows the battery's charge.
- **Switch the reading on:** set `BATT_FITTED 1` in `config.h` and flash again. Until then the clip ignores GPIO 4 (no battery icon, no low-battery shutdown).
- **Calibrate once:** hold the touch pad for 1.5 s to open the menu and choose Info, compare "Batt x.xx V" with a multimeter on the battery, and set `BATT_CAL` in `config.h` to *real ÷ shown* (e.g. 4.02 / 3.95 = 1.018).
- Below ~3.5 V (battery) the board's regulator starts to brown out after the diode drop, so the clip shows "Battery low / Charge or switch off" and sleeps. **Switch it off** (or charge it): even asleep the board keeps drawing a little, and a flat LiPo left on keeps draining.
- Using different resistors: set `BATT_DIVIDER` in `config.h` to (R1 + R2) ÷ R2. Keep the pin voltage under 2.5 V.

### Step B3: remove the power LED (recommended)

The SuperMini's red power LED is on whenever the board has power, **including deep sleep**, and draws about 1 mA. That's more than the rest of the sleeping clip and empties the 450 mAh battery in about two weeks of "sleep". Remove it: hold the iron on the LED (or its tiny resistor next to it) for a second and flick it off, or cut its resistor with a craft knife. The blue LED on GPIO 8 is separate and only flickers with OLED traffic. After this, measure the current with a multimeter in series with the switch: asleep it should be well under 1 mA. For storage, always slide the switch off.

### Step C: sandwich stack

1. Front: OLED facing out.
2. Middle: LiPo wrapped in Kapton/electrical tape, foam-taped to the OLED's back.
3. Back: ESP32-C3 + TP4056 on short 3–4 cm wires, resistors heat-shrunk along the wires. Keep the ESP32 antenna tip away from metal.
4. TTP223 glued flat against the inside wall of the case (senses through 2–3 mm plastic). Clip or N52 magnets epoxied to the rear.

## Software setup

### Firmware

**Arduino IDE** (Tools menu):
- Board: **ESP32C3 Dev Module** (ESP32 core **3.x**)
- USB CDC On Boot: **Enabled**
- Partition Scheme: **Minimal SPIFFS (1.9MB APP with OTA/190KB SPIFFS)** (needed for Wi-Fi mode's OTA; harmless otherwise, so keep it for both)

**Libraries** (Library Manager):

| Library | Version | Needed for |
|---|---|---|
| Adafruit SSD1306 + Adafruit GFX Library | latest | always |
| NimBLE-Arduino (h2zero) | **2.x** | always |
| ArduinoJson (Benoit Blanchon) | **7.x** | Wi-Fi mode only |
| TJpg_Decoder (Bodmer) | latest (1.1.x) | Wi-Fi mode only |

The default Bluetooth-only build needs **no `secrets.h`**, no Spotify keys and no passwords. Just open `firmware/bag_clip/bag_clip.ino` and upload.

### Phone app (Android 8 or newer)

Your phone: **Google Pixel** (stock Android 13+). No background-killer settings needed; do the *Allow restricted settings* step below.

1. **Get the APK.** On GitHub open **Actions → Build firmware and app**, click the latest green run, and download **bag-clip-app** at the bottom (a zip with `app-debug.apk` inside). On the phone: sign in to GitHub in Chrome first (artifact downloads need it), then **Files → Downloads → tap the zip → Extract**. Each build is kept for 90 days. Or build it yourself: open `phone-app/` in Android Studio and press Run.
2. **Install it.** Copy the APK to the phone and open it. Android asks to allow installing from that app (Files / Chrome): allow it. Play Protect may warn that it's an unknown app; choose *Install anyway*. **Updating later:** every build is signed with the same key (`phone-app/app/bagclip-debug.keystore`), so a newer APK installs straight over the old one and keeps all the permissions below. No uninstall needed.
3. **Open Bag Clip** and do the three steps on its screen:
   1. **Allow Bluetooth** (Android asks for "Nearby devices").
   2. **Allow reading what's playing**: switch on *Bag Clip* in the *Notification access* list. Android calls it that because media info comes through the same door; the app ignores your notifications' content. **Android 13+:** for apps installed from a file this switch is greyed out at first. Go to *Settings → Apps → Bag Clip → ⋮ (top right) → Allow restricted settings*, then switch it on.
   3. **Find my clip**: tap the clip to wake it, tap the button, and type the **6-digit PIN** the clip shows when your phone asks. (Android 8–11: switch **Location** on in quick settings while searching; those versions only find Bluetooth devices with Location on.)
4. Optional but recommended: **Let it run in the background** → set Bag Clip to *Unrestricted*. Xiaomi/Redmi/POCO, Oppo/Realme, Vivo and some Samsung phones kill background apps aggressively; on Xiaomi also enable *Autostart*.
5. Play a song in any app. Title, artist, album art and progress appear on the clip within a second or so.

**Before the hardware arrives (one-hour test):** do steps 1–3.2 (skip *Find my clip*), play a song, check the app's screen shows it, lock the phone for an hour, then open the app again: it should still show the right song. That proves the phone half works.

The app keeps running in the background on its own (Android keeps apps with notification access alive), reconnects whenever the clip wakes up, and follows whichever app is playing.

### Bluetooth protocol (for nRF Connect or your own app)

Service `b4c1a000-6f1a-4a8e-9c3e-2f5d7a0e1c00`. All characteristics need the PIN pairing first (plain "Just Works" pairing is refused), and a connection that hasn't paired within 45 s is dropped. Text is UTF-8.

| Characteristic | Direction | Format |
|---|---|---|
| `b4c1a002…` Settings | read / write | `brightness=200;dim=8;sleep=5;scroll=3;art=1` (any subset) |
| `b4c1a003…` Control | phone → clip | `toggle`, `play`, `pause`, `next`, `prev`, `refresh` (the clip passes them on to the app) |
| `b4c1a004…` Status | read / notify | `fw=`, `mode=`, `bat=`, `mv=`, `usb=`, `app=`, `playing=`, `title=` … lines |
| `b4c1a005…` Now playing | phone → clip | newline-separated `key=value`, any subset: `title`, `artist`, `next`, `dur` (ms, 0 = unknown length), `pos` (ms), `play` (1/0), `app`, `pbat` (%), or `clear=1` for nothing playing. Up to 512 bytes |
| `b4c1a006…` Album art | phone → clip | exactly 128 bytes: 32×32, 1 bit per pixel, rows top to bottom, MSB = leftmost, 1 = lit. An empty write (0 bytes) clears the art; longer writes are rejected |
| `b4c1a007…` Buttons | clip → phone (notify) | `toggle`, `next`, `prev`, `refresh` when the pad is tapped. Subscribing marks the app as linked |
| `b4c1a001…` Wi-Fi | read / write | Wi-Fi mode only: `add` ⏎ `SSID` ⏎ `password`, or `del` ⏎ `SSID` |

### Browser settings page (optional)

`phone/index.html` uses Web Bluetooth (Chrome on Android or a laptop) to show the clip's status and change settings, and in Wi-Fi mode to add Wi-Fi networks. Browsers only allow Bluetooth on `https://` pages, e.g. GitHub Pages. The app does the same job, so you only need this on a laptop or in Wi-Fi mode.

### Wi-Fi mode extras: web page and OTA updates

Only with `WIFI_MODE 1`. Once on Wi-Fi the clip serves **http://bagclip.local** (log in as `admin` with `ADMIN_PASSWORD`) with settings, the Wi-Fi list, play buttons and a firmware upload box. Arduino IDE can also upload over the air (**Tools → Port → bagclip at 192.168.x.x**). GitHub builds attach both `.bin` files (`bag_clip-firmware-ble` and `-wifi`).

To read Spotify directly (no app connected), fill in `secrets.h`: create an app at developer.spotify.com/dashboard with redirect URI `http://127.0.0.1:8888/callback`, get a refresh token with scopes `user-read-currently-playing user-read-playback-state user-modify-playback-state`, and paste the client ID, secret and token. Controls then need Spotify Premium.

### Settings

| Setting | Default | Range | What it does |
|---|---|---|---|
| Brightness | 200 | 1–255 | OLED contrast |
| Dim after | 8 s | 0–3600 (0 = never) | Dims the panda screen when nothing has happened for this long |
| Sleep after | 5 min | 0–240 (0 = never) | Deep sleep when nothing is playing, even while the phone is connected. Tap to wake; the app reconnects by itself |
| Scroll speed | 3 | 1–5 | Long titles scroll at 12 / 20 / 30 / 42 / 60 pixels per second |
| Album art | on | on/off | 32×32 black-and-white thumbnail |

Defaults live in `config.h`; changes from the app, the phone page or the web page are saved in flash.

### The idle panda animation

When nothing is playing, the left half of the screen shows a 64×64 panda that loops through a ~16 s routine: looks around and blinks, waves, eats a stick of bamboo, then dozes off with floating z's. The text on the right ("Nothing playing", "Paused", ...) stays as before.

![panda animation, 4x size](docs/images/panda_idle_preview.gif)

![the 11 pictures](docs/images/panda_poses.png)

How it runs on the clip:

- **Pictures in flash, not RAM.** `firmware/bag_clip/panda_anim.h` holds each different picture once (11 × 512 bytes = 5.6 KB of flash) plus a play list of (picture, milliseconds). Repeats like "blink, open, blink" reuse the same picture. RAM use is 2 variables.
- **Non-blocking.** No `delay()`: each pass of `loop()` checks whether the current step's time is up. The screen is only redrawn when the picture actually changes (at most ~5 times a second, during the wave) or once a second for the battery/status text, so Bluetooth and the touch pad are as responsive as before and the I²C bus is idle most of the time.
- **Dims and stops.** After the *Dim after* time (default 8 s) the screen dims and the animation freezes on the dozing "zzz" picture, redrawing only every 5 s. Any tap or new song wakes it up and the routine starts again. Deep sleep after the *Sleep after* time is unchanged.

### Your own animation

Any animated GIF works (`pip install pillow` first):

```
python3 tools/gif_to_frames.py my.gif --preview check.gif
```

That overwrites `firmware/bag_clip/panda_anim.h`; flash the sketch again and it plays. Open `check.gif` first to see exactly what the OLED will show (4× size). The script:

- shrinks the GIF to fit 64×64 (keeps its shape, centred on black) and keeps each frame's own timing;
- lights up the **bright** parts. For a dark drawing on a white background add `--invert`; for photos or shaded art add `--dither`; if it looks too thin or too blobby, try `--threshold 80` or `--threshold 160`;
- keeps at most 24 different pictures (`--max-frames`, 512 bytes each) so a long GIF doesn't fill the flash; it picks them evenly and stretches their timing;
- `--speed 0.5` plays it at half speed, `--rest N` picks the step that's frozen on screen while dimmed (default the first; the default panda uses `--rest 25`, the zzz picture).

Simple, high-contrast GIFs with a plain background look best: on a 1-bit 64×64 screen, cartoon outlines read well and detailed photos turn to noise. To change the default panda itself, edit `tools/draw_panda.py`, then run it and the converter:

```
python3 tools/draw_panda.py
python3 tools/gif_to_frames.py docs/images/panda_idle.gif --rest 25
```

## Editing the greeting

At switch-on the clip shows a birthday greeting before anything else, one tap per step:

1. The panda wearing its cap, **"Hpy bday Kusuma."**
2. Tap: the panda's paw reaches up, takes the cap off and lifts it away (about 1.2 s).
3. The panda without its cap, **"From your lovely panda"**.
4. Tap: the normal screens.

![greeting animation](docs/images/greeting_anim_preview.gif)

Everything about it is drawn by one script, `tools/make_oled_panda.py`, which writes `firmware/bag_clip/greeting_img.h`. Don't edit the `.h` by hand; change the script and run it again:

| To change | Edit, in `tools/make_oled_panda.py` |
|---|---|
| The words | `SCREEN_1_LINES`, `SCREEN_2_LINES` at the top (1–4 short lines each; the font shrinks to fit) |
| How the paw moves, how fast | `ANIMATION` at the top: each step says where the paw ends, where the cap ends, how many pictures and how many milliseconds each |
| The drawing | `panda()`, `hat()`, `paw()` |

```
python3 tools/make_oled_panda.py
```

Then look at `docs/images/greeting_1_preview.png`, `greeting_2_preview.png` and `greeting_anim_preview.gif` (enlarged OLED look) and flash `firmware/bag_clip` again.

- **Switch it off** without deleting anything: `GREETING_ENABLED 0` in `firmware/bag_clip/config.h`.
- While the greeting is showing, taps only move it on (nothing is sent to the phone). If the clip goes to sleep in the middle, a tap wakes it on the same step; it starts from the top only after switching off and on.
- The firmware side is small: `uiGreeting()` and `uiGreetingHatOff()` in `ui.cpp`, and the `greetStage` lines in `bag_clip.ino`.

## Screens

| Screen | When |
|---|---|
| Now playing | Status bar (play/pause icon, Bluetooth icon with an x when the phone isn't connected, battery %), album art on the left, title / artist / ► next-up scrolling on the right, elapsed and total time, progress bar |
| Paused | Same as now playing with a pause icon for 10 s, then the panda |
| Panda (idle) | Animated panda with "Nothing playing", "Paused", "Phone not connected" or "Open the Bag Clip app". Dims after the dim time, sleeps after the sleep time |
| Greeting | At switch-on: panda in its cap with "Hpy bday Kusuma.", tap, the panda's paw takes the cap off, then "From your lovely panda", tap, then the normal screens. Survives sleep; starts over only after switching off and on. See "Editing the greeting" |
| Set me up! | Never paired: open the app and tap Find my clip |
| Bluetooth PIN | While your phone pairs |
| Menu | Hold the pad 1.5 s. Tap moves to the next item, hold chooses it; inside an item, hold goes back. Closes by itself after 30 s untouched |
| Music player | Menu: the song, play/pause, previous and next; 1 tap play/pause, 2 taps next, 3 taps previous (the music plays on the phone) |
| Dino game | Menu: tap to jump over the cacti; the high score is kept until the clip is switched off |
| Motivation | Menu: one line at a time, tap for the next. Edit the lines in `firmware/bag_clip/motivation.h` |
| Brightness | Menu: tap through Low / Medium / High, hold to save |
| Info | Menu: version, phone link, which app is playing, phone battery, clip battery voltage |
| Power off | Menu: deep sleep straight away (tap to turn on). The slide switch cuts the power completely |
| Messages | Short pop-ups: "Next", "Previous", "Phone not connected", "Open the Bag Clip app" |
| Wi-Fi mode only | Connecting / no Wi-Fi / Spotify error screens, OTA progress bar |

## Pre-flight checklist (do in order, before closing the case)

- [ ] **1. USB & board:** plug in, select ESP32C3 Dev Module + USB CDC On Boot Enabled + Minimal SPIFFS partition, upload a blink/serial test. COM port shows up, upload clean.
- [ ] **2. OLED & touch:** jumper-wire OLED + TTP223, flash `bag_clip` (with `BATT_FITTED 0`, the default). OLED shows "Bag Clip v3.0.1" then "Set me up!"; TTP223 LED lights when touched; holding it 1.5 s opens the menu (tap to move, hold to choose).
- [ ] **3. App + Bluetooth:** install the app, do its three steps (PIN on the OLED), play a song in Spotify. Title, artist, album art and times within ~1 s; long titles scroll smoothly. Turn the phone screen off: the clip keeps updating. Hotspot and Wi-Fi can stay off.
- [ ] **4. Touch, dim & sleep:** 1 tap pauses/plays, 2 taps skip, 3 taps go back (on the phone, within ~0.5 s). Pause: animated panda after 10 s (blinks, waves, eats bamboo, smooth with no stutter), dims and freezes on the "zzz" picture after 8 s more, sleeps after 5 min. A tap while dimmed restarts the animation at full brightness. Tap → wakes, and the app reconnects within a few seconds.
- [ ] **5. Battery:** USB unplugged, solder LiPo + TP4056 + switch + diode (stripe towards the ESP32) + the two resistors, remove the power LED (Step B3), set `BATT_FITTED 1` and flash, switch ON. Nothing warm, boots on battery, battery % shows; calibrate `BATT_CAL` against a multimeter. TP4056 LED red while charging / blue when full. Then plug USB into the ESP32 with the switch ON: the clip keeps running and nothing gets warm (the diode is doing its job).

## Build check

There is no Arduino or Android toolchain in the cloud workspace, so `.github/workflows/build.yml` does it on GitHub for every push:
- **Firmware**, twice: the default Bluetooth-only build and the Wi-Fi mode build (ESP32 core 3.1.1, NimBLE-Arduino 2.1.0, ArduinoJson 7.2.1, TJpg_Decoder 1.1.0, Minimal SPIFFS partition). Each `.bin` is attached.
- **Android app**: `gradle assembleDebug` (Android Gradle Plugin 8.7, Kotlin 2.0, compileSdk 35, minSdk 26). The debug-signed APK is attached as `bag-clip-app`.

A green check means it compiles; it doesn't test behaviour on real hardware or a real phone.

## Firmware notes (v3)

- **Bluetooth only by default.** All Wi-Fi, HTTPS, Spotify, JPEG and web-server code is compiled out unless `WIFI_MODE 1`, which also frees a lot of RAM and flash.
- **Taps go to the phone.** The clip sends `toggle`/`next`/`prev` as a BLE notification; the app calls play/pause/skip on the active media session (or sends a media key if no player is open, which resumes the last one). The play icon flips immediately and the phone's next update confirms it.
- **Progress bar** is interpolated on the clip between updates; the app re-syncs the position on every play/pause/seek and every 30 s.
- **Album art** is dithered on the phone (same Floyd–Steinberg + contrast stretch as v2's on-clip decoder), so the clip just copies 128 bytes.
- **Text:** the app strips accents ("Beyoncé" → "Beyonce") and turns other non-ASCII characters into `?`, since the OLED font is ASCII only.
- **Low power:** 80 MHz CPU, slow BLE connection parameters after pairing, slower advertising (every 250–500 ms) while waiting for the phone, deep sleep after the sleep time even while connected. The app doesn't request its own connection speed, so the clip's slow setting stays in force.
- **Pairing:** LE Secure Connections with a random 6-digit PIN on the OLED. Every characteristic needs a PIN-paired link; pairing without the PIN is refused, and a connection that hasn't paired within 45 s is dropped (the clip keeps advertising until your phone is paired, so a stray device can't lock it out). Nobody nearby can see what you play or skip your songs.
- **Stuck pad:** if the pad is still pressed when the clip goes to sleep (squashed in the bag), it checks again every minute with the screen off instead of waking over and over.
- Carried over from v2: smooth time-based scrolling, tap queue, battery gauge and calibration, panda idle screen (now animated), settings in flash, sleep/wake on the touch pad, low-battery shutdown.

## Known risks / open questions

- **Not tested on hardware or a phone yet.** Library APIs (NimBLE-Arduino 2.x, ESP32 core 3.x) are pinned in the build check; older versions (NimBLE 1.x, core 2.x) won't compile.
- **Android only** for the Bluetooth-only mode (see "iPhone" above).
- **Background killing:** some Android skins stop background apps despite notification access. If the clip stops updating with the phone screen off, set the app's battery use to *Unrestricted* (and *Autostart* on Xiaomi). If Android does unbind the app's listener, it asks to be reconnected straight away, and again whenever you open the app.
- **Sideloaded app on Android 13+** needs *Allow restricted settings* before notification access can be switched on (see the app setup steps).
- **Next up** only shows when the playing app shares its queue with Android. Many apps do; Spotify may or may not depending on version. Everything else works without it.
- **Pairing a second phone / a new phone:** remove *BagClip* from the old phone's Bluetooth list and pair again from the new one. The clip keeps up to a few bonds.
- **Memory (Wi-Fi mode):** Bluetooth + Wi-Fi + HTTPS together use a lot of the C3's 400 KB RAM. The firmware keeps only one HTTPS connection open at a time. If you see resets with "alloc failed" on the serial monitor, turn album art off first.
- **Spotify certificates (Wi-Fi mode):** `certs.h` bundles 7 common root CAs. If every request fails with a TLS error, set `SPOTIFY_TLS_INSECURE 1` in `secrets.h` for a bench test and report it.
- **Battery icon accuracy:** voltage-based, ±10 %. It reads a bit low under load (Wi-Fi mode especially) and a bit high while charging.
- **GPIO 8 is the SuperMini's onboard LED and a boot strapping pin.** With the OLED's pull-ups it boots fine; the LED may flicker with I2C traffic. GPIO 9 is the BOOT button, so don't hold it while powering on.
- **Battery into the 5V pin through a diode:** the onboard regulator needs ~3.2 V+ at the 5V pin and the diode drops ~0.3 V, so the last ~10–15 % of the LiPo isn't usable (the clip sleeps at 3.5 V battery). An "ideal diode" chip would lose almost nothing, but needs SMD soldering.
- **Bluetooth power:** advertising and the slow connection add a little drain while awake; both stop completely in deep sleep. Automatic light sleep between radio events would save more but isn't enabled in the stock Arduino core; a possible later improvement.
