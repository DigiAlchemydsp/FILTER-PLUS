#!/usr/bin/env python3
"""Generate the FILTER + scrolling type-name GIF."""
import os
from PIL import Image, ImageChops, ImageDraw, ImageFont

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
OUT = os.path.join(ROOT, 'shots', 'lowcpu')
TYPES = ['TRASH', 'COMB', 'BP2', 'BP', 'HP1', 'LP1', 'HP4-', 'LP4-',
         'HP2', 'LP2', 'NOTCH', 'COMB-', 'TRASH-']

BG = (10, 10, 12)
FG = (245, 245, 245)
ACCENT = (255, 198, 72)
MUTED = (120, 120, 126)


def font(size):
    for path in ('C:/Windows/Fonts/arialbd.ttf', 'C:/Windows/Fonts/segoeuib.ttf',
                 'C:/Windows/Fonts/arial.ttf'):
        if os.path.isfile(path):
            return ImageFont.truetype(path, size)
    return ImageFont.load_default()


def main():
    os.makedirs(OUT, exist_ok=True)
    f = font(96)
    small = font(24)
    prefix = 'FILTER'
    probe = Image.new('RGB', (10, 10), BG)
    pd = ImageDraw.Draw(probe)
    pw = int(pd.textlength(prefix, font=f))
    sw = int(max(pd.textlength(t, font=f) for t in TYPES))
    x0, pad = 40, 40
    W = x0 + pw + sw + pad
    H = 300
    cy = H // 2
    row = 108
    suffix_x = x0 + pw
    base = Image.new('RGBA', (W, H), BG + (255,))
    bd = ImageDraw.Draw(base)
    bd.text((x0, cy), prefix, font=f, fill=FG, anchor='lm')
    bd.text((x0, H - 32), 'Filter+  -  13 types, every one cheaper than stock',
            font=small, fill=MUTED, anchor='lm')
    # soft vertical window around the fixed word
    mask = Image.new('L', (W, H), 0)
    mp = mask.load()
    inner, outer = 0.20 * row, 0.70 * row
    for y in range(H):
        d = abs(y - cy)
        if d <= inner:
            a = 255
        elif d >= outer:
            a = 0
        else:
            a = int(255 * (outer - d) / (outer - inner))
        for x in range(W):
            mp[x, y] = a
    frames, durations = [], []
    hold, trans = 5, 5
    for i, name in enumerate(TYPES):
        nxt = TYPES[(i + 1) % len(TYPES)]
        for k in range(hold):
            progress = 0.0
            ov = Image.new('RGBA', (W, H), (0, 0, 0, 0))
            od = ImageDraw.Draw(ov)
            od.text((suffix_x, cy), name, font=f, fill=ACCENT, anchor='lm')
            ov.putalpha(ImageChops.multiply(ov.getchannel('A'), mask))
            frames.append(Image.alpha_composite(base, ov).convert('RGB'))
            durations.append(40)
        for k in range(trans):
            p = (k + 1) / trans
            ov = Image.new('RGBA', (W, H), (0, 0, 0, 0))
            od = ImageDraw.Draw(ov)
            od.text((suffix_x, cy + p * row), name, font=f, fill=ACCENT, anchor='lm')
            od.text((suffix_x, cy - (1 - p) * row), nxt, font=f, fill=ACCENT, anchor='lm')
            ov.putalpha(ImageChops.multiply(ov.getchannel('A'), mask))
            frames.append(Image.alpha_composite(base, ov).convert('RGB'))
            durations.append(40)
    pal = frames[0].quantize(colors=32, method=Image.MEDIANCUT)
    frames = [fr.quantize(palette=pal, dither=Image.NONE) for fr in frames]
    dest = os.path.join(OUT, 'FILTER-plus-types.gif')
    frames[0].save(dest, save_all=True, append_images=frames[1:], duration=durations,
                   loop=0, optimize=True, disposal=2)
    print('wrote', dest, len(frames), 'frames', '%dx%d' % (W, H))


if __name__ == '__main__':
    main()
