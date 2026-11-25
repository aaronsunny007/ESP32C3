#include "settings.h"
#include <Preferences.h>

Settings settings;

static Preferences prefs;
static SemaphoreHandle_t lock;
static WifiNet nets[MAX_WIFI_NETWORKS];
static int netCount = 0;
static volatile uint32_t netVersion = 1;

static void saveNetsLocked() {
  prefs.putUChar("wn", netCount);
  for (int i = 0; i < MAX_WIFI_NETWORKS; i++) {
    char ks[4] = {'s', char('0' + i), 0}, kp[4] = {'p', char('0' + i), 0};
    if (i < netCount) {
      prefs.putString(ks, nets[i].ssid);
      prefs.putString(kp, nets[i].pass);
    } else {
      prefs.remove(ks);
      prefs.remove(kp);
    }
  }
  netVersion++;
}

void settingsBegin() {
  lock = xSemaphoreCreateMutex();
  prefs.begin("bagclip", false);
  settings.brightness    = prefs.getUChar("bright", DEFAULT_BRIGHTNESS);
  settings.dimAfterSec   = prefs.getUShort("dim", DEFAULT_DIM_SEC);
  settings.sleepAfterMin = prefs.getUShort("sleep", DEFAULT_SLEEP_MIN);
  settings.scrollSpeed   = prefs.getUChar("scroll", DEFAULT_SCROLL);
  settings.showArt       = prefs.getBool("art", DEFAULT_SHOW_ART);

  netCount = min<int>(prefs.getUChar("wn", 0), MAX_WIFI_NETWORKS);
  for (int i = 0; i < netCount; i++) {
    char ks[4] = {'s', char('0' + i), 0}, kp[4] = {'p', char('0' + i), 0};
    strlcpy(nets[i].ssid, prefs.getString(ks, "").c_str(), sizeof(nets[i].ssid));
    strlcpy(nets[i].pass, prefs.getString(kp, "").c_str(), sizeof(nets[i].pass));
  }
}

void settingsSave() {
  xSemaphoreTake(lock, portMAX_DELAY);
  prefs.putUChar("bright", settings.brightness);
  prefs.putUShort("dim", settings.dimAfterSec);
  prefs.putUShort("sleep", settings.sleepAfterMin);
  prefs.putUChar("scroll", settings.scrollSpeed);
  prefs.putBool("art", settings.showArt);
  xSemaphoreGive(lock);
}

bool settingsApply(const String &kv) {
  Settings s = settings;
  bool ok = true;
  int start = 0;
  while (start < (int)kv.length()) {
    int end = kv.indexOf(';', start);
    if (end < 0) end = kv.length();
    String pair = kv.substring(start, end);
    start = end + 1;
    pair.trim();
    if (pair.isEmpty()) continue;
    int eq = pair.indexOf('=');
    if (eq < 0) { ok = false; continue; }
    String k = pair.substring(0, eq), v = pair.substring(eq + 1);
    k.trim(); v.trim();
    long n = v.toInt();
    if (k == "brightness" && n >= 1 && n <= 255)      s.brightness = n;
    else if (k == "dim" && n >= 0 && n <= 3600)       s.dimAfterSec = n;
    else if (k == "sleep" && n >= 0 && n <= 240)      s.sleepAfterMin = n;
    else if (k == "scroll" && n >= 1 && n <= 5)       s.scrollSpeed = n;
    else if (k == "art" && (v == "0" || v == "1"))    s.showArt = (n == 1);
    else ok = false;
  }
  if (ok) {
    settings = s;
    settingsSave();
  }
  return ok;
}

String settingsToString() {
  char buf[96];
  snprintf(buf, sizeof(buf), "brightness=%u;dim=%u;sleep=%u;scroll=%u;art=%d",
           settings.brightness, settings.dimAfterSec, settings.sleepAfterMin,
           settings.scrollSpeed, settings.showArt ? 1 : 0);
  return String(buf);
}

int wifiCount() {
  xSemaphoreTake(lock, portMAX_DELAY);
  int n = netCount;
  xSemaphoreGive(lock);
  return n;
}

bool wifiGet(int index, WifiNet &out) {
  xSemaphoreTake(lock, portMAX_DELAY);
  bool ok = index >= 0 && index < netCount;
  if (ok) out = nets[index];
  xSemaphoreGive(lock);
  return ok;
}

bool wifiAdd(const char *ssid, const char *pass) {
  size_t sl = strlen(ssid), pl = strlen(pass);
  if (sl == 0 || sl > 32 || pl > 64 || (pl > 0 && pl < 8)) return false;  // WPA2 needs 8-63 (64 = hex key)
  xSemaphoreTake(lock, portMAX_DELAY);
  int slot = -1;
  for (int i = 0; i < netCount; i++)
    if (strcmp(nets[i].ssid, ssid) == 0) slot = i;
  if (slot < 0) {
    if (netCount == MAX_WIFI_NETWORKS) {   // full: drop the oldest
      memmove(&nets[0], &nets[1], sizeof(WifiNet) * (MAX_WIFI_NETWORKS - 1));
      netCount--;
    }
    slot = netCount++;
  }
  strlcpy(nets[slot].ssid, ssid, sizeof(nets[slot].ssid));
  strlcpy(nets[slot].pass, pass, sizeof(nets[slot].pass));
  saveNetsLocked();
  xSemaphoreGive(lock);
  return true;
}

bool wifiRemove(const char *ssid) {
  xSemaphoreTake(lock, portMAX_DELAY);
  bool found = false;
  for (int i = 0; i < netCount; i++) {
    if (strcmp(nets[i].ssid, ssid) == 0) {
      memmove(&nets[i], &nets[i + 1], sizeof(WifiNet) * (netCount - i - 1));
      netCount--;
      found = true;
      break;
    }
  }
  if (found) saveNetsLocked();
  xSemaphoreGive(lock);
  return found;
}

String wifiListSsids(char sep) {
  String s;
  xSemaphoreTake(lock, portMAX_DELAY);
  for (int i = 0; i < netCount; i++) {
    if (i) s += sep;
    s += nets[i].ssid;
  }
  xSemaphoreGive(lock);
  return s;
}

uint32_t wifiListVersion() { return netVersion; }

// The stored token is only used while secrets.h still holds the token it came from,
// so pasting a new token into secrets.h always wins.
static String tokenTag(const char *fromSecrets) {
  return String(fromSecrets).substring(0, 12);
}

String storedRefreshToken(const char *fromSecrets) {
  xSemaphoreTake(lock, portMAX_DELAY);
  String tag = prefs.getString("rttag", "");
  String rt = prefs.getString("rt", "");
  xSemaphoreGive(lock);
  return (tag == tokenTag(fromSecrets) && rt.length()) ? rt : String(fromSecrets);
}

void storeRefreshToken(const char *fromSecrets, const String &token) {
  xSemaphoreTake(lock, portMAX_DELAY);
  prefs.putString("rttag", tokenTag(fromSecrets));
  prefs.putString("rt", token);
  xSemaphoreGive(lock);
}
