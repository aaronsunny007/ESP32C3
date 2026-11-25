/*
 * ESP32-C3 Spotify "Now Playing" Bag Clip — Wi-Fi firmware
 * Based on build guide v6 (Option 1), with fixes listed in PROJECT.md "Firmware review".
 *
 * Board:     ESP32C3 Dev Module, "USB CDC On Boot: Enabled"
 * Libraries: SpotifyArduino (Brian Lough), ArduinoJson 6.x, Adafruit SSD1306,
 *            Adafruit GFX Library, WiFiManager (tzapu)
 *
 * Credentials live in secrets.h (copy secrets.example.h -> secrets.h).
 *
 * Controls (TTP223 on GPIO3):
 *   1 tap   -> play / pause (needs Spotify Premium)
 *   2 taps  -> next track   (needs Spotify Premium)
 *   tap while asleep -> wake up
 * Sleeps after 5 minutes with nothing playing.
 */

#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <WiFiManager.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <SpotifyArduino.h>
#include <SpotifyArduinoCert.h>
#include "esp_sleep.h"

#if __has_include("secrets.h")
  #include "secrets.h"
#else
  #error "Missing secrets.h: copy secrets.example.h to secrets.h and fill in your Spotify keys."
#endif

// ---------- Pins (ESP32-C3 SuperMini) ----------
#define OLED_SDA   8
#define OLED_SCL   9
#define TOUCH_PIN  3          // must be GPIO0-5 to wake the C3 from deep sleep
#define SCREEN_WIDTH  128
#define SCREEN_HEIGHT 64
#define OLED_ADDR  0x3C

// ---------- Timing ----------
const unsigned long POLL_PLAYING_MS   = 3500;
const unsigned long POLL_IDLE_MS      = 12000;
const unsigned long IDLE_SLEEP_MS     = 5UL * 60UL * 1000UL;   // 5 minutes
const unsigned long TAP_WINDOW_MS     = 400;                   // gap that ends a tap sequence
const unsigned long FRAME_MS          = 30;                    // ~33 fps redraw cap
const int           MAX_POLL_FAILURES = 5;                     // then treat as "not playing"

// Many ESP32-C3 SuperMini boards fail to join Wi-Fi at full TX power
// (antenna/regulator issue). 8.5 dBm is the common fix; raise it if range is poor.
const wifi_power_t WIFI_TX_POWER = WIFI_POWER_8_5dBm;

Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, -1);
WiFiClientSecure client;
SpotifyArduino spotify(client, SPOTIFY_CLIENT_ID, SPOTIFY_CLIENT_SECRET, SPOTIFY_REFRESH_TOKEN);

String currentTitle = "", currentArtist = "";
bool isPlaying = false;
long trackProgress = 0, trackDuration = 1;
unsigned long progressStamp = 0;           // millis() when trackProgress was last read
int scrollX = 0;
unsigned long lastPoll = 0, lastScroll = 0, lastFrame = 0, inactiveSince = 0;
unsigned long lastTapTime = 0;
int tapCount = 0;
bool lastTouchState = LOW;
int pollFailures = 0;

// The default GFX font only has ASCII. Replace each non-ASCII (UTF-8) character
// with '?' so accented / Hindi titles don't print as garbage or break scroll width.
String toDisplayable(const char *s) {
  String out;
  if (!s) return out;
  for (const unsigned char *p = (const unsigned char *)s; *p; p++) {
    if (*p < 0x80) out += (char)*p;
    else if ((*p & 0xC0) == 0xC0) out += '?';  // lead byte of a multi-byte char
    // continuation bytes (10xxxxxx) are skipped
  }
  return out;
}

void showMessage(const char *line1, const char *line2 = nullptr) {
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextWrap(false);
  display.setCursor(0, 20);
  display.println(line1);
  if (line2) { display.setCursor(0, 34); display.println(line2); }
  display.display();
}

void enterDeepSleep() {
  showMessage("Sleeping...", "Tap to wake");
  delay(600);
  display.ssd1306_command(SSD1306_DISPLAYOFF);

  // If a finger is still on the pad, a HIGH-level wake would fire instantly.
  unsigned long t0 = millis();
  while (digitalRead(TOUCH_PIN) == HIGH && millis() - t0 < 3000) delay(10);

  WiFi.disconnect(true);
  WiFi.mode(WIFI_OFF);
  esp_deep_sleep_enable_gpio_wakeup(1ULL << TOUCH_PIN, ESP_GPIO_WAKEUP_GPIO_HIGH);
  esp_deep_sleep_start();
}

void onTrackData(CurrentlyPlaying cp) {
  isPlaying = cp.isPlaying;
  if (isPlaying) {
    String newTitle = toDisplayable(cp.trackName);
    if (newTitle != currentTitle) { currentTitle = newTitle; scrollX = 0; }
    // v6 bug: guide used cp.artists.artistName, but artists is an array.
    currentArtist = (cp.numArtists > 0) ? toDisplayable(cp.artists[0].artistName) : "";
    trackProgress = cp.progressMs;
    trackDuration = (cp.durationMs > 0) ? cp.durationMs : 1;
    progressStamp = millis();
    inactiveSince = 0;
  } else if (inactiveSince == 0) {
    inactiveSince = millis();
  }
}

void renderOLED() {
  if (millis() - lastFrame < FRAME_MS) return;
  lastFrame = millis();

  display.clearDisplay();
  display.setTextSize(1);
  display.setTextWrap(false);
  if (!isPlaying) {
    display.setCursor(22, 28);
    display.println("Spotify Paused");
    display.display();
    return;
  }
  display.setCursor(0, 0);
  display.println("NOW PLAYING");

  int titleWidth = currentTitle.length() * 6;
  if (titleWidth > SCREEN_WIDTH) {
    display.setCursor(scrollX, 16);
    display.print(currentTitle);
    if (millis() - lastScroll > 60) {
      scrollX -= 2;
      if (scrollX < -titleWidth) scrollX = SCREEN_WIDTH;
      lastScroll = millis();
    }
  } else {
    display.setCursor(0, 16);
    display.print(currentTitle);
  }

  display.setCursor(0, 34);
  display.print(currentArtist);

  // Advance the bar between polls so it moves smoothly instead of jumping every 3.5 s.
  long shownProgress = trackProgress + (long)(millis() - progressStamp);
  if (shownProgress > trackDuration) shownProgress = trackDuration;
  int barW = map(shownProgress, 0, trackDuration, 0, SCREEN_WIDTH - 4);
  display.drawRect(0, 54, SCREEN_WIDTH, 8, SSD1306_WHITE);
  display.fillRect(2, 56, constrain(barW, 0, SCREEN_WIDTH - 4), 4, SSD1306_WHITE);
  display.display();
}

void handleTouch() {
  bool state = digitalRead(TOUCH_PIN);
  if (state == HIGH && lastTouchState == LOW) {
    tapCount++;
    lastTapTime = millis();
    delay(40);  // debounce
  }
  lastTouchState = state;

  if (tapCount > 0 && (millis() - lastTapTime > TAP_WINDOW_MS)) {
    if (tapCount == 1) {
      if (isPlaying) spotify.pause(); else spotify.play();
    } else {
      spotify.nextTrack();
    }
    tapCount = 0;
    lastPoll = 0;  // force an immediate refresh
    inactiveSince = 0;  // a tap counts as activity
  }
}

void setup() {
  Serial.begin(115200);
  pinMode(TOUCH_PIN, INPUT);

  Wire.begin(OLED_SDA, OLED_SCL);
  if (!display.begin(SSD1306_SWITCHCAPVCC, OLED_ADDR)) {
    Serial.println("SSD1306 not found: check wiring / address (0x3C or 0x3D)");
  }
  display.setTextColor(SSD1306_WHITE);

  bool wokeFromTouch = (esp_sleep_get_wakeup_cause() == ESP_SLEEP_WAKEUP_GPIO);
  showMessage(wokeFromTouch ? "Waking up..." : "Connecting...");

  WiFi.mode(WIFI_STA);
  WiFi.setTxPower(WIFI_TX_POWER);

  WiFiManager wm;
  wm.setConnectTimeout(15);
  wm.setConfigPortalTimeout(120);
  wm.setAPCallback([](WiFiManager *) {
    showMessage("Setup Wi-Fi on phone:", "SpotifyClip-Setup");
  });
  if (!wm.autoConnect("SpotifyClip-Setup")) {
    // v6 called ESP.restart() here, which loops forever (and drains the
    // battery) when the hotspot is out of range. Sleep instead; tap to retry.
    showMessage("No Wi-Fi", "Tap later to retry");
    delay(2000);
    enterDeepSleep();
  }
  WiFi.setTxPower(WIFI_TX_POWER);  // WiFiManager may reset it while connecting

  client.setCACert(spotify_server_cert);
  // If you get TLS / -1 errors on every request, Spotify may have rotated its
  // certificate chain. For a bench test only, replace the line above with:
  //   client.setInsecure();

  showMessage("Spotify login...");
  if (!spotify.refreshAccessToken()) {
    showMessage("Spotify login failed", "Check secrets.h");
    Serial.println("refreshAccessToken failed: check client id/secret/refresh token");
    delay(5000);
  }

  inactiveSince = millis();
  lastTouchState = digitalRead(TOUCH_PIN);  // ignore the tap that woke us
}

void loop() {
  handleTouch();

  unsigned long pollInterval = isPlaying ? POLL_PLAYING_MS : POLL_IDLE_MS;
  // Don't start a blocking HTTPS call while the user is mid double-tap.
  if (tapCount == 0 && millis() - lastPoll > pollInterval) {
    int status = spotify.getCurrentlyPlaying(onTrackData, SPOTIFY_MARKET);
    if (status == 200) {
      pollFailures = 0;
    } else if (status == 204) {          // nothing playing on any device
      pollFailures = 0;
      isPlaying = false;
      if (inactiveSince == 0) inactiveSince = millis();
    } else {
      Serial.printf("getCurrentlyPlaying status %d\n", status);
      // Without this, a dropped hotspot while playing would keep the clip awake forever.
      if (++pollFailures >= MAX_POLL_FAILURES) {
        isPlaying = false;
        if (inactiveSince == 0) inactiveSince = millis();
      }
    }
    lastPoll = millis();
  }

  if (!isPlaying && inactiveSince > 0 && (millis() - inactiveSince > IDLE_SLEEP_MS)) {
    enterDeepSleep();
  }
  renderOLED();
}
