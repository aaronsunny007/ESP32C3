#!/usr/bin/env python3
"""Draws the default animated idle panda and writes it as a GIF.

    python3 tools/draw_panda.py                    # -> docs/images/panda_idle.gif (64x64)
                                                   #    docs/images/panda_idle_preview.gif (4x)
    python3 tools/gif_to_frames.py docs/images/panda_idle.gif --rest 9
                                                   # -> firmware/bag_clip/panda_anim.h

The panda blinks, waves, eats bamboo and dozes off. Each pose is drawn at 8x size and
scaled down to 64x64 with a threshold, so it stays crisp on the 1-bit OLED.
Lit (white) pixels are the panda's white fur; black fur is unlit with a lit outline.

Needs Pillow:  pip install pillow
"""
import math
import os
from PIL import Image, ImageDraw

S = 8            # drawing scale
N = 64           # final size


class Pose:
    def __init__(self):
        self.im = Image.new("1", (N * S, N * S), 0)
        self.d = ImageDraw.Draw(self.im)

    def ell(self, cx, cy, rx, ry, fill):
        self.d.ellipse([(cx - rx) * S, (cy - ry) * S, (cx + rx) * S, (cy + ry) * S], fill=fill)

    def line(self, pts, width, fill):
        self.d.line([(x * S, y * S) for x, y in pts], fill=fill, width=int(width * S), joint="curve")
        r = width / 2
        for x, y in (pts[0], pts[-1]):
            self.ell(x, y, r, r, fill)

    def limb(self, pts, width=6.0):
        """A black arm/leg: lit outline, dark inside."""
        self.line(pts, width + 2.6, 1)
        self.line(pts, width, 0)

    def bamboo(self, x0, y0, x1, y1, leaves=True):
        self.line([(x0, y0), (x1, y1)], 5.4, 0)     # dark edge so it shows over white fur
        self.line([(x0, y0), (x1, y1)], 3.2, 1)
        n = max(1, int(math.hypot(x1 - x0, y1 - y0) // 6))
        dx, dy = (x1 - x0), (y1 - y0)
        L = math.hypot(dx, dy)
        px, py = -dy / L, dx / L            # perpendicular
        for i in range(1, n + 1):           # dark segment joints
            t = i / (n + 1)
            cx, cy = x0 + dx * t, y0 + dy * t
            self.d.line([((cx - px * 1.7) * S, (cy - py * 1.7) * S),
                         ((cx + px * 1.7) * S, (cy + py * 1.7) * S)], fill=0, width=int(0.9 * S))
        if leaves:
            for ang, ln in ((-0.9, 7), (0.5, 6)):
                a = math.atan2(dy, dx) + ang
                tip = (x1 + math.cos(a) * ln, y1 + math.sin(a) * ln)
                self.line([(x1, y1), tip], 4.0, 0)
                self.line([(x1, y1), tip], 2.2, 1)

    def z(self, x, y, s):
        w = 1.3
        self.line([(x, y), (x + s, y)], w, 1)
        self.line([(x + s, y), (x, y + s)], w, 1)
        self.line([(x, y + s), (x + s, y + s)], w, 1)

    def done(self):
        small = self.im.convert("L").resize((N, N), Image.LANCZOS)
        return small.point(lambda v: 255 if v > 110 else 0).convert("1")


def panda(eyes="open", arms="down", mouth="smile", bamboo=None, zs=0):
    p = Pose()
    # ears: lit ring, dark middle
    for cx in (17, 47):
        p.ell(cx, 9, 7, 7, 1)
        p.ell(cx, 9, 4, 4, 0)
    # body, with a dark gap so the head stands out
    p.ell(32, 54, 18, 15, 1)
    # legs
    p.limb([(21, 62), (23, 66)], 7)
    p.limb([(43, 62), (41, 66)], 7)
    # arms
    if arms == "down":
        p.limb([(17, 44), (21, 55)])
        p.limb([(47, 44), (43, 55)])
    elif arms in ("wave1", "wave2"):
        p.limb([(17, 44), (21, 55)])
        tip = (58, 28) if arms == "wave1" else (61, 35)
        p.limb([(46, 44), tip])
    elif arms == "hold":
        p.limb([(17, 44), (25, 47)])
        p.limb([(47, 44), (39, 49)])
    # head (dark ring first so it separates from ears and body)
    p.ell(32, 23, 20, 17, 0)
    p.ell(32, 23, 18, 15, 1)
    # eye patches: tilted dark ovals
    for sx in (1, -1):
        patch = Image.new("1", (9 * S, 12 * S), 0)
        ImageDraw.Draw(patch).ellipse([0, 0, 9 * S - 1, 12 * S - 1], fill=1)
        patch = patch.rotate(35 * sx, expand=True)
        cx, cy = 32 - 8 * sx, 22
        p.im.paste(0, (int(cx * S - patch.width / 2), int(cy * S - patch.height / 2)), patch)
        if eyes == "open":
            p.ell(cx, cy, 2.4, 2.4, 1)
            p.ell(cx - 0.6 * sx, cy - 0.8, 0.9, 0.9, 0)
        else:  # closed: a little lit curve
            p.d.arc([(cx - 3) * S, (cy - 2.5) * S, (cx + 3) * S, (cy + 2) * S], 20, 160, fill=1, width=int(1.3 * S))
    # nose
    p.ell(32, 29.5, 2.6, 1.7, 0)
    if mouth == "smile":
        p.d.arc([29 * S, 29.5 * S, 35 * S, 34.5 * S], 20, 160, fill=0, width=int(1.2 * S))
    elif mouth == "open":
        p.ell(32, 33.5, 2.4, 2.2, 0)
    elif mouth == "chew":
        p.line([(30, 33.5), (34, 33.5)], 1.1, 0)
    if bamboo:
        p.bamboo(*bamboo)
        # paws over the stick
        p.ell(25, 47, 3.4, 3.4, 1); p.ell(25, 47, 2.2, 2.2, 0)
        p.ell(39, 49, 3.4, 3.4, 1); p.ell(39, 49, 2.2, 2.2, 0)
    for i in range(zs):  # sleepy z's, growing up and to the right
        s = 3.5 + i * 1.2
        p.z((53, 56, 58.5)[i], (31, 23, 15)[i], s)
    return p.done()


def main():
    # unique poses (index = frame number in the header)
    poses = [
        panda(),                                                    # 0 idle
        panda(eyes="closed"),                                       # 1 blink
        panda(arms="wave1"),                                        # 2 wave
        panda(arms="wave2"),                                        # 3 wave
        panda(arms="hold", mouth="open", bamboo=(22, 54, 38, 26)),  # 4 bite (long stick)
        panda(arms="hold", mouth="chew", bamboo=(22, 54, 37, 29)),  # 5 chew
        panda(arms="hold", mouth="open", bamboo=(22, 54, 33, 34, False)),  # 6 bite (shorter)
        panda(arms="hold", mouth="chew", bamboo=(22, 54, 30, 40, False)),  # 7 chew (stub)
        panda(eyes="closed", zs=1),                                 # 8 sleep z
        panda(eyes="closed", zs=2),                                 # 9 sleep zz
        panda(eyes="closed", zs=3),                                 # 10 sleep zzz
    ]
    # (pose, ms) - the order it plays in
    seq = [(0, 1600), (1, 150), (0, 900),
           (2, 220), (3, 220), (2, 220), (3, 220), (2, 220), (0, 800),
           (4, 350), (5, 300), (4, 300), (5, 300), (6, 350), (7, 300), (6, 300), (7, 400), (0, 1000),
           (1, 150), (0, 500), (1, 150), (0, 400),
           (1, 600), (8, 600), (9, 600), (10, 900), (8, 600), (9, 600), (10, 900), (1, 500), (0, 900)]
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
