#!/usr/bin/env python3
"""Turn a PNG/JPG into a 1-bit PROGMEM array for the bag clip's OLED (Adafruit GFX drawBitmap).

Usage:
    python3 tools/png_to_bitmap.py my_image.png NAME [--size 64x64] [--invert] [--threshold 128]

Prints a C array named NAME (plus NAME_W / NAME_H) to stdout. Lit OLED pixels are the
*bright* parts of the image, so draw your picture white-on-black (or pass --invert).
For the animated idle screen use tools/gif_to_frames.py instead.

Needs Pillow:  pip install pillow
"""
import argparse
from PIL import Image


def to_array(img, name):
    w, h = img.size
    px = img.load()
    out = [f"// {w}x{h}, 1 bit per pixel, rows MSB-first (Adafruit GFX drawBitmap format)",
           f"#define {name}_W {w}", f"#define {name}_H {h}",
           f"const uint8_t {name}[] PROGMEM = {{"]
    for y in range(h):
        row = []
        for xb in range(0, w, 8):
            byte = 0
            for bit in range(8):
                x = xb + bit
                if x < w and px[x, y]:
                    byte |= 0x80 >> bit
            row.append(f"0x{byte:02X}")
        out.append("  " + ", ".join(row) + ",")
    out.append("};")
    return "\n".join(out)


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("image")
    ap.add_argument("name")
    ap.add_argument("--size", default="64x64", help="WxH to fit into (keeps aspect ratio)")
    ap.add_argument("--invert", action="store_true", help="light up the dark parts instead")
    ap.add_argument("--threshold", type=int, default=128)
    ap.add_argument("--dither", action="store_true", help="Floyd-Steinberg dither (photos)")
    a = ap.parse_args()

    w, h = (int(v) for v in a.size.lower().split("x"))
    img = Image.open(a.image).convert("L")
    img.thumbnail((w, h), Image.LANCZOS)
    canvas = Image.new("L", (w, h), 255 if a.invert else 0)
    canvas.paste(img, ((w - img.width) // 2, (h - img.height) // 2))
    if a.invert:
        canvas = canvas.point(lambda v: 255 - v)
    bw = canvas.convert("1") if a.dither else canvas.point(lambda v: 255 if v >= a.threshold else 0).convert("1")
    print(to_array(bw, a.name))


if __name__ == "__main__":
    main()
