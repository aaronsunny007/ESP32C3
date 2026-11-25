#include "battery.h"
#include "config.h"

static int smoothedMv = 0;
static unsigned long lastRead = 0;
static int lowReadings = 0;

// Resting LiPo voltage -> charge. Approximate; under Wi-Fi load the voltage sags ~50 mV.
static const int CURVE_MV[]  = {3450, 3600, 3700, 3750, 3800, 3850, 3900, 4000, 4100, 4200};
static const int CURVE_PCT[] = {   0,    8,   20,   30,   42,   55,   65,   80,   92,  100};
static const int CURVE_N = sizeof(CURVE_MV) / sizeof(CURVE_MV[0]);

static int readOnceMv() {
  uint32_t sum = 0;
  const int samples = 16;
  for (int i = 0; i < samples; i++) sum += analogReadMilliVolts(BATT_PIN);  // factory-calibrated
  return (int)((sum / samples) * BATT_DIVIDER * BATT_CAL);
}

void batteryBegin() {
  if (!BATT_FITTED) return;   // no divider: nothing to read, the icon stays hidden
  pinMode(BATT_PIN, INPUT);
  analogSetPinAttenuation(BATT_PIN, ADC_11db);   // C3 measures up to ~2.5 V at the pin
  smoothedMv = readOnceMv();
  lastRead = millis();
}

void batteryUpdate() {
  if (!BATT_FITTED || millis() - lastRead < BATT_READ_MS) return;
  lastRead = millis();
  int mv = readOnceMv();
  // Jumps (plugging USB in or out) are taken straight away, small noise is smoothed.
  smoothedMv = (abs(mv - smoothedMv) > 300) ? mv : (smoothedMv * 3 + mv) / 4;
  lowReadings = (batteryPresent() && !batteryOnUsb() && smoothedMv < BATT_CRITICAL_MV) ? lowReadings + 1 : 0;
}

int  batteryMv()      { return smoothedMv; }
bool batteryPresent() { return BATT_FITTED && smoothedMv >= BATT_MISSING_MV; }
bool batteryOnUsb()   { return smoothedMv >= BATT_USB_MV; }
bool batteryCritical(){ return lowReadings >= 3; }   // 3 readings in a row, ~15 s

int batteryPercent() {
  if (!batteryPresent()) return -1;
  int mv = smoothedMv;
  if (mv <= CURVE_MV[0]) return 0;
  if (mv >= CURVE_MV[CURVE_N - 1]) return 100;
  for (int i = 1; i < CURVE_N; i++) {
    if (mv <= CURVE_MV[i]) {
      return CURVE_PCT[i - 1] + (long)(mv - CURVE_MV[i - 1]) * (CURVE_PCT[i] - CURVE_PCT[i - 1]) /
                                    (CURVE_MV[i] - CURVE_MV[i - 1]);
    }
  }
  return 100;
}
