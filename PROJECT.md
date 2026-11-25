# ESP32-C3 Spotify "Now Playing" Bag Clip

A wearable clip that shows the current Spotify song on a small OLED screen, with album art, what's playing next and a battery gauge. Tap once to play/pause, twice to skip, three times to go back. When nothing is playing it shows a panda, dims, and then sleeps (tap to wake). Your phone pairs with it over Bluetooth to set up Wi-Fi and change settings. Runs on a rechargeable LiPo.

Source of truth for the hardware: *Spotify Bag Clip Complete Guide v6* (in `docs/`). This file summarises it, tracks progress, and records where the firmware differs from the PDF.

## Status

| Stage | State |
|---|---|
| Project file + firmware v1 saved | Done (25 Nov 2026) |
| Firmware v2 (Bluetooth setup, battery, panda, album art, queue, settings, OTA) | Written 25 Nov 2026. Compiles on GitHub (see "Build check"), **not yet tested on hardware** |
| Parts ordered (add 2 × 100 kΩ resistors) | To do |
| Pre-flight stage 1: USB & board | To do |
| Pre-flight stage 2: OLED & touch | To do |
| Pre-flight stage 3: Bluetooth setup + Spotify sync | To do |
| Pre-flight stage 4: touch, dim & sleep | To do |
| Pre-flight stage 5: battery smoke test + battery level | To do |
| Enclosure + final assembly | To do |

**Next step:** order parts (plus two 100 kΩ resistors), then compile and flash `firmware/bag_clip` for pre-flight stages 1–2.

## How Bluetooth and Wi-Fi share the work

Spotify only talks over the internet, so the clip still needs Wi-Fi (home Wi-Fi or your phone's hotspot) to get songs, album art and the queue. Bluetooth is how your phone talks to the clip itself.

| Job | Bluetooth (phone ↔ clip) | Wi-Fi (clip ↔ internet) |
|---|---|---|
| Pairing your phone (6-digit PIN shown on the OLED) | ✓ | |
| Adding / removing Wi-Fi networks (up to 5) | ✓ | ✓ (web page) |
| Settings: brightness, dim time, sleep time, scroll speed, album art on/off | ✓ | ✓ (web page) |
| Play / pause / next / previous from the phone | ✓ (sent on to Spotify over Wi-Fi) | ✓ |
| Status: battery, Wi-Fi, song | ✓ | ✓ |
| Now playing, album art, next in queue, playback control | | ✓ |
| Firmware updates (OTA) | | ✓ |

Why not Bluetooth only: the clip can't reach Spotify through Bluetooth without a companion app on the phone forwarding the data, and iPhones don't allow that from a web page. Why not Wi-Fi only: the old setup hotspot (WiFiManager) needed you to join a separate network every time you changed Wi-Fi; Bluetooth makes setup a one-tap job and lets you add your phone's hotspot and home Wi-Fi once.

Both radios run at the same time (the ESP32-C3 shares one antenna between them). The Bluetooth link is encrypted and needs the PIN on first pairing, so Wi-Fi passwords never travel in the clear and nobody nearby can skip your songs.

## Files

```
esp32c3/
├── README.md                 ← quick start
├── PROJECT.md                ← this file
├── .gitignore                ← keeps secrets.h out of git
├── .github/workflows/build.yml ← compiles the firmware on every push
├── docs/
│   ├── Spotify_Bag_Clip_Complete_Guide_v6.pdf
│   └── images/panda_preview.png
├── firmware/bag_clip/        ← the firmware (one Arduino sketch, several tabs)
│   ├── bag_clip.ino          ← setup/loop, touch, sleep, network task
│   ├── config.h              ← pins, timings, defaults  (edit for hardware changes)
│   ├── secrets.example.h     ← copy to secrets.h: Spotify keys, admin password
│   ├── spotify.cpp/.h        ← Spotify Web API (now playing, queue, controls, art)
│   ├── art.cpp/.h            ← album art JPEG → black-and-white thumbnail
│   ├── ui.cpp/.h             ← every screen, icons, scrolling
│   ├── panda.h               ← idle-screen panda bitmaps (swap for your own)
│   ├── battery.cpp/.h        ← battery voltage and %
│   ├── ble_link.cpp/.h       ← Bluetooth service for the phone
│   ├── web_portal.cpp/.h     ← settings web page + OTA
│   ├── settings.cpp/.h       ← settings and Wi-Fi list stored in flash
│   ├── state.cpp/.h          ← shared state between tasks
│   └── certs.h               ← root certificates for Spotify's HTTPS
├── phone/index.html          ← phone setup page (Web Bluetooth)
└── tools/
    ├── png_to_bitmap.py      ← turn any picture into an OLED bitmap
    └── draw_panda.py         ← how the default panda was drawn
```

The two v1 sketches (`spotify_bag_clip_wifi`, `spotify_bag_clip_ble`) are replaced by `bag_clip` and remain in git history.

## Parts (~₹600–₹800, order from Quartz Components or Robu.in)

| Part | Search for | Check before buying | ₹ |
|---|---|---|---|
| ESP32-C3 SuperMini | "ESP32-C3 SuperMini with soldered headers" | Headers pre-soldered | 248–299 |
| 0.96" OLED | "0.96 inch I2C OLED 4 pin SSD1306" | 4-pin I2C (GND, VCC, SCL, SDA), not 7-pin SPI. Yellow/blue looks best | 144–199 |
| TTP223 touch sensor | "TTP223 capacitive touch sensor module" | 3 pins (VCC, GND, I/O) | 15–30 |
| TP4056 charger | "TP4056 Type-C with protection" | 6 pads (B+, B-, OUT+, OUT-), not the 4-pad unprotected one | 32–55 |
| LiPo battery | "3.7V 500mAh LiPo battery" | Slim pouch ~30×20×6 mm | 80–150 |
| Slide switch | "Mini SPDT slide switch 3 pin" | Cuts battery fully | 10–20 |
| **2 × 100 kΩ resistors** | "100K ohm 1/4W resistor" (1% metal film if possible) | For the battery gauge | 5–10 |
| 100 nF capacitor (optional) | "104 ceramic capacitor" | Steadier battery reading | 2 |
| Jumper wires | "Female to Female jumper wires 40 pin" | For the no-solder bench test | 40–60 |
| Tools + clip | 25W soldering iron kit, metal alligator badge clip | Solder, flux, heat-shrink, foam tape | 180–300 |

## Safety rules (read before wiring)

1. **Battery goes to the 5V pin only.** TP4056 OUT+ (through the switch) → ESP32-C3 **5V**. Never 3.3V: a full LiPo is 4.2 V and will damage the 3.3 V rail.
2. **Never wire the battery straight to GPIO4.** It must go through the two resistors; 4.2 V on a GPIO pin will damage the chip.
3. **Check the OLED pin labels.** Some print GND, VCC, SCL, SDA and others VCC, GND, SCL, SDA. Wire by the text on *your* board.
4. **Never dual-power.** Switch OFF before plugging USB-C into either the TP4056 or the ESP32-C3.

## Wiring

### Step A: bench test with jumper wires (USB powered, no solder)

| ESP32-C3 | OLED | TTP223 | Purpose |
|---|---|---|---|
| 3.3V | VCC | VCC | Power (share via breadboard / Y-splice) |
| GND | GND | GND | Common ground |
| GPIO 8 | SDA | — | I2C data |
| GPIO 9 | SCL | — | I2C clock |
| GPIO 3 | — | I/O (SIG) | Touch input + deep-sleep wake (must be GPIO 0–5) |

Without the battery divider the clip simply hides the battery icon.

### Step B: battery and charging (8 solder joints)

| From | To | Notes |
|---|---|---|
| LiPo red (+) | TP4056 B+ | |
| LiPo black (−) | TP4056 B− | |
| TP4056 OUT+ | Switch centre pin | |
| Switch outer pin | ESP32-C3 **5V** | Feeds the onboard 3.3 V regulator |
| TP4056 OUT− | ESP32-C3 GND | |

### Step B2: battery level (2 resistors, 4 solder joints)

A voltage divider halves the battery voltage (max 4.2 V → 2.1 V) so the ESP32-C3's ADC can read it.

```
 ESP32-C3 5V pin (= switch output = battery +)
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
| ESP32-C3 **5V** pin (same pad as the switch wire) | R1, one leg |
| R1, other leg | **GPIO 4** and R2, one leg |
| R2, other leg | ESP32-C3 **GND** |

- Taking it after the switch means the divider draws nothing when the clip is switched off. While on, it draws ~20 µA, which is small next to the clip itself.
- With USB plugged into the ESP32 (switch off) the 5V pin sits at ~4.7 V, so the screen shows **USB** and a lightning bolt instead of a percentage.
- **Calibrate once:** hold the touch pad for 1.5 s to open the info screen, compare "Batt x.xx V" with a multimeter on the battery, and set `BATT_CAL` in `config.h` to *real ÷ shown* (e.g. 4.02 / 3.95 = 1.018).
- Below ~3.45 V the board's regulator starts to brown out, so the clip shows "Battery low" and sleeps until charged.
- Using different resistors: set `BATT_DIVIDER` in `config.h` to (R1 + R2) ÷ R2. Keep the pin voltage under 2.5 V.

### Step C: sandwich stack

1. Front: OLED facing out.
2. Middle: LiPo wrapped in Kapton/electrical tape, foam-taped to the OLED's back.
3. Back: ESP32-C3 + TP4056 on short 3–4 cm wires, resistors heat-shrunk along the wires. Keep the ESP32 antenna tip away from metal.
4. TTP223 glued flat against the inside wall of the case (senses through 2–3 mm plastic). Clip or N52 magnets epoxied to the rear.

## Software setup

**Arduino IDE** (Tools menu):
- Board: **ESP32C3 Dev Module** (ESP32 core **3.x**)
- USB CDC On Boot: **Enabled**
- Partition Scheme: **Minimal SPIFFS (1.9MB APP with OTA/190KB SPIFFS)** — the firmware with Bluetooth + Wi-Fi + HTTPS is too big for the default scheme, and OTA needs two app slots.

**Libraries** (Library Manager):

| Library | Version |
|---|---|
| Adafruit SSD1306 + Adafruit GFX Library | latest |
| ArduinoJson (Benoit Blanchon) | **7.x** |
| NimBLE-Arduino (h2zero) | **2.x** |
| TJpg_Decoder (Bodmer) | latest (1.1.x) |

SpotifyArduino and WiFiManager are **no longer needed**: the firmware has its own small Spotify client (it needs the queue, which SpotifyArduino doesn't support) and Bluetooth replaces the Wi-Fi setup portal.

**Spotify keys:**
1. Create an app at developer.spotify.com/dashboard. Add redirect URI `http://127.0.0.1:8888/callback` (Spotify no longer accepts `localhost`).
2. Get a refresh token with scopes **`user-read-currently-playing user-read-playback-state user-modify-playback-state`**. The middle one is new in v2 (for "next in queue"); if you made a token for v1, make a new one. SpotifyArduino's `getRefreshToken` example still works for this step (install it just for that, or use any Spotify OAuth helper).
3. Copy `firmware/bag_clip/secrets.example.h` to `secrets.h`, paste the client ID, secret and refresh token, and **change `ADMIN_PASSWORD`**. `secrets.h` is gitignored.

Play/pause and skip need **Spotify Premium** (Spotify's rule). Without it the clip shows "Needs Spotify Premium" when you tap; the now-playing display works on Free.

### First run: pair your phone

1. Flash the firmware. The OLED says **Set me up!** (no Wi-Fi saved yet).
2. On your phone, open the setup page (see below) and tap **Connect**, pick **BagClip**.
3. Your phone asks for a PIN: type the 6 digits shown on the clip. This happens once; the phone is remembered after that.
4. Under **Wi-Fi networks**, add your phone's hotspot and your home Wi-Fi. The clip joins the strongest one in range, and switches automatically when you move.
5. Play a song on Spotify. Title, artist, album art and "next up" appear within a few seconds.

**The phone page** is `phone/index.html` and uses Web Bluetooth:
- **Android:** Chrome. **Laptop:** Chrome or Edge. **iPhone:** the free *Bluefy* browser (Safari has no Web Bluetooth).
- Browsers only allow Bluetooth on `https://` pages. Easiest: on GitHub, Settings → Pages → deploy from branch `main` (or the default branch), folder `/ (root)`, then open `https://<your-user>.github.io/esp32c3/phone/`. GitHub Pages on a private repo needs a paid plan; otherwise copy the file to any free static host.
- **No-page fallback:** the *nRF Connect* app (Android and iPhone). Connect to BagClip, open the service starting `b4c1a000`, and write UTF-8 text to:

| Characteristic | Write | Read |
|---|---|---|
| `b4c1a001…` Wi-Fi | `add` ⏎ `SSID` ⏎ `password` (newline-separated), or `del` ⏎ `SSID` | saved network names |
| `b4c1a002…` Settings | `brightness=200;dim=8;sleep=5;scroll=3;art=1` (any subset) | current settings |
| `b4c1a003…` Control | `toggle`, `play`, `pause`, `next`, `prev`, `refresh` | — |
| `b4c1a004…` Status | — | `bat=`, `wifi=`, `ip=`, `title=` … lines (also notifies) |

### Settings web page and OTA updates (over Wi-Fi)

Once on Wi-Fi, the clip serves a page at **http://bagclip.local** (or the IP from the info screen: hold the pad 1.5 s). Log in as `admin` with your `ADMIN_PASSWORD`. It has the same settings and Wi-Fi list as the phone page, play buttons, and a firmware upload box.

Two ways to update firmware without a cable:
- **Arduino IDE:** with your computer on the same Wi-Fi, pick **Tools → Port → bagclip at 192.168.x.x** and upload as usual. It asks for `ADMIN_PASSWORD`.
- **Browser:** Sketch → Export Compiled Binary, then upload `bag_clip.ino.bin` on the web page. Each GitHub build also attaches the `.bin` as an artifact.

The clip shows a progress bar and restarts when done. Settings and saved Wi-Fi survive updates.

### Settings

| Setting | Default | Range | What it does |
|---|---|---|---|
| Brightness | 200 | 1–255 | OLED contrast |
| Dim after | 8 s | 0–3600 (0 = never) | Dims the panda screen when nothing has happened for this long |
| Sleep after | 5 min | 0–240 (0 = never) | Deep sleep when nothing is playing. Never sleeps while the phone is connected |
| Scroll speed | 3 | 1–5 | Long titles scroll at 12 / 20 / 30 / 42 / 60 pixels per second |
| Album art | on | on/off | 32×32 black-and-white thumbnail |

Defaults live in `config.h`; changes from the phone or web page are saved in flash.

### Swapping the panda

The idle screen draws `PANDA_OPEN` and, for a quick blink every 4 s, `PANDA_BLINK` from `firmware/bag_clip/panda.h` (64×64, 1 bit per pixel, lit pixels = white).

![panda preview](docs/images/panda_preview.png)

To use your own picture (`pip install pillow` first):

```
python3 tools/png_to_bitmap.py my_pic.png PANDA_OPEN  > open.h
python3 tools/png_to_bitmap.py my_pic_blink.png PANDA_BLINK > blink.h
```

Paste each array over the one in `panda.h` (keep the `PANDA_W` / `PANDA_H` lines at the bottom). For one picture with no blink, convert it twice under both names. Draw white on black, or add `--invert` for a black-on-white drawing and `--dither` for photos. The online tool *image2cpp* also works: 64×64, "Horizontal – 1 bit per pixel", Arduino code output.

## Screens

| Screen | When |
|---|---|
| Now playing | Status bar (play/pause icon, Wi-Fi bars, Bluetooth icon when the phone is connected, battery % or USB), album art on the left, title / artist / ► next-up scrolling on the right, elapsed and total time, progress bar |
| Paused | Same as now playing with a pause icon for 10 s, then the panda |
| Panda (idle) | Nothing playing: blinking panda, "Nothing playing", Wi-Fi and battery. Dims after the dim time, sleeps after the sleep time |
| Set me up! | No Wi-Fi saved yet: pair your phone |
| Bluetooth PIN | While your phone pairs |
| Info | Hold the pad 1.5 s: version, Wi-Fi name and IP, battery voltage, last Spotify status |
| Updating firmware | During OTA, with a progress bar |
| Messages | Short pop-ups: "Next", "Needs Spotify Premium", "No active device" |

## Pre-flight checklist (do in order, before closing the case)

- [ ] **1. USB & board:** plug in, select ESP32C3 Dev Module + USB CDC On Boot Enabled + Minimal SPIFFS partition, upload a blink/serial test. COM port shows up, upload clean.
- [ ] **2. OLED & touch:** jumper-wire OLED + TTP223, flash `bag_clip`. OLED shows "Bag Clip v2.0.0" then "Set me up!"; TTP223 LED lights when touched; holding it 1.5 s shows the info screen.
- [ ] **3. Bluetooth + Spotify:** pair from the phone page (PIN on the OLED), add your hotspot, play a song. Title, artist, album art, next-up and times within ~5 s; long titles scroll smoothly. http://bagclip.local opens.
- [ ] **4. Touch, dim & sleep:** 1 tap pauses/plays, 2 taps skip, 3 taps go back. Pause: panda after 10 s, dims after 8 s more, sleeps after 5 min. Tap → wakes.
- [ ] **5. Battery:** USB unplugged, solder LiPo + TP4056 + switch + the two resistors, switch ON. Nothing warm, boots on battery, battery % shows; calibrate `BATT_CAL` against a multimeter. TP4056 LED red while charging / blue when full.

## Build check

There is no Arduino toolchain in the cloud workspace, so `.github/workflows/build.yml` compiles the sketch on GitHub for every push (ESP32 core 3.1.1, NimBLE-Arduino 2.1.0, ArduinoJson 7.2.1, TJpg_Decoder 1.1.0, Minimal SPIFFS partition) and attaches the `.bin`. A green check means it compiles; it doesn't test behaviour on real hardware.

## Firmware notes (v2)

- **No more frozen screen during network calls.** Spotify requests run in their own FreeRTOS task, so scrolling and touch stay smooth while HTTPS calls are in flight. Taps are queued to that task, which also fixes the v1 issue of a poll swallowing a double-tap.
- **Smoother scrolling:** time-based (constant speed regardless of frame time), 1-pixel steps, a 1.5 s pause at the start, and a seamless wrap-around instead of jumping back.
- **Keep-alive to api.spotify.com** avoids a full TLS handshake on every poll (faster, less battery). Album art, queue and token calls use a second short-lived connection, closed before the next one opens to save RAM.
- **Album art:** Spotify's smallest cover (64×64 JPEG, ~3 KB) is decoded at half size and Floyd–Steinberg dithered with automatic contrast stretch.
- **Next in queue** is fetched when the song changes and every 2 min while playing (the reply is large, so it isn't polled every few seconds).
- **Refresh token rotation:** if Spotify hands out a new refresh token it is saved in flash and used until you paste a different one into `secrets.h`.
- Carried over from v1: `WIFI_POWER_8_5dBm` TX power fix for SuperMini boards, sleep instead of reboot-looping when Wi-Fi is missing, wait for the finger to lift before sleeping, poll failures count as "not playing" so it can still sleep, non-ASCII characters shown as `?`, interpolated progress bar, Spotify login errors on screen.

## Known risks / open questions

- **Not tested on hardware yet.** Library APIs (NimBLE-Arduino 2.x, ESP32 core 3.x) are pinned in the build check; older versions (NimBLE 1.x, core 2.x) won't compile.
- **Memory:** Bluetooth + Wi-Fi + HTTPS together use a lot of the C3's 400 KB RAM. The firmware keeps only one HTTPS connection open at a time. If you see resets with "alloc failed" on the serial monitor, turn album art off first.
- **Spotify certificates:** `certs.h` bundles 7 common root CAs. If every request fails with a TLS error, set `SPOTIFY_TLS_INSECURE 1` in `secrets.h` for a bench test and report it.
- **Battery icon accuracy:** voltage-based, ±10 %. It reads a bit low during Wi-Fi activity and a bit high while charging.
- **GPIO 8 is the SuperMini's onboard LED and a boot strapping pin.** With the OLED's pull-ups it boots fine; the LED may flicker with I2C traffic. GPIO 9 is the BOOT button, so don't hold it while powering on.
- **Battery into the 5V pin:** the onboard regulator needs ~3.4 V+ input, so the last ~10 % of the LiPo isn't usable (the clip sleeps at 3.45 V).
- **Bluetooth power:** advertising adds a little drain while awake; it stops completely in deep sleep.
