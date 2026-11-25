#include "ble_link.h"
#include <NimBLEDevice.h>
#include "config.h"
#include "settings.h"
#include "battery.h"

static NimBLECharacteristic *statusChar = nullptr;
static NimBLECharacteristic *buttonsChar = nullptr;
static unsigned long lastStatus = 0;
static String lastStatusText;
static int connections = 0;   // the app and the phone page can be connected at once

static String charText(NimBLECharacteristic *c) {
  return String(c->getValue().c_str());
}

static void setText(NimBLECharacteristic *c, const String &s) {
  c->setValue((const uint8_t *)s.c_str(), s.length());
}

class ServerCallbacks : public NimBLEServerCallbacks {
  void onConnect(NimBLEServer *, NimBLEConnInfo &) override {
    stateLock(); connections++; stateRef().bleConnected = true; stateUnlock();
  }
  void onDisconnect(NimBLEServer *, NimBLEConnInfo &, int) override {
    stateLock();
    AppState &s = stateRef();
    s.blePasskey = 0;
    connections = max(0, connections - 1);
    s.bleConnected = connections > 0;
    if (!s.bleConnected && s.appLinked) {
      // Phone gone (out of range, Bluetooth off): stop "playing" so the clip can
      // show the panda and go to sleep. The song stays for when it comes back.
      s.appLinked = false;
      s.playing = false;
    }
    stateUnlock();
    // advertising restarts automatically
  }
  // The phone asks for a PIN: show a fresh random one on the OLED.
  uint32_t onPassKeyDisplay() override {
    uint32_t pin = esp_random() % 1000000;
    stateLock(); stateRef().blePasskey = pin; stateUnlock();
    return pin;
  }
  void onAuthenticationComplete(NimBLEConnInfo &info) override {
    stateLock(); stateRef().blePasskey = 0; stateUnlock();
    NimBLEServer *server = NimBLEDevice::getServer();
    if (!info.isEncrypted()) {
      server->disconnect(info.getConnHandle());
      return;
    }
    // Paired and encrypted: switch to a slow, low-power connection.
    server->updateConnParams(info.getConnHandle(), BLE_CONN_MIN_INTERVAL, BLE_CONN_MAX_INTERVAL,
                             BLE_CONN_LATENCY, BLE_CONN_TIMEOUT);
  }
};

#if WIFI_MODE
class WifiCallbacks : public NimBLECharacteristicCallbacks {
  void onRead(NimBLECharacteristic *c, NimBLEConnInfo &) override { setText(c, wifiListSsids('\n')); }
  void onWrite(NimBLECharacteristic *c, NimBLEConnInfo &) override {
    String v = charText(c);
    int a = v.indexOf('\n');
    String cmd = a < 0 ? v : v.substring(0, a);
    String rest = a < 0 ? String() : v.substring(a + 1);
    bool ok = false;
    if (cmd == "add") {
      int b = rest.indexOf('\n');
      String ssid = b < 0 ? rest : rest.substring(0, b);
      String pass = b < 0 ? String() : rest.substring(b + 1);
      ok = wifiAdd(ssid.c_str(), pass.c_str());
    } else if (cmd == "del") {
      ok = wifiRemove(rest.c_str());
    }
    Serial.printf("[ble] wifi %s -> %s\n", cmd.c_str(), ok ? "ok" : "rejected");
    setText(c, wifiListSsids('\n'));   // don't leave the password sitting in the value
    if (ok) sendNetCommand(CMD_WIFI_CHANGED);
  }
};
#endif

class SettingsCallbacks : public NimBLECharacteristicCallbacks {
  void onRead(NimBLECharacteristic *c, NimBLEConnInfo &) override { setText(c, settingsToString()); }
  void onWrite(NimBLECharacteristic *c, NimBLEConnInfo &) override {
    // The display picks up new values on its next frame. A rejected write changes
    // nothing; the app notices because reading back gives the old values.
    settingsApply(charText(c));
  }
};

class ControlCallbacks : public NimBLECharacteristicCallbacks {
  void onWrite(NimBLECharacteristic *c, NimBLEConnInfo &) override {
    String v = charText(c);
    v.trim();
    if (v == "toggle")       sendCommand(CMD_TOGGLE);
    else if (v == "play")    sendCommand(CMD_PLAY);
    else if (v == "pause")   sendCommand(CMD_PAUSE);
    else if (v == "next")    sendCommand(CMD_NEXT);
    else if (v == "prev")    sendCommand(CMD_PREV);
    else if (v == "refresh") sendCommand(CMD_REFRESH);
  }
};

// Now playing from the app: newline-separated key=value pairs, any subset, e.g.
//   title=Song\nartist=Band\nnext=Other song\ndur=215000\npos=42000\nplay=1\napp=Spotify\npbat=81
// "clear=1" means nothing is playing on the phone.
class NowPlayingCallbacks : public NimBLECharacteristicCallbacks {
  void onWrite(NimBLECharacteristic *c, NimBLEConnInfo &) override {
    String v = charText(c);
    String title, artist;
    bool haveTitle = false, haveArtist = false;

    stateLock();
    AppState &s = stateRef();
    int start = 0;
    while (start < (int)v.length()) {
      int end = v.indexOf('\n', start);
      if (end < 0) end = v.length();
      String line = v.substring(start, end);
      start = end + 1;
      int eq = line.indexOf('=');
      if (eq <= 0) continue;
      String k = line.substring(0, eq), val = line.substring(eq + 1);
      if (k == "title")       { title = val; haveTitle = true; }
      else if (k == "artist") { artist = val; haveArtist = true; }
      else if (k == "next")   toDisplayable(val.c_str(), s.nextTitle, sizeof(s.nextTitle));
      else if (k == "dur")    s.durationMs = max(1L, val.toInt());
      else if (k == "pos")    { s.progressMs = max(0L, val.toInt()); s.progressStamp = millis(); }
      else if (k == "play")   s.playing = val.toInt() == 1;
      else if (k == "app")    toDisplayable(val.c_str(), s.sourceApp, sizeof(s.sourceApp));
      else if (k == "pbat")   s.phoneBattery = constrain((int)val.toInt(), -1, 100);
      else if (k == "clear")  { s.hasTrack = false; s.playing = false; s.title[0] = s.artist[0] = s.nextTitle[0] = 0; s.artValid = false; }
    }
    if (haveTitle) {
      char t[sizeof(s.title)], a[sizeof(s.artist)];
      toDisplayable(title.c_str(), t, sizeof(t));
      toDisplayable(haveArtist ? artist.c_str() : s.artist, a, sizeof(a));
      if (strcmp(t, s.title) != 0 || strcmp(a, s.artist) != 0) {   // a new song
        strlcpy(s.title, t, sizeof(s.title));
        strlcpy(s.artist, a, sizeof(s.artist));
        s.trackVersion++;
        s.artValid = false;   // the app sends the new cover straight after
      }
      s.hasTrack = true;
    }
    stateUnlock();
  }
};

// Album art from the app: exactly ART_SIZE*ART_SIZE/8 bytes, rows top to bottom,
// 4 bytes per row, most significant bit = leftmost pixel, 1 = lit. Anything else clears it.
class ArtCallbacks : public NimBLECharacteristicCallbacks {
  void onWrite(NimBLECharacteristic *c, NimBLEConnInfo &) override {
    NimBLEAttValue v = c->getValue();
    stateLock();
    AppState &s = stateRef();
    if (v.size() == sizeof(s.art)) {
      memcpy(s.art, v.data(), sizeof(s.art));
      s.artValid = true;
    } else {
      s.artValid = false;
    }
    stateUnlock();
  }
};

class ButtonsCallbacks : public NimBLECharacteristicCallbacks {
  void onSubscribe(NimBLECharacteristic *, NimBLEConnInfo &, uint16_t subValue) override {
    stateLock(); stateRef().appLinked = subValue != 0; stateUnlock();
    Serial.printf("[ble] app %s\n", subValue ? "linked" : "unlinked");
  }
};

static String statusText() {
  AppState st = stateGet();
  String s;
  s.reserve(320);
  s += "fw=" FW_VERSION "\n";
  s += "mode=" + String(WIFI_MODE ? "wifi" : "ble") + "\n";
  s += "bat=" + String(batteryPercent()) + "\n";
  s += "mv=" + String(batteryMv()) + "\n";
  s += "usb=" + String(batteryOnUsb() ? 1 : 0) + "\n";
#if WIFI_MODE
  static const char *wifiNames[] = {"setup", "connecting", "connected", "failed"};
  static const char *spotNames[] = {"idle", "ok", "auth_failed", "error"};
  s += "wifi=" + String(wifiNames[st.wifi]) + "\n";
  s += "ssid=" + String(st.ssid) + "\n";
  s += "ip=" + String(st.ip) + "\n";
  s += "spotify=" + String(spotNames[st.spotify]) + "\n";
#endif
  s += "app=" + String(st.appLinked ? 1 : 0) + "\n";
  s += "playing=" + String(st.playing ? 1 : 0) + "\n";
  s += "title=" + String(st.title) + "\n";
  s += "artist=" + String(st.artist) + "\n";
  s += "next=" + String(st.nextTitle);
  return s;
}

void bleBegin() {
  NimBLEDevice::init(DEVICE_NAME);
  NimBLEDevice::setMTU(247);   // a whole now-playing update fits in one or two packets
  // Bonding + PIN (shown on the OLED) + LE Secure Connections, so nobody nearby can
  // read what you're playing, change settings or skip your songs.
  NimBLEDevice::setSecurityAuth(true, true, true);
  NimBLEDevice::setSecurityIOCap(BLE_HS_IO_DISPLAY_ONLY);

  NimBLEServer *server = NimBLEDevice::createServer();
  server->setCallbacks(new ServerCallbacks());
  NimBLEService *svc = server->createService(BLE_SVC_UUID);

  const uint32_t secureW = NIMBLE_PROPERTY::WRITE | NIMBLE_PROPERTY::WRITE_ENC | NIMBLE_PROPERTY::WRITE_AUTHEN;
  const uint32_t secureRW = secureW | NIMBLE_PROPERTY::READ | NIMBLE_PROPERTY::READ_ENC | NIMBLE_PROPERTY::READ_AUTHEN;
#if WIFI_MODE
  svc->createCharacteristic(BLE_WIFI_UUID, secureRW, 256)->setCallbacks(new WifiCallbacks());
#endif
  svc->createCharacteristic(BLE_SETTINGS_UUID, secureRW, 128)->setCallbacks(new SettingsCallbacks());
  svc->createCharacteristic(BLE_CONTROL_UUID, secureW, 16)->setCallbacks(new ControlCallbacks());
  statusChar = svc->createCharacteristic(
      BLE_STATUS_UUID, NIMBLE_PROPERTY::READ | NIMBLE_PROPERTY::READ_ENC | NIMBLE_PROPERTY::NOTIFY, 512);
  setText(statusChar, statusText());
  svc->createCharacteristic(BLE_NOW_UUID, secureW, 512)->setCallbacks(new NowPlayingCallbacks());
  svc->createCharacteristic(BLE_ART_UUID, secureW, ART_SIZE * ART_SIZE / 8)->setCallbacks(new ArtCallbacks());
  buttonsChar = svc->createCharacteristic(
      BLE_BUTTONS_UUID, NIMBLE_PROPERTY::READ | NIMBLE_PROPERTY::READ_ENC | NIMBLE_PROPERTY::NOTIFY, 16);
  buttonsChar->setCallbacks(new ButtonsCallbacks());
  svc->start();

  NimBLEAdvertising *adv = NimBLEDevice::getAdvertising();
  adv->setName(DEVICE_NAME);
  adv->addServiceUUID(BLE_SVC_UUID);
  adv->enableScanResponse(true);
  adv->setMinInterval(BLE_ADV_MIN_INTERVAL);
  adv->setMaxInterval(BLE_ADV_MAX_INTERVAL);
  adv->start();
  Serial.println("[ble] advertising as " DEVICE_NAME);
}

bool bleHasBonds() { return NimBLEDevice::getNumBonds() > 0; }

bool bleSendToPhone(Command c) {
  if (!buttonsChar || !stateGet().appLinked) return false;
  static const char *names[] = {"toggle", "play", "pause", "next", "prev", "refresh"};
  if (c > CMD_REFRESH) return false;
  setText(buttonsChar, names[c]);
  return buttonsChar->notify();
}

void bleUpdate() {
  if (!statusChar || millis() - lastStatus < 2000) return;
  lastStatus = millis();
  String s = statusText();
  if (s == lastStatusText) return;
  lastStatusText = s;
  setText(statusChar, s);
  statusChar->notify();   // the page re-reads the full value when notified
}
