#include "ble_link.h"
#include <NimBLEDevice.h>
#include "config.h"
#include "settings.h"
#include "state.h"
#include "battery.h"

static NimBLECharacteristic *statusChar = nullptr;
static unsigned long lastStatus = 0;
static String lastStatusText;

static String charText(NimBLECharacteristic *c) {
  return String(c->getValue().c_str());
}

static void setText(NimBLECharacteristic *c, const String &s) {
  c->setValue((const uint8_t *)s.c_str(), s.length());
}

class ServerCallbacks : public NimBLEServerCallbacks {
  void onConnect(NimBLEServer *, NimBLEConnInfo &) override {
    stateLock(); stateRef().bleConnected = true; stateUnlock();
  }
  void onDisconnect(NimBLEServer *, NimBLEConnInfo &, int) override {
    stateLock(); stateRef().bleConnected = false; stateRef().blePasskey = 0; stateUnlock();
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
    if (!info.isEncrypted()) NimBLEDevice::getServer()->disconnect(info.getConnHandle());
  }
};

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
    if (ok) sendCommand(CMD_WIFI_CHANGED);
  }
};

class SettingsCallbacks : public NimBLECharacteristicCallbacks {
  void onRead(NimBLECharacteristic *c, NimBLEConnInfo &) override { setText(c, settingsToString()); }
  void onWrite(NimBLECharacteristic *c, NimBLEConnInfo &) override {
    // The display picks up new values on its next frame. A rejected write changes
    // nothing; the page notices because reading back gives the old values.
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

static String statusText() {
  AppState st = stateGet();
  static const char *wifiNames[] = {"setup", "connecting", "connected", "failed"};
  static const char *spotNames[] = {"idle", "ok", "auth_failed", "error"};
  String s;
  s.reserve(256);
  s += "fw=" FW_VERSION "\n";
  s += "bat=" + String(batteryPercent()) + "\n";
  s += "mv=" + String(batteryMv()) + "\n";
  s += "usb=" + String(batteryOnUsb() ? 1 : 0) + "\n";
  s += "wifi=" + String(wifiNames[st.wifi]) + "\n";
  s += "ssid=" + String(st.ssid) + "\n";
  s += "ip=" + String(st.ip) + "\n";
  s += "spotify=" + String(spotNames[st.spotify]) + "\n";
  s += "playing=" + String(st.playing ? 1 : 0) + "\n";
  s += "title=" + String(st.title) + "\n";
  s += "artist=" + String(st.artist) + "\n";
  s += "next=" + String(st.nextTitle);
  return s;
}

void bleBegin() {
  NimBLEDevice::init(DEVICE_NAME);
  NimBLEDevice::setMTU(185);
  // Bonding + PIN (shown on the OLED) + LE Secure Connections, so Wi-Fi passwords
  // travel encrypted and strangers can't change settings or skip your songs.
  NimBLEDevice::setSecurityAuth(true, true, true);
  NimBLEDevice::setSecurityIOCap(BLE_HS_IO_DISPLAY_ONLY);

  NimBLEServer *server = NimBLEDevice::createServer();
  server->setCallbacks(new ServerCallbacks());
  NimBLEService *svc = server->createService(BLE_SVC_UUID);

  const uint32_t secureRW = NIMBLE_PROPERTY::READ | NIMBLE_PROPERTY::READ_ENC | NIMBLE_PROPERTY::READ_AUTHEN |
                            NIMBLE_PROPERTY::WRITE | NIMBLE_PROPERTY::WRITE_ENC | NIMBLE_PROPERTY::WRITE_AUTHEN;
  svc->createCharacteristic(BLE_WIFI_UUID, secureRW, 256)->setCallbacks(new WifiCallbacks());
  svc->createCharacteristic(BLE_SETTINGS_UUID, secureRW, 128)->setCallbacks(new SettingsCallbacks());
  svc->createCharacteristic(BLE_CONTROL_UUID,
                            NIMBLE_PROPERTY::WRITE | NIMBLE_PROPERTY::WRITE_ENC | NIMBLE_PROPERTY::WRITE_AUTHEN, 16)
      ->setCallbacks(new ControlCallbacks());
  statusChar = svc->createCharacteristic(
      BLE_STATUS_UUID, NIMBLE_PROPERTY::READ | NIMBLE_PROPERTY::READ_ENC | NIMBLE_PROPERTY::NOTIFY, 512);
  setText(statusChar, statusText());
  svc->start();

  NimBLEAdvertising *adv = NimBLEDevice::getAdvertising();
  adv->setName(DEVICE_NAME);
  adv->addServiceUUID(BLE_SVC_UUID);
  adv->enableScanResponse(true);
  adv->start();
  Serial.println("[ble] advertising as " DEVICE_NAME);
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
