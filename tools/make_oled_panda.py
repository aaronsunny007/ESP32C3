"""Draws the OLED panda and writes panda_64.h / splash_128x64.h (+ previews) into the current folder.
Copy splash_128x64.h to firmware/bag_clip/splash_img.h and rename SPLASH_IMG... only if you change the name.
Needs Pillow."""
import sys
from pathlib import Path
sys.path.insert(0, str(Path(__file__).parent))
from PIL import Image, ImageDraw, ImageFont
from png_to_bitmap import to_array

K = 4          # supersample so edges downscale cleanly
N = 64 * K

def panda():
    im = Image.new("L", (N, N), 0)
    d = ImageDraw.Draw(im)
    s = N / 256
    def e(cx, cy, rx, ry, fill, outline=None, w=0):
        d.ellipse([(cx-rx)*s, (cy-ry)*s, (cx+rx)*s, (cy+ry)*s], fill=fill, outline=outline, width=int(w*s))
    # body (lit) + arms/feet (dark with lit outline)
    e(128, 246, 70, 56, 255)
    # ears: dark inside, lit outline
    e(52, 66, 32, 32, 0, 255, 9); e(204, 66, 32, 32, 0, 255, 9)
    # head
    e(128, 128, 94, 74, 255)
    # eye patches (dark) + lit eyes
    e(91, 128, 24, 30, 0); e(165, 128, 24, 30, 0)
    e(92, 124, 9, 10, 255); e(164, 124, 9, 10, 255)
    # nose + smile
    e(128, 152, 13, 9, 0)
    d.arc([104*s, 150*s, 128*s, 178*s], 20, 160, fill=0, width=int(8*s))
    d.arc([128*s, 150*s, 152*s, 178*s], 20, 160, fill=0, width=int(8*s))
    # top hat: dark with lit outline and lit band
    d.rounded_rectangle([98*s, 4*s, 158*s, 58*s], radius=6*s, fill=255)
    d.rectangle([98*s, 34*s, 158*s, 46*s], fill=0)
    d.rounded_rectangle([78*s, 54*s, 178*s, 66*s], radius=5*s, fill=255)
    d.rectangle([78*s, 66*s, 178*s, 68*s], fill=0)
    # bow tie (dark on the lit chest), tucked under the chin
    d.polygon([(128*s,208*s),(88*s,188*s),(88*s,228*s)], fill=0)
    d.polygon([(128*s,208*s),(168*s,188*s),(168*s,228*s)], fill=0)
    e(128, 208, 11, 13, 0)
    d.ellipse([122*s, 202*s, 134*s, 214*s], fill=255)
    return im.resize((64, 64), Image.LANCZOS).point(lambda v: 255 if v >= 128 else 0).convert("1")

p = panda()
p.save("panda_64.png")

# 128x64 splash: panda left, wordmark right
sp = Image.new("1", (128, 64), 0)
sp.paste(p, (0, 0))
d = ImageDraw.Draw(sp)
try:
    f1 = ImageFont.truetype("/usr/share/fonts/truetype/dejavu/DejaVuSans-Bold.ttf", 14)
    f2 = ImageFont.truetype("/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf", 9)
except Exception:
    f1 = f2 = ImageFont.load_default()
d.text((68, 18), "Bag", font=f1, fill=1)
d.text((68, 33), "Clip", font=f1, fill=1)
d.text((68, 51), "now playing", font=f2, fill=1)
sp.save("splash_128x64.png")

open("panda_64.h", "w").write("// Kawaii panda with hat and bow tie for the 0.96\" SSD1306 OLED\n" + to_array(p, "PANDA_IMG") + "\n")
open("splash_128x64.h", "w").write("// Bag Clip splash screen, full 128x64 OLED\n" + to_array(sp, "SPLASH_IMG") + "\n")

# preview: OLED-look, 6x, with pixel grid
def preview(img, name):
    z = 6
    big = img.convert("L").resize((img.width*z, img.height*z), Image.NEAREST).convert("RGB")
    px = big.load()
    for y in range(big.height):
        for x in range(big.width):
            lit = px[x, y][0] > 0
            gap = (x % z == 0) or (y % z == 0)
            px[x, y] = (6, 14, 28) if gap else ((140, 196, 255) if lit else (3, 6, 12))
    out = Image.new("RGB", (big.width+24, big.height+24), (20, 20, 24)); out.paste(big, (12, 12)); out.save(name)
preview(p, "panda_64_preview.png"); preview(sp, "splash_128x64_preview.png")
print("ok")
