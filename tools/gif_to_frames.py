#!/usr/bin/env python3
"""Turn an animated GIF into the idle-screen animation header for the bag clip.

    python3 tools/gif_to_frames.py my.gif                       # -> firmware/bag_clip/panda_anim.h
    python3 tools/gif_to_frames.py my.gif --invert --dither     # dark drawing on white / photos
    python3 tools/gif_to_frames.py my.gif --max-frames 12 --size 64x64 --preview check.gif

What it does:
  * reads every GIF frame with its delay (full frames, so GIFs that only store the
    changed area still come out right)
  * shrinks each frame to fit the box (default 64x64, the left half of the screen),
    keeping the shape, centred on black
  * turns it into 1 bit: bright pixels light up (use --invert for black-on-white art,
    --dither for photos, --threshold to tune)
  * merges repeated pictures, so a 30-step GIF with 10 different pictures stores 10
  * if there are more than --max-frames different pictures, keeps that many spread
    evenly through the GIF and gives the dropped frames' time to the ones kept

Each 64x64 picture takes 512 bytes of flash (the clip has ~1.9 MB for the program);
no RAM is used, the pictures are drawn straight from flash.

Needs Pillow:  pip install pillow
"""
import argparse
import os
from PIL import Image, ImageOps, ImageSequence

HERE = os.path.dirname(os.path.abspath(__file__))
DEFAULT_OUT = os.path.join(HERE, "..", "firmware", "bag_clip", "panda_anim.h")
MIN_MS = 60          # the screen can't usefully go faster than this
MAX_MS = 65000       # stored as uint16_t


def load(path, invert):
    im = Image.open(path)
    frames = []
    for fr in ImageSequence.Iterator(im):
        ms = fr.info.get("duration", im.info.get("duration", 100)) or 100
        rgba = fr.convert("RGBA")
        off = 255 if invert else 0                               # transparent -> "off" pixels
        bg = Image.new("RGBA", rgba.size, (off, off, off, 255))
        frames.append((Image.alpha_composite(bg, rgba).convert("L"), int(ms)))
    return frames


def to_1bit(img, w, h, args):
    if args.invert:
        img = ImageOps.invert(img)
    fitted = ImageOps.contain(img, (w, h), Image.LANCZOS)
    canvas = Image.new("L", (w, h), 0)
    canvas.paste(fitted, ((w - fitted.width) // 2, (h - fitted.height) // 2))
    if args.dither:
        return canvas.convert("1")                               # Floyd-Steinberg
    return canvas.point(lambda v: 255 if v > args.threshold else 0).convert("1")


def pack(img):
    w, h = img.size
    px = img.load()
    out = bytearray()
    for y in range(h):
        for xb in range(0, w, 8):
            b = 0
            for bit in range(8):
                if xb + bit < w and px[xb + bit, y]:
                    b |= 0x80 >> bit
            out.append(b)
    return bytes(out)


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("gif")
    ap.add_argument("-o", "--out", default=DEFAULT_OUT, help="header to write (default: firmware/bag_clip/panda_anim.h)")
    ap.add_argument("--size", default="64x64", help="WxH box to fit into (default 64x64)")
    ap.add_argument("--max-frames", type=int, default=24, help="most different pictures to keep (default 24 = 12 KB)")
    ap.add_argument("--threshold", type=int, default=110, help="0-255, brighter than this lights up (default 110)")
    ap.add_argument("--invert", action="store_true", help="light up the dark parts (black-on-white drawings)")
    ap.add_argument("--dither", action="store_true", help="dither instead of a hard threshold (photos)")
    ap.add_argument("--speed", type=float, default=1.0, help="play this many times faster (0.5 = half speed)")
    ap.add_argument("--rest", type=int, default=0, help="step shown, frozen, while the screen is dimmed (default 0)")
    ap.add_argument("--preview", help="also write what the OLED will show as a 4x GIF here")
    args = ap.parse_args()

    w, h = (int(v) for v in args.size.lower().split("x"))
    src = load(args.gif, args.invert)
    if not src:
        raise SystemExit("no frames in " + args.gif)

    # 1-bit frames, then fold consecutive identical ones together
    steps = []
    for img, ms in src:
        bits = to_1bit(img, w, h, args)
        data = pack(bits)
        if steps and steps[-1][0] == data:
            steps[-1][1] += ms
        else:
            steps.append([data, ms, bits])
    if len(steps) > 1 and steps[0][0] == steps[-1][0]:            # loop seam
        steps[0][1] += steps.pop()[1]

    # unique pictures
    uniq = []
    for s in steps:
        if s[0] not in uniq:
            uniq.append(s[0])

    if len(uniq) > args.max_frames:
        keep_idx = {round(i * (len(uniq) - 1) / (args.max_frames - 1)) for i in range(args.max_frames)} \
            if args.max_frames > 1 else {0}
        keep = [u for i, u in enumerate(uniq) if i in keep_idx]
        merged = []
        for s in steps:
            if s[0] in keep or not merged:
                if s[0] not in keep:            # very first step dropped: borrow the first kept picture
                    s = [keep[0], s[1], None]
                merged.append(list(s))
            else:
                merged[-1][1] += s[1]           # dropped picture: give its time to the previous step
        steps, uniq = merged, keep
        print(f"note: kept {len(uniq)} of the GIF's different pictures (--max-frames)")

    seq = []
    for s in steps:
        ms = int(round(s[1] / args.speed))
        seq.append((uniq.index(s[0]), max(MIN_MS, min(MAX_MS, ms))))
    rest = min(max(args.rest, 0), len(seq) - 1)
    nbytes = (w + 7) // 8 * h

    name = os.path.basename(args.gif)
    lines = [
        f"// Idle-screen animation: {len(uniq)} pictures of {w}x{h}, {len(seq)} steps, "
        f"{sum(ms for _, ms in seq) / 1000:.1f} s per loop, {len(uniq) * nbytes} bytes of flash.",
        f"// Generated from {name} by tools/gif_to_frames.py. Do not edit by hand:",
        f"// re-run the script with your own GIF to replace it (see PROJECT.md, \"Your own animation\").",
        "#pragma once",
        "#include <Arduino.h>",
        "",
        f"#define PANDA_ANIM_W      {w}",
        f"#define PANDA_ANIM_H      {h}",
        f"#define PANDA_ANIM_FRAMES {len(uniq)}   // different pictures",
        f"#define PANDA_ANIM_STEPS  {len(seq)}   // entries in PANDA_ANIM_SEQ",
        f"#define PANDA_ANIM_REST   {rest}   // step shown (frozen) while the screen is dimmed",
        "",
        "struct PandaStep { uint8_t frame; uint16_t ms; };",
        "",
        "// play order: which picture, and for how long",
        "const PandaStep PANDA_ANIM_SEQ[PANDA_ANIM_STEPS] PROGMEM = {",
    ]
    for i in range(0, len(seq), 6):
        lines.append("  " + " ".join(f"{{{f}, {ms}}}," for f, ms in seq[i:i + 6]))
    lines += ["};", "",
              "// 1 bit per pixel, rows MSB-first (Adafruit GFX drawBitmap format)",
              f"const uint8_t PANDA_ANIM_FRAMES_DATA[PANDA_ANIM_FRAMES][{nbytes}] PROGMEM = {{"]
    per_row = (w + 7) // 8
    for i, data in enumerate(uniq):
        lines.append(f"  {{ // {i}")
        for r in range(0, len(data), per_row):
            lines.append("    " + ", ".join(f"0x{b:02X}" for b in data[r:r + per_row]) + ",")
        lines.append("  },")
    lines.append("};")
    os.makedirs(os.path.dirname(os.path.abspath(args.out)), exist_ok=True)
    with open(args.out, "w") as f:
        f.write("\n".join(lines) + "\n")
    print(f"{len(uniq)} pictures, {len(seq)} steps, {len(uniq) * nbytes} bytes -> {args.out}")

    if args.preview:
        def unpack(data):
            img = Image.new("L", (w, h), 0)
            px = img.load()
            for y in range(h):
                for x in range(w):
                    if data[y * per_row + x // 8] & (0x80 >> (x % 8)):
                        px[x, y] = 255
            return img.resize((w * 4, h * 4), Image.NEAREST)
        pics = [unpack(uniq[f]) for f, _ in seq]
        pics[0].save(args.preview, save_all=True, append_images=pics[1:],
                     duration=[ms for _, ms in seq], loop=0, disposal=1, optimize=False)
        print("preview ->", args.preview)


if __name__ == "__main__":
    main()
