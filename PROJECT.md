# ESP32-C3 Spotify "Now Playing" Bag Clip

A wearable clip that shows the current Spotify song on a small OLED screen. Tap it once to play/pause, tap twice to skip, and it sleeps after 5 minutes of silence (tap to wake). Runs on a rechargeable LiPo.

Source of truth: *Spotify Bag Clip Complete Guide v6* (uploaded PDF). This file summarises it, tracks progress, and records where the firmware here differs from the PDF.

## Status

| Stage | State |
|---|---|
| Project file + firmware saved | Done (25 Nov 2026) |
| Firmware reviewed, fixes applied | Done, **not yet compiled** (no Arduino toolchain in the cloud workspace) |
| Parts ordered | To do |
| Pre-flight stage 1: USB & board | To do |
| Pre-flight stage 2: OLED & touch | To do |
| Pre-flight stage 3: Spotify sync | To do |
| Pre-flight stage 4: touch & sleep | To do |
| Pre-flight stage 5: battery smoke test | To do |
| Enclosure + final assembly | To do |

**Next step:** order parts, then compile and flash `firmware/spotify_bag_clip_wifi` for pre-flight stages 1–2.

## Files

```
esp32c3/
├── README.md                        ← quick start
├── PROJECT.md                       ← this file
├── .gitignore                       ← keeps secrets.h out of git
├── docs/
│   └── Spotify_Bag_Clip_Complete_Guide_v6.pdf  ← original build guide
└── firmware/
    ├── spotify_bag_clip_wifi/       ← main firmware (Wi-Fi / hotspot, any phone)
    │   ├── spotify_bag_clip_wifi.ino
    │   └── secrets.example.h        ← copy to secrets.h, add Spotify keys
    └── spotify_bag_clip_ble/        ← optional Bluetooth LE version (Android only)
        └── spotify_bag_clip_ble.ino
```

## Parts (~₹600–₹800, order from Quartz Components or Robu.in)

| Part | Search for | Check before buying | ₹ |
|---|---|---|---|
| ESP32-C3 SuperMini | "ESP32-C3 SuperMini with soldered headers" | Headers pre-soldered | 248–299 |
| 0.96" OLED | "0.96 inch I2C OLED 4 pin SSD1306" | 4-pin I2C (GND, VCC, SCL, SDA), not 7-pin SPI. Yellow/blue looks best | 144–199 |
| TTP223 touch sensor | "TTP223 capacitive touch sensor module" | 3 pins (VCC, GND, I/O) | 15–30 |
| TP4056 charger | "TP4056 Type-C with protection" | 6 pads (B+, B-, OUT+, OUT-), not the 4-pad unprotected one | 32–55 |
| LiPo battery | "3.7V 500mAh LiPo battery" | Slim pouch ~30×20×6 mm | 80–150 |
| Slide switch | "Mini SPDT slide switch 3 pin" | Cuts battery fully | 10–20 |
| Jumper wires | "Female to Female jumper wires 40 pin" | For the no-solder bench test | 40–60 |
| Tools + clip | 25W soldering iron kit, metal alligator badge clip | Solder, flux, heat-shrink, foam tape | 180–300 |

## Safety rules (read before wiring)

1. **Battery goes to the 5V pin only.** TP4056 OUT+ (through the switch) → ESP32-C3 **5V**. Never 3.3V: a full LiPo is 4.2 V and will damage the 3.3 V rail.
2. **Check the OLED pin labels.** Some print GND, VCC, SCL, SDA and others VCC, GND, SCL, SDA. Wire by the text on *your* board.
3. **Never dual-power.** Switch OFF before plugging USB-C into either the TP4056 or the ESP32-C3.

## Wiring

### Step A: bench test with jumper wires (USB powered, no solder)

| ESP32-C3 | OLED | TTP223 | Purpose |
|---|---|---|---|
| 3.3V | VCC | VCC | Power (share via breadboard / Y-splice) |
| GND | GND | GND | Common ground |
| GPIO 8 | SDA | — | I2C data |
| GPIO 9 | SCL | — | I2C clock |
| GPIO 3 | — | I/O (SIG) | Touch input + deep-sleep wake (must be GPIO 0–5) |

### Step B: battery and charging (8 solder joints)

| From | To | Notes |
|---|---|---|
| LiPo red (+) | TP4056 B+ | |
| LiPo black (−) | TP4056 B− | |
| TP4056 OUT+ | Switch centre pin | |
| Switch outer pin | ESP32-C3 **5V** | Feeds the onboard 3.3 V regulator |
| TP4056 OUT− | ESP32-C3 GND | |

### Step C: sandwich stack

1. Front: OLED facing out.
2. Middle: LiPo wrapped in Kapton/electrical tape, foam-taped to the OLED's back.
3. Back: ESP32-C3 + TP4056 on short 3–4 cm wires. Keep the ESP32 antenna tip away from metal.
4. TTP223 glued flat against the inside wall of the case (senses through 2–3 mm plastic). Clip or N52 magnets epoxied to the rear.

## Software setup

**Arduino IDE:** board **ESP32C3 Dev Module**, **USB CDC On Boot: Enabled**.

**Libraries:** SpotifyArduino (Brian Lough, install from its GitHub ZIP if Library Manager doesn't list it), **ArduinoJson 6.x** (SpotifyArduino uses the v6 API, so pin to 6.21.x rather than 7), Adafruit SSD1306, Adafruit GFX Library, WiFiManager (tzapu).

**Spotify keys:**
1. Create an app at developer.spotify.com/dashboard. Add redirect URI `http://localhost:8888/callback`.
2. Run SpotifyArduino's `getRefreshToken` example with scopes `user-read-currently-playing user-modify-playback-state`.
3. Copy `secrets.example.h` to `secrets.h` and paste the client ID, secret and refresh token. `secrets.h` is gitignored.

**First run:** the clip opens a Wi-Fi network called **SpotifyClip-Setup**. Join it from your phone and pick your hotspot / home Wi-Fi. It remembers it after that.

Play/pause and skip need **Spotify Premium** (Spotify's API restriction). Now-playing display works on Free.

## Pre-flight checklist (do in order, before closing the case)

- [ ] **1. USB & board:** plug in, select ESP32C3 Dev Module + USB CDC On Boot Enabled, upload a blink/serial test. COM port shows up, upload clean.
- [ ] **2. OLED & touch:** jumper-wire OLED + TTP223, flash the Wi-Fi firmware. OLED shows "Connecting...", TTP223 LED lights when touched.
- [ ] **3. Spotify sync:** join SpotifyClip-Setup, enter Wi-Fi, play a song. Title, artist and progress bar within ~4 s, long titles scroll.
- [ ] **4. Touch & sleep:** 1 tap pauses/plays, 2 taps skip, pause for 5 min → sleeps, tap → wakes.
- [ ] **5. Battery smoke test:** USB unplugged, solder LiPo + TP4056 + switch, switch ON. Nothing warm, boots on battery, TP4056 LED red while charging / blue when full.

## Firmware review (changes from the v6 PDF)

Wi-Fi sketch (`spotify_bag_clip_wifi.ino`):

| # | Problem in v6 | Fix |
|---|---|---|
| 1 | `cp.artists.artistName` won't compile: `artists` is an array in SpotifyArduino | `cp.artists[0].artistName` |
| 2 | Keys hard-coded in the sketch | Moved to gitignored `secrets.h` with a clear `#error` if missing |
| 3 | If Wi-Fi fails, `ESP.restart()` loops forever, draining the battery when the hotspot is off | Shows "No Wi-Fi" and deep-sleeps; tap to retry |
| 4 | Wi-Fi drops while a song is playing → `isPlaying` stays true → never sleeps | After 5 failed polls in a row, treat as not playing so the 5-min sleep timer runs |
| 5 | Many C3 SuperMini boards can't join Wi-Fi at full TX power | `WiFi.setTxPower(WIFI_POWER_8_5dBm)` (raise if range is poor) |
| 6 | Entering sleep with a finger still on the pad wakes it instantly | Waits (up to 3 s) for release before sleeping |
| 7 | A 1 s HTTPS poll could start between the two taps of a double-tap and swallow it | Polling is skipped while a tap sequence is pending |
| 8 | Hindi / accented titles print as garbage and break the scroll width | Non-ASCII characters shown as `?` |
| 9 | Progress bar jumps every 3.5 s | Interpolated between polls |
| 10 | Redraws the OLED every loop pass (wasted I2C + power) | Capped at ~33 fps |
| 11 | Setup text "WiFi: SpotifyClip-Setup" is wider than the screen; guide says "Connecting..." | Shows "Connecting..." then a 2-line portal prompt |
| 12 | No feedback if the Spotify token is wrong | "Spotify login failed / Check secrets.h" on screen + serial log |

Checked and fine as written: `renderOLED()` calls `display.display()` correctly on both paths; `esp_deep_sleep_enable_gpio_wakeup` is the right API for the C3 and GPIO3 is a valid wake pin.

BLE sketch (`spotify_bag_clip_ble.ino`): track strings were edited from the BLE callback while `loop()` was drawing them (race), now handed over under a mutex. Default BLE MTU fits only 20 bytes, so "Blinding Lights|The Weeknd|1" (28) would be cut; MTU raised to 185 (the phone app must request it too). Service UUID added to advertising, plus the same sleep/finger and non-ASCII fixes.

## Known risks / open questions

- **Not compiled yet.** Expect small API differences depending on ESP32 Arduino core version (2.x vs 3.x). Compile first in stage 1.
- **Spotify TLS certificate:** `SpotifyArduinoCert.h` may be outdated. If every request returns -1, try `client.setInsecure()` for the bench test (comment in sketch), then update the cert.
- **GPIO 8 is the SuperMini's onboard LED and a boot strapping pin.** With the OLED's pull-ups it boots fine; the LED may flicker with I2C traffic. GPIO 9 is the BOOT button, so don't hold it while powering on.
- **Battery into the 5V pin:** the onboard regulator needs ~3.4 V+ input, so the clip may brown out in the last ~15% of battery.
- **BLE version:** the guide's MacroDroid "write BLE characteristic" step is unverified and iPhone can't do it at all. Stick with Wi-Fi unless you're on Android and want to test it.
