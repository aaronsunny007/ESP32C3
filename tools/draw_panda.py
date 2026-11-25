#!/usr/bin/env python3
"""Draws the default panda (eyes open + blink frame) used on the idle screen.
Run: python3 tools/draw_panda.py  -> writes panda_0.png, panda_1.png and 8x previews.
"""
from PIL import Image, ImageDraw
def panda(blink=False, S=8):
    W=64*S
    im=Image.new('1',(W,W),0); d=ImageDraw.Draw(im)
    def e(x0,y0,x1,y1,f): d.ellipse([x0*S,y0*S,x1*S,y1*S],fill=f)
    # ears (lit ring + dark centre gives "black ear" look)
    for cx in (13,51):
        e(cx-11,4,cx+11,26,1); e(cx-7,8,cx+7,22,0)
    # head: lit, with dark gap around so ears separate
    e(4,10,60,60,0)
    e(6,12,58,58,1)
    # eye patches (dark), tilted ovals
    for sx in (1,-1):
        patch=Image.new('1',(14*S,20*S),0); pd=ImageDraw.Draw(patch)
        pd.ellipse([0,0,14*S-1,20*S-1],fill=1)
        patch=patch.rotate(30*sx,expand=True)
        cx=32-11*sx; cy=34
        im.paste(0,(int(cx*S-patch.width/2),int(cy*S-patch.height/2)),patch)
        if blink:
            d.line([( (cx-4)*S, (cy+1)*S),((cx+4)*S,(cy+1)*S)],fill=1,width=int(1.6*S))
        else:
            e(cx-3.5,cy-3.5,cx+3.5,cy+3.5,1); e(cx-0.5+sx*-1,cy-2.5,cx+1.5+sx*-1,cy-0.5,0)
    # nose + mouth
    e(28,42,36,47,0)
    d.line([(32*S,46*S),(32*S,50*S)],fill=0,width=int(1.3*S))
    d.arc([27*S,46*S,32*S,52*S],10,170,fill=0,width=int(1.3*S))
    d.arc([32*S,46*S,37*S,52*S],10,170,fill=0,width=int(1.3*S))
    # cheeks hint
    return im.convert('L').resize((64,64),Image.LANCZOS).point(lambda v:255 if v>110 else 0).convert('1')
for b in (False,True):
    p=panda(b); p.save(f'panda_{int(b)}.png')
    p.resize((512,512),Image.NEAREST).save(f'preview_{int(b)}.png')
