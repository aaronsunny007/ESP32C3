#include "art.h"
#include <TJpg_Decoder.h>

static uint8_t gray[ART_SIZE * ART_SIZE];
static int16_t offX = 0, offY = 0;

// TJpg_Decoder hands us the picture in small RGB565 blocks.
static bool onBlock(int16_t x, int16_t y, uint16_t w, uint16_t h, uint16_t *px) {
  for (int j = 0; j < h; j++) {
    int gy = y + j + offY;
    if (gy < 0 || gy >= ART_SIZE) continue;
    for (int i = 0; i < w; i++) {
      int gx = x + i + offX;
      if (gx < 0 || gx >= ART_SIZE) continue;
      uint16_t c = px[j * w + i];
      int r = (c >> 11) << 3, g = ((c >> 5) & 0x3F) << 2, b = (c & 0x1F) << 3;
      gray[gy * ART_SIZE + gx] = (r * 77 + g * 150 + b * 29) >> 8;
    }
  }
  return true;
}

bool artConvertJpeg(const uint8_t *jpg, size_t len, uint8_t *out) {
  uint16_t w = 0, h = 0;
  if (TJpgDec.getJpgSize(&w, &h, jpg, len) != JDR_OK || w == 0 || h == 0) return false;

  // The decoder can shrink by 1, 2, 4 or 8. Pick the smallest shrink that fits.
  uint8_t scale = 1;
  while (scale < 8 && (w / scale > ART_SIZE || h / scale > ART_SIZE)) scale <<= 1;
  int sw = w / scale, sh = h / scale;
  offX = (ART_SIZE - min(sw, ART_SIZE)) / 2;   // centre if smaller (or crop if larger)
  offY = (ART_SIZE - min(sh, ART_SIZE)) / 2;
  if (sw > ART_SIZE) offX = -(sw - ART_SIZE) / 2;
  if (sh > ART_SIZE) offY = -(sh - ART_SIZE) / 2;

  memset(gray, 0, sizeof(gray));
  TJpgDec.setJpgScale(scale);
  TJpgDec.setSwapBytes(false);
  TJpgDec.setCallback(onBlock);
  if (TJpgDec.drawJpg(0, 0, jpg, len) != JDR_OK) return false;

  // Stretch contrast so dark album covers still show detail.
  uint8_t lo = 255, hi = 0;
  for (uint8_t v : gray) { lo = min(lo, v); hi = max(hi, v); }
  int range = max(1, hi - lo);

  // Floyd-Steinberg dithering to 1 bit.
  static int16_t err[ART_SIZE * ART_SIZE];
  for (int i = 0; i < ART_SIZE * ART_SIZE; i++) err[i] = (int16_t)((gray[i] - lo) * 255 / range);
  memset(out, 0, ART_SIZE * ART_SIZE / 8);
  for (int y = 0; y < ART_SIZE; y++) {
    for (int x = 0; x < ART_SIZE; x++) {
      int i = y * ART_SIZE + x;
      int v = err[i];
      int lit = v >= 128 ? 255 : 0;
      if (lit) out[y * (ART_SIZE / 8) + x / 8] |= 0x80 >> (x & 7);
      int e = v - lit;
      if (x + 1 < ART_SIZE) err[i + 1] += e * 7 / 16;
      if (y + 1 < ART_SIZE) {
        if (x > 0) err[i + ART_SIZE - 1] += e * 3 / 16;
        err[i + ART_SIZE] += e * 5 / 16;
        if (x + 1 < ART_SIZE) err[i + ART_SIZE + 1] += e * 1 / 16;
      }
    }
  }
  return true;
}
