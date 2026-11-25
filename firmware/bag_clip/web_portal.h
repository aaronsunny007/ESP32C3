// Settings web page (http://bagclip.local or the IP on the info screen) and
// over-the-air firmware updates, both over Wi-Fi and protected by ADMIN_PASSWORD.
#pragma once
#include <Arduino.h>

void webBegin();   // call once Wi-Fi is up
void webLoop();    // call from loop()
