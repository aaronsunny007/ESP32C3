// Album art: decode Spotify's 64x64 JPEG thumbnail and dither it to a
// black-and-white ART_SIZE x ART_SIZE bitmap for the OLED.
#pragma once
#include <Arduino.h>
#include "state.h"

// Returns true and fills out (ART_SIZE*ART_SIZE/8 bytes, drawBitmap format) on success.
bool artConvertJpeg(const uint8_t *jpg, size_t len, uint8_t *out);
