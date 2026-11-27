#!/usr/bin/env python3
"""Draws the power-on greeting for the 0.96" OLED and writes firmware/bag_clip/greeting_img.h.

What the clip shows at switch-on (one tap moves to the next step):
  1. Panda wearing its cap, "Hpy bday" / "Kusuma."
  2. Tap: the panda's paw reaches up, takes the cap off and lifts it away (animation)
  3. Panda without the cap, "From your" / "lovely" / "panda"
  4. Tap: the normal clip screens

HOW TO EDIT (everything you are likely to change is in the "EDIT HERE" block below):
  - the words on each screen ........ SCREEN_1_LINES / SCREEN_2_LINES (1 to 4 short lines)
  - how the paw moves, and how fast .. ANIMATION (steps with a duration in milliseconds)
  - the drawing itself ............... panda(), hat(), paw() further down
Then run, from the repository folder:
    python3 tools/make_oled_panda.py
and flash firmware/bag_clip again. Check the result first in docs/images/:
greeting_1_preview.png, greeting_2_preview.png and greeting_anim_preview.gif (OLED look, enlarged).
To switch the greeting off without deleting anything: GREETING_ENABLED 0 in firmware/bag_clip/config.h.
Needs Pillow (pip install pillow).
"""
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).parent))
from PIL import Image, ImageDraw, ImageFont
from png_to_bitmap import to_array

# ============================== EDIT HERE ==============================

SCREEN_1_LINES = ["Hpy bday", "Kusuma."]
SCREEN_2_LINES = ["From your", "lovely", "panda"]

# The cap animation, step by step. Positions are in the 256x256 drawing space of the
# panda (it is shrunk to 64x64 for the OLED): x to the right, y downwards.
#   paw   = where the paw is at the end of the step (None = paw out of sight)
#   hat   = how far the cap has moved from its place on the head (dx, dy)
#   frames, ms = how many pictures the step uses and how long each one is shown
# Every picture costs 512 bytes of flash; 20-30 pictures is fine.
PAW_HIDDEN = (214, 300)          # below the screen edge: where the paw starts and ends
PAW_ON_BRIM = (172, 60)          # holding the right end of the cap's brim
ANIMATION = [
    dict(paw=PAW_ON_BRIM, hat=(0, 0),     frames=6, ms=45),   # paw reaches up to the cap
    dict(paw=PAW_ON_BRIM, hat=(0, 0),     frames=1, ms=300),  # ...holds it for a moment
    dict(paw=(184, -70),  hat=(12, -130), frames=8, ms=45),   # lifts it up and away
    dict(paw=PAW_HIDDEN,  hat=(12, -130), frames=6, ms=45),   # paw goes back down
]

# =======================================================================

ROOT = Path(__file__).resolve().parent.parent
OUT_H = ROOT / "firmware" / "bag_clip" / "greeting_img.h"
PREVIEW_DIR = ROOT / "docs" / "images"
BOLD = "/usr/share/fonts/truetype/dejavu/DejaVuSans-Bold.ttf"
TEXT_X, TEXT_W = 66, 61          # right half of the screen, 1 px margin
SHOULDER = (200, 236)            # the arm grows from the body here (lower right)


def panda(d):
    """The panda without the cap, lit = white, on a 256x256 drawing."""
    def e(cx, cy, rx, ry, fill, outline=None, w=0):
        d.ellipse([cx - rx, cy - ry, cx + rx, cy + ry], fill=fill, outline=outline, width=w)

    e(128, 246, 70, 56, 255)                                     # body
    e(52, 66, 32, 32, 0, 255, 9); e(204, 66, 32, 32, 0, 255, 9)  # ears: dark, lit outline
    e(128, 128, 94, 74, 255)                                     # head
    e(91, 128, 24, 30, 0); e(165, 128, 24, 30, 0)                # eye patches
    e(92, 124, 9, 10, 255); e(164, 124, 9, 10, 255)              # eyes
    e(128, 152, 13, 9, 0)                                        # nose
    d.arc([104, 150, 128, 178], 20, 160, fill=0, width=8)        # smile
    d.arc([128, 150, 152, 178], 20, 160, fill=0, width=8)
    d.polygon([(128, 208), (88, 188), (88, 228)], fill=0)        # bow tie
    d.polygon([(128, 208), (168, 188), (168, 228)], fill=0)
    e(128, 208, 11, 13, 0)
    d.ellipse([122, 202, 134, 214], fill=255)


def hat(d, dx=0, dy=0):
    """The cap: clear its outline (so the ears don't show through), then draw it lit."""
    crown = [98 + dx, 4 + dy, 158 + dx, 58 + dy]
    brim = [78 + dx, 54 + dy, 178 + dx, 66 + dy]
    d.rounded_rectangle([crown[0] - 3, crown[1] - 3, crown[2] + 3, brim[3] + 4], radius=8, fill=0)
    d.rounded_rectangle([brim[0] - 3, brim[1] - 3, brim[2] + 3, brim[3] + 4], radius=7, fill=0)
    d.rounded_rectangle(crown, radius=6, fill=255)
    d.rounded_rectangle(brim, radius=5, fill=255)
    d.rectangle([98 + dx, 34 + dy, 158 + dx, 46 + dy], fill=0)   # dark band


def paw(d, x, y):
    """The panda's arm from the shoulder to (x, y), bending at the elbow so it goes round
    the outside of the head: dark with a lit outline, round paw with a pad."""
    elbow = ((SHOULDER[0] + x) / 2 + 42, (SHOULDER[1] + y) / 2)
    arm = [SHOULDER, elbow, (x, y)]
    for r, fill in ((32, 255), (21, 0)):              # outline first, then the dark arm
        d.line(arm, fill=fill, width=2 * r, joint="curve")
        for px, py in arm:
            d.ellipse([px - r, py - r, px + r, py + r], fill=fill)
    d.ellipse([x - 8, y - 12, x + 8, y + 2], fill=255)           # paw pad


def picture(paw_at=None, hat_at=(0, 0), with_hat=True):
    """One 64x64 frame of the panda's square."""
    im = Image.new("L", (256, 256), 0)
    d = ImageDraw.Draw(im)
    panda(d)
    if with_hat:
        hat(d, *hat_at)
    if paw_at is not None:
        paw(d, *paw_at)
    return im.resize((64, 64), Image.LANCZOS).point(lambda v: 255 if v >= 128 else 0).convert("1")


def animation_frames():
    """Expand ANIMATION into (picture, ms) pairs, easing in and out within each step."""
    out = []
    p0, h0 = PAW_HIDDEN, (0, 0)
    ease = lambda t: t * t * (3 - 2 * t)
    for step in ANIMATION:
        p1, h1 = step["paw"], step["hat"]
        for i in range(1, step["frames"] + 1):
            t = ease(i / step["frames"])
            p = (p0[0] + (p1[0] - p0[0]) * t, p0[1] + (p1[1] - p0[1]) * t)
            h = (round(h0[0] + (h1[0] - h0[0]) * t), round(h0[1] + (h1[1] - h0[1]) * t))
            out.append((picture(p, h), step["ms"]))
        p0, h0 = p1, h1
    return out


def fit_font(lines):
    """Largest bold font (max 15 px) where every line fits the text column."""
    for size in range(15, 7, -1):
        f = ImageFont.truetype(BOLD, size)
        if all(f.getbbox(t)[2] <= TEXT_W for t in lines):
            return f
    return ImageFont.truetype(BOLD, 8)


def screen(square, lines):
    img = Image.new("1", (128, 64), 0)
    img.paste(square, (0, 0))
    d = ImageDraw.Draw(img)
    f = fit_font(lines)
    asc, desc = f.getmetrics()
    step = asc + desc + 2
    y = (64 - step * len(lines) + 2) // 2
    for t in lines:
        d.text((TEXT_X, y), t, font=f, fill=1)
        y += step
    return img


def oled_look(img, z):
    big = img.convert("L").resize((img.width * z, img.height * z), Image.NEAREST)
    return Image.merge("RGB", [big.point(lambda v: 140 if v else 3),
                               big.point(lambda v: 196 if v else 6),
                               big.point(lambda v: 255 if v else 12)])


def main():
    s1 = screen(picture(), SCREEN_1_LINES)
    s2 = screen(picture(with_hat=False), SCREEN_2_LINES)
    frames = animation_frames()

    h = ["// Power-on greeting for the OLED. GENERATED by tools/make_oled_panda.py:",
         "// edit the text and the animation there, run it, and flash again.",
         "#pragma once", ""]
    h += ["// Screen 1: " + " / ".join(SCREEN_1_LINES) + " (panda wearing its cap)", to_array(s1, "GREET_BDAY"), ""]
    h += ["// Screen 2: " + " / ".join(SCREEN_2_LINES) + " (panda without the cap)", to_array(s2, "GREET_FROM"), ""]
    h += ["// Animation between them: the panda's square (top left 64x64) only; the text stays.",
          "#define GREET_ANIM_W 64", "#define GREET_ANIM_H 64", f"#define GREET_ANIM_COUNT {len(frames)}",
          "const uint16_t GREET_ANIM_MS[GREET_ANIM_COUNT] PROGMEM = {"
          + ", ".join(str(ms) for _, ms in frames) + "};",
          "const uint8_t GREET_ANIM_DATA[GREET_ANIM_COUNT][512] PROGMEM = {"]
    for i, (img, _) in enumerate(frames):
        body = to_array(img, "X").split("{", 1)[1].rsplit("}", 1)[0]
        h.append(f"  {{ // frame {i}" + body + "  },")
    h += ["};", ""]
    OUT_H.write_text("\n".join(h))

    oled_look(s1, 6).save(PREVIEW_DIR / "greeting_1_preview.png")
    oled_look(s2, 6).save(PREVIEW_DIR / "greeting_2_preview.png")
    gif, ms = [oled_look(s1, 4)], [1500]
    for img, d in frames:
        f = s1.copy()
        f.paste(img, (0, 0))
        gif.append(oled_look(f, 4)); ms.append(d)
    gif.append(oled_look(s2, 4)); ms.append(2000)
    gif[0].save(PREVIEW_DIR / "greeting_anim_preview.gif", save_all=True, append_images=gif[1:], duration=ms, loop=0)
    print(f"wrote {OUT_H.relative_to(ROOT)} ({len(frames)} animation frames, "
          f"{sum(m for _, m in frames)} ms) and previews in docs/images/")


if __name__ == "__main__":
    main()
