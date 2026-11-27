#!/usr/bin/env python3
"""Draws the animated idle panda (the same panda as the birthday greeting) and writes it as a GIF.

    python3 tools/draw_panda.py                    # -> docs/images/panda_idle.gif (64x64)
                                                   #    docs/images/panda_idle_preview.gif (4x)
                                                   #    docs/images/panda_poses.png
    python3 tools/gif_to_frames.py docs/images/panda_idle.gif --rest 25
                                                   # -> firmware/bag_clip/panda_anim.h

The panda looks around and blinks, waves, eats a stick of bamboo and dozes off. Every pose
is drawn with panda() from tools/make_oled_panda.py (the greeting's panda) at 4x size and
shrunk to 64x64 with a threshold, so the idle screen and the greeting are the same panda.
Lit (white) pixels are the panda's white fur; black fur is unlit with a lit outline.

Needs Pillow:  pip install pillow
"""
import math
import os
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).parent))
from PIL import Image, ImageDraw
from make_oled_panda import panda

N = 64      # final size
BIG = 256   # drawing size (make_oled_panda's space)


def bamboo(d, x0, y0, x1, y1, leaves=True):
    """A lit bamboo stick with a dark edge (so it shows over the white fur) and dark joints."""
    d.line([(x0, y0), (x1, y1)], fill=0, width=34)
    d.line([(x0, y0), (x1, y1)], fill=255, width=20)
    dx, dy = x1 - x0, y1 - y0
    length = math.hypot(dx, dy)
    px, py = -dy / length, dx / length
    n = max(1, int(length // 26))
    for i in range(1, n + 1):
        t = i / (n + 1)
        cx, cy = x0 + dx * t, y0 + dy * t
        d.line([(cx - px * 8, cy - py * 8), (cx + px * 8, cy + py * 8)], fill=0, width=4)
    if leaves:
        a0 = math.atan2(dy, dx)
        for ang, ln in ((-0.9, 30), (0.55, 26)):
            a = a0 + ang
            tip = (x1 + math.cos(a) * ln, y1 + math.sin(a) * ln)
            d.line([(x1, y1), tip], fill=0, width=17)
            d.line([(x1, y1), tip], fill=255, width=8)


def held_paw(d, x, y):
    """A round paw holding the stick: dark with a lit ring."""
    d.ellipse([x - 20, y - 20, x + 20, y + 20], fill=255)
    d.ellipse([x - 13, y - 13, x + 13, y + 13], fill=0)


def wave_paw(d, x, y):
    """A short arm from the right shoulder straight up to a round paw beside the face."""
    for width, fill in ((52, 255), (36, 0)):
        d.line([(208, 236), (x, y)], fill=fill, width=width)
    d.ellipse([x - 30, y - 30, x + 30, y + 30], fill=255)
    d.ellipse([x - 22, y - 22, x + 22, y + 22], fill=0)
    d.ellipse([x - 7, y - 12, x + 7, y + 2], fill=255)           # paw pad


def zed(d, x, y, s):
    w = max(4, int(s / 4))
    d.line([(x, y), (x + s, y), (x, y + s), (x + s, y + s)], fill=255, width=w, joint="curve")


def pose(eyes="open", mouth="smile", wave=None, stick=None, zs=0):
    im = Image.new("L", (BIG, BIG), 0)
    d = ImageDraw.Draw(im)
    panda(d, eyes=eyes, mouth=mouth)
    if wave:                               # the paw raised beside the face
        wave_paw(d, *wave)
    if stick:                              # bamboo from the lower left up to the mouth
        bamboo(d, *stick)
        held_paw(d, stick[0] + (stick[2] - stick[0]) * 0.35, stick[1] + (stick[3] - stick[1]) * 0.35)
    for i in range(zs):                    # z's rising between the ears, where the cap sat
        s = (16, 18, 20)[i]
        zed(d, (98, 122, 148)[i], (34, 16, 0)[i], s)
    return im.resize((N, N), Image.LANCZOS).point(lambda v: 255 if v >= 128 else 0).convert("1")


def main():
    # unique poses (index = frame number in the header)
    poses = [
        pose(),                                                   # 0 idle
        pose(eyes="closed"),                                      # 1 blink
        pose(eyes="left"),                                        # 2 looks left
        pose(eyes="right"),                                       # 3 looks right
        pose(wave=(222, 150)),                                    # 4 wave, paw out
        pose(wave=(224, 112)),                                    # 5 wave, paw up
        pose(mouth="open", stick=(70, 256, 120, 180, False)),     # 6 bite (long stick)
        pose(mouth="chew", stick=(70, 256, 108, 200, False)),     # 7 chew (shorter)
        pose(eyes="closed", zs=1),                                # 8 sleep z
        pose(eyes="closed", zs=2),                                # 9 sleep zz
        pose(eyes="closed", zs=3),                                # 10 sleep zzz
    ]
    # (pose, ms) - the order it plays in
    seq = [(0, 1600), (2, 700), (0, 300), (3, 700), (0, 400), (1, 150), (0, 700),
           (4, 220), (5, 220), (4, 220), (5, 220), (4, 220), (0, 800),
           (6, 350), (7, 300), (6, 300), (7, 300), (6, 350), (7, 400), (0, 1000),
           (1, 150), (0, 500), (1, 150), (0, 400),
           (1, 600), (8, 600), (9, 600), (10, 900), (8, 600), (9, 600), (10, 900), (1, 500)]
    frames = [poses[i] for i, _ in seq]
    durs = [ms for _, ms in seq]
    out = os.path.join(os.path.dirname(__file__), "..", "docs", "images")
    os.makedirs(out, exist_ok=True)
    # disposal=1 and no optimisation so every GIF frame is a full picture
    pal = [f.convert("L") for f in frames]
    pal[0].save(os.path.join(out, "panda_idle.gif"), save_all=True, append_images=pal[1:],
                duration=durs, loop=0, disposal=1, optimize=False)
    big = [f.resize((N * 4, N * 4), Image.NEAREST).convert("L") for f in frames]
    big[0].save(os.path.join(out, "panda_idle_preview.gif"), save_all=True, append_images=big[1:],
                duration=durs, loop=0, disposal=1, optimize=False)
    sheet = Image.new("L", (len(poses) * (N + 4) * 3, N * 3), 60)
    for i, pz in enumerate(poses):
        sheet.paste(pz.resize((N * 3, N * 3), Image.NEAREST).convert("L"), (i * (N + 4) * 3, 0))
    sheet.save(os.path.join(out, "panda_poses.png"))
    print(f"{len(poses)} poses, {len(seq)} steps, {sum(durs) / 1000:.1f} s loop -> docs/images/panda_idle.gif")


if __name__ == "__main__":
    main()
