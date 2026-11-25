/*
 * ESP32-C3 Spotify "Now Playing" Bag Clip — Bluetooth LE firmware (optional)
 * Based on build guide v6 (section 5), with fixes listed in PROJECT.md "Firmware review".
 * Same wiring as the Wi-Fi build. No Spotify keys needed; an Android phone pushes
 * "Title|Artist|1" (1 = playing, 0 = paused) to the clip over BLE.
 *
 * Libraries: Adafruit SSD1306, Adafruit GFX Library (BLE is built into the ESP32 core)
 */

#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>
#include <BLE2902.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include "esp_sleep.h"

#define OLED_SDA   8
#define OLED_SCL   9
#define TOUCH_PIN  3
#define SCREEN_WIDTH  128
#define SCREEN_HEIGHT 64

#define SERVICE_UUID       "4fafc201-1fb5-459e-8fcc-c5c9c331914b"
#define TRACK_CHAR_UUID    "beb5483e-36e1-4688-b7f5-ea07361b26a8"  // phone writes "Title|Artist|1"
#define CONTROL_CHAR_UUID  "cba1d466-344c-4be3-ab3f-189f80dd7518"  // clip notifies "PAUSE" / "NEXT"

const unsigned long IDLE_SLEEP_MS = 5UL * 60UL * 1000UL;
const unsigned long TAP_WINDOW_MS = 400;
const unsigned long FRAME_MS      = 30;

Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, -1);
BLECharacteristic *pControlChar;
volatile bool deviceConnected = false;
volatile unsigned long disconnectedSince = 0;

// onWrite() runs on the BLE task, loop() on the Arduino task. v6 edited the
// display Strings directly from the callback, which can corrupt them mid-draw.
// Now the callback only stores the raw payload under a mutex; loop() parses it.
SemaphoreHandle_t payloadLock;
String pendingPayload;
bool payloadReady = false;

String currentTitle  = "Waiting for phone...";
String currentArtist = "BLE: SpotifyClip";
bool isPlaying = true;
int scrollX = 0;
unsigned long lastScroll = 0, lastFrame = 0, lastTapTime = 0;
int tapCount = 0;
bool lastTouchState = LOW;

String toDisplayable(const String &s) {
  String out;
  for (size_t i = 0; i < s.length(); i++) {
    unsigned char c = s[i];
    if (c < 0x80) out += (char)c;
    else if ((c & 0xC0) == 0xC0) out += '?';
  }
  return out;
}

class ServerCallbacks : public BLEServerCallbacks {
  void onConnect(BLEServer *) override {
    deviceConnected = true;
    disconnectedSince = 0;
  }
  void onDisconnect(BLEServer *) override {
    deviceConnected = false;
    disconnectedSince = millis();
    BLEDevice::startAdvertising();
  }
};

class TrackCallback : public BLECharacteristicCallbacks {
  void onWrite(BLECharacteristic *pChar) override {
    String val = String(pChar->getValue().c_str());  // works on core 2.x and 3.x
    xSemaphoreTake(payloadLock, portMAX_DELAY);
    pendingPayload = val;
    payloadReady = true;
    xSemaphoreGive(payloadLock);
  }
};

void applyPayload() {
  if (!payloadReady) return;
  xSemaphoreTake(payloadLock, portMAX_DELAY);
  String val = pendingPayload;
  payloadReady = false;
  xSemaphoreGive(payloadLock);

  int firstSep = val.indexOf('|');
  int lastSep = val.lastIndexOf('|');
  if (firstSep <= 0) return;
  String newTitle = toDisplayable(val.substring(0, firstSep));
  if (newTitle != currentTitle) { currentTitle = newTitle; scrollX = 0; }
  if (lastSep > firstSep) {
    currentArtist = toDisplayable(val.substring(firstSep + 1, lastSep));
    String flag = val.substring(lastSep + 1);
    flag.trim();
    isPlaying = (flag == "1");
  } else {
    currentArtist = toDisplayable(val.substring(firstSep + 1));
    isPlaying = true;
  }
}

void renderOLED() {
  if (millis() - lastFrame < FRAME_MS) return;
  lastFrame = millis();

  display.clearDisplay();
  display.setTextSize(1);
  display.setTextWrap(false);
  display.setCursor(0, 0);
  display.println(deviceConnected ? "BLE: SPOTIFY" : "BLE: DISCONNECTED");

  if (!isPlaying) {
    display.setCursor(22, 28);
    display.println("Spotify Paused");
    display.display();
    return;
  }

  int titleWidth = currentTitle.length() * 6;
  if (titleWidth > SCREEN_WIDTH) {
    display.setCursor(scrollX, 18);
    display.print(currentTitle);
    if (millis() - lastScroll > 60) {
      scrollX -= 2;
      if (scrollX < -titleWidth) scrollX = SCREEN_WIDTH;
      lastScroll = millis();
    }
  } else {
    display.setCursor(0, 18);
    display.print(currentTitle);
  }
  display.setCursor(0, 38);
  display.print(currentArtist);
  display.display();
}

void handleTouch() {
  bool state = digitalRead(TOUCH_PIN);
  if (state == HIGH && lastTouchState == LOW) {
    tapCount++;
    lastTapTime = millis();
    delay(40);
  }
  lastTouchState = state;

  if (tapCount > 0 && (millis() - lastTapTime > TAP_WINDOW_MS)) {
    if (deviceConnected) {
      String cmd = (tapCount == 1) ? "PAUSE" : "NEXT";
      pControlChar->setValue(cmd.c_str());
      pControlChar->notify();
    }
    tapCount = 0;
  }
}

void enterDeepSleep() {
  display.clearDisplay();
  display.setCursor(22, 28);
  display.println("Sleeping...");
  display.display();
  delay(600);
  display.ssd1306_command(SSD1306_DISPLAYOFF);
  unsigned long t0 = millis();
  while (digitalRead(TOUCH_PIN) == HIGH && millis() - t0 < 3000) delay(10);
  esp_deep_sleep_enable_gpio_wakeup(1ULL << TOUCH_PIN, ESP_GPIO_WAKEUP_GPIO_HIGH);
  esp_deep_sleep_start();
}

void setup() {
  Serial.begin(115200);
  pinMode(TOUCH_PIN, INPUT);
  payloadLock = xSemaphoreCreateMutex();

  Wire.begin(OLED_SDA, OLED_SCL);
  display.begin(SSD1306_SWITCHCAPVCC, 0x3C);
  display.setTextColor(SSD1306_WHITE);

  BLEDevice::init("SpotifyClip");
  // Default BLE MTU only fits 20 bytes per write; "Blinding Lights|The Weeknd|1"
  // is 28. Allow a larger MTU (the phone side must also request it).
  BLEDevice::setMTU(185);

  BLEServer *pServer = BLEDevice::createServer();
  pServer->setCallbacks(new ServerCallbacks());
  BLEService *pService = pServer->createService(SERVICE_UUID);

  BLECharacteristic *pTrackChar = pService->createCharacteristic(
      TRACK_CHAR_UUID, BLECharacteristic::PROPERTY_WRITE | BLECharacteristic::PROPERTY_WRITE_NR);
  pTrackChar->setCallbacks(new TrackCallback());

  pControlChar = pService->createCharacteristic(CONTROL_CHAR_UUID, BLECharacteristic::PROPERTY_NOTIFY);
  pControlChar->addDescriptor(new BLE2902());

  pService->start();
  BLEAdvertising *adv = BLEDevice::getAdvertising();
  adv->addServiceUUID(SERVICE_UUID);   // lets phone apps filter scans by service
  adv->setScanResponse(true);
  BLEDevice::startAdvertising();

  disconnectedSince = millis();
  lastTouchState = digitalRead(TOUCH_PIN);
}

void loop() {
  applyPayload();
  handleTouch();
  if (!deviceConnected && disconnectedSince > 0 && (millis() - disconnectedSince > IDLE_SLEEP_MS)) {
    enterDeepSleep();
  }
  renderOLED();
}
