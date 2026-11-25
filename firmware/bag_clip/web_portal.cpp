// Wi-Fi mode only (WIFI_MODE 1 in config.h).
#include "config.h"
#if WIFI_MODE

#include "web_portal.h"
#include <WiFi.h>
#include <WebServer.h>
#include <ESPmDNS.h>
#include <ArduinoOTA.h>
#include <Update.h>
#include "config.h"
#include "settings.h"
#include "state.h"
#include "battery.h"
#include "ui.h"

#if __has_include("secrets.h")
  #include "secrets.h"
#endif

static WebServer server(80);
static bool started = false;
static const char *ADMIN_USER = "admin";

static String esc(const char *s) {
  String o;
  for (; *s; s++) {
    switch (*s) {
      case '<': o += "&lt;"; break;
      case '>': o += "&gt;"; break;
      case '&': o += "&amp;"; break;
      case '"': o += "&quot;"; break;
      default: o += *s;
    }
  }
  return o;
}

static bool authed() {
  if (server.authenticate(ADMIN_USER, ADMIN_PASSWORD)) return true;
  server.requestAuthentication(BASIC_AUTH, DEVICE_NAME, "Wrong password");
  return false;
}

static void redirectHome() {
  server.sendHeader("Location", "/");
  server.send(303);
}

static void setOta(bool active, int pct) {
  stateLock();
  stateRef().otaActive = active;
  stateRef().otaPercent = pct;
  stateUnlock();
}

static void handleRoot() {
  if (!authed()) return;
  AppState st = stateGet();
  String h;
  h.reserve(4096);
  h += F("<!doctype html><html><head><meta charset=utf-8><meta name=viewport content='width=device-width,initial-scale=1'>"
         "<title>Bag Clip</title><style>body{font-family:system-ui,sans-serif;max-width:480px;margin:0 auto;padding:16px;"
         "background:#111;color:#eee}h1{font-size:1.4em}fieldset{border:1px solid #444;border-radius:8px;margin:12px 0}"
         "label{display:block;margin:8px 0}input,button{font-size:1em}input[type=range]{width:100%}"
         "button{padding:6px 12px;border-radius:6px;border:0;background:#1db954;color:#000}"
         ".del{background:#a33;color:#fff}small{color:#999}</style></head><body><h1>Bag Clip</h1>");

  // Status
  h += "<p>";
  if (st.hasTrack) h += String(st.playing ? "&#9654; " : "&#10074;&#10074; ") + esc(st.title) + " &middot; " + esc(st.artist);
  else h += "Nothing playing";
  if (st.nextTitle[0]) h += "<br><small>Next: " + esc(st.nextTitle) + "</small>";
  h += "<br><small>Battery: ";
  h += batteryPresent() ? (batteryOnUsb() ? String("USB power") : String(batteryPercent()) + "% (" + String(batteryMv()) + " mV)") : String("no divider fitted");
  h += " &middot; Wi-Fi: " + esc(st.ssid) + " &middot; v" FW_VERSION "</small></p>";
  h += F("<form method=post action=/control>"
         "<button name=cmd value=prev>&#9198;</button> <button name=cmd value=toggle>&#9199;</button> "
         "<button name=cmd value=next>&#9197;</button></form>");

  // Settings
  h += F("<form method=post action=/settings><fieldset><legend>Display</legend>");
  h += "<label>Brightness <input type=range name=brightness min=1 max=255 value=" + String(settings.brightness) + "></label>";
  h += "<label>Dim the idle screen after (seconds, 0 = never) <input type=number name=dim min=0 max=3600 value=" + String(settings.dimAfterSec) + "></label>";
  h += "<label>Sleep when nothing plays for (minutes, 0 = never) <input type=number name=sleep min=0 max=240 value=" + String(settings.sleepAfterMin) + "></label>";
  h += "<label>Scroll speed <input type=range name=scroll min=1 max=5 value=" + String(settings.scrollSpeed) + "></label>";
  h += String("<label><input type=checkbox name=art value=1") + (settings.showArt ? " checked" : "") + "> Album art</label>";
  h += F("<button>Save</button></fieldset></form>");

  // Wi-Fi networks
  h += F("<fieldset><legend>Saved Wi-Fi (tried strongest first)</legend>");
  for (int i = 0; i < wifiCount(); i++) {
    WifiNet n;
    if (!wifiGet(i, n)) continue;
    h += "<form method=post action=/wifi/del>" + esc(n.ssid) + " <input type=hidden name=ssid value=\"" + esc(n.ssid) +
         "\"><button class=del>Remove</button></form>";
  }
  h += F("<form method=post action=/wifi/add><label>Network <input name=ssid maxlength=32 required></label>"
         "<label>Password <input name=pass type=password maxlength=64></label><button>Add</button></form>"
         "<small>Up to 5. Adding a 6th replaces the oldest.</small></fieldset>");

  // Firmware update
  h += F("<form method=post action=/update enctype=multipart/form-data><fieldset><legend>Firmware update</legend>"
         "<small>In Arduino IDE: Sketch &gt; Export Compiled Binary, then pick bag_clip.ino.bin.</small>"
         "<label><input type=file name=fw accept=.bin required></label><button>Upload</button></fieldset></form>");
  h += F("</body></html>");
  server.send(200, "text/html", h);
}

static void handleSettings() {
  if (!authed()) return;
  String kv = "brightness=" + server.arg("brightness") + ";dim=" + server.arg("dim") + ";sleep=" + server.arg("sleep") +
              ";scroll=" + server.arg("scroll") + ";art=" + (server.hasArg("art") ? "1" : "0");
  if (!settingsApply(kv)) return (void)server.send(400, "text/plain", "Invalid value");
  redirectHome();
}

static void handleWifiAdd() {
  if (!authed()) return;
  if (!wifiAdd(server.arg("ssid").c_str(), server.arg("pass").c_str()))
    return (void)server.send(400, "text/plain", "Name must be 1-32 characters and password empty or 8-64.");
  sendNetCommand(CMD_WIFI_CHANGED);
  redirectHome();
}

static void handleWifiDel() {
  if (!authed()) return;
  wifiRemove(server.arg("ssid").c_str());
  redirectHome();
}

static void handleControl() {
  if (!authed()) return;
  String c = server.arg("cmd");
  if (c == "toggle") sendCommand(CMD_TOGGLE);
  else if (c == "next") sendCommand(CMD_NEXT);
  else if (c == "prev") sendCommand(CMD_PREV);
  redirectHome();
}

static void handleStatusJson() {
  if (!authed()) return;
  AppState st = stateGet();
  String j = "{\"fw\":\"" FW_VERSION "\",\"battery\":" + String(batteryPercent()) + ",\"mv\":" + String(batteryMv()) +
             ",\"playing\":" + String(st.playing ? "true" : "false") + ",\"settings\":\"" + settingsToString() + "\"}";
  server.send(200, "application/json", j);
}

// Browser upload of a compiled .bin
static void handleUpdateDone() {
  if (!authed()) return;
  bool ok = !Update.hasError();
  server.sendHeader("Connection", "close");
  server.send(ok ? 200 : 500, "text/plain", ok ? "Update OK, restarting..." : "Update failed. Check the .bin file.");
  setOta(false, 0);
  if (ok) {
    delay(500);
    ESP.restart();
  }
}

static void handleUpdateUpload() {
  HTTPUpload &up = server.upload();
  if (!server.authenticate(ADMIN_USER, ADMIN_PASSWORD)) return;   // nothing written without the password
  static unsigned long lastDraw = 0;
  if (up.status == UPLOAD_FILE_START) {
    Serial.printf("[ota] web upload %s\n", up.filename.c_str());
    setOta(true, 0);
    if (!Update.begin(UPDATE_SIZE_UNKNOWN)) Update.printError(Serial);
  } else if (up.status == UPLOAD_FILE_WRITE) {
    if (Update.write(up.buf, up.currentSize) != up.currentSize) Update.printError(Serial);
    if (millis() - lastDraw > 300) {          // the upload blocks loop(), so draw from here
      lastDraw = millis();
      // Size is unknown for multipart uploads; show progress against a ~1.5 MB image.
      setOta(true, min(99, (int)(up.totalSize * 100 / 1500000)));
      uiRender(SCR_OTA, stateGet(), true);
    }
  } else if (up.status == UPLOAD_FILE_END) {
    if (!Update.end(true)) Update.printError(Serial);
  } else if (up.status == UPLOAD_FILE_ABORTED) {
    Update.abort();
    setOta(false, 0);
  }
}

void webBegin() {
  if (started) return;
  started = true;

  // ArduinoOTA: the clip shows up as a network port in Arduino IDE (Tools > Port).
  ArduinoOTA.setHostname(MDNS_NAME);
  ArduinoOTA.setPassword(ADMIN_PASSWORD);
  ArduinoOTA.onStart([]() { setOta(true, 0); uiRender(SCR_OTA, stateGet(), true); });
  ArduinoOTA.onProgress([](unsigned int done, unsigned int total) {
    static int lastPct = -1;
    int pct = total ? (int)(done * 100ULL / total) : 0;
    if (pct == lastPct) return;
    lastPct = pct;
    setOta(true, pct);
    uiRender(SCR_OTA, stateGet(), true);
  });
  ArduinoOTA.onEnd([]() { setOta(true, 100); uiMessage("Update done", "Restarting..."); });
  ArduinoOTA.onError([](ota_error_t e) { Serial.printf("[ota] error %u\n", e); setOta(false, 0); });
  ArduinoOTA.begin();            // also starts mDNS as bagclip.local
  MDNS.addService("http", "tcp", 80);

  server.on("/", HTTP_GET, handleRoot);
  server.on("/settings", HTTP_POST, handleSettings);
  server.on("/wifi/add", HTTP_POST, handleWifiAdd);
  server.on("/wifi/del", HTTP_POST, handleWifiDel);
  server.on("/control", HTTP_POST, handleControl);
  server.on("/status.json", HTTP_GET, handleStatusJson);
  server.on("/update", HTTP_POST, handleUpdateDone, handleUpdateUpload);
  server.begin();
  Serial.printf("[web] http://%s.local  (http://%s)\n", MDNS_NAME, WiFi.localIP().toString().c_str());
}

void webLoop() {
  if (!started) return;
  ArduinoOTA.handle();
  server.handleClient();
}

#endif  // WIFI_MODE
