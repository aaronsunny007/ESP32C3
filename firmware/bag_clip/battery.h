// LiPo battery level from a 100k/100k voltage divider on BATT_PIN.
#pragma once
#include <Arduino.h>

void batteryBegin();
void batteryUpdate();        // call from loop(); samples every BATT_READ_MS
int  batteryMv();            // battery voltage in millivolts (smoothed)
int  batteryPercent();       // 0-100, or -1 when no divider is fitted
bool batteryPresent();       // false on the USB bench test without the divider
bool batteryOnUsb();         // 5 V from USB instead of the LiPo
bool batteryCritical();      // low enough that the clip should sleep to protect itself
