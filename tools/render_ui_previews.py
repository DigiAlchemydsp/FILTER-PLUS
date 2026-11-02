#!/usr/bin/env python3
"""Render deterministic FLTR-page previews for the Filter+ TYPE list.

The curve math and the TYPE glyph bitmaps are parsed from the adapter sources,
so these previews follow the same code the firmware runs. Outputs go to
shots/lowcpu/ (individual panels + contact sheets).
"""
import os
import re
from PIL import Image, ImageDraw, ImageFont

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
DT_UI = os.path.join(ROOT, 'filterplus-dt', 'filter_ui.c')
TABLES = os.path.join(ROOT, 'shared', 'filter_tables.h')
OUT = os.path.join(ROOT, 'shots', 'lowcpu')

GX0, GX1, GY0, GY1 = 20, 71, 7, 23
TBX0, TBX1, TBY0, TBY1 = 77, 93, 7, 23
CH = GY1 - GY0 - 4

MASK = 0xFFFFFFFF


def u32(v):
    return v & MASK


def s32(v):
    v &= MASK
    return v - 0x100000000 if v & 0x80000000 else v


def parse_array(path, name):
    text = open(path, encoding='utf-8').read()
    m = re.search(name + r'\s*\[[^\]]*\]\s*=\s*\{(.*?)\};', text, re.S)
    if not m:
        raise SystemExit('array not found: %s in %s' % (name, path))
    body = m.group(1)
    return [int(tok, 0) for tok in re.findall(r'-?0x[0-9a-fA-F]+|-?\d+', body)]


G27 = parse_array(TABLES, 'G27')
K27 = parse_array(TABLES, 'K27')
LOG2Q8 = parse_array(DT_UI, 'LOG2Q8')
COS15 = parse_array(DT_UI, 'COS15')
GLYPHS = {n: parse_array(DT_UI, n) for n in ('COMB_GLYPH', 'TRASH_GLYPH')}
DIGITS = {1: [2, 6, 2, 2, 7], 2: [7, 1, 7, 4, 7], 4: [5, 5, 7, 1, 1]}


def mulsh(a, b, sh):
    ua = u32(-a) if a < 0 else u32(a)
    ub = u32(-b) if b < 0 else u32(b)
    al, ah = ua & 0xffff, ua >> 16
    bl, bh = ub & 0xffff, ub >> 16
    ll = u32(al * bl)
    lh = u32(al * bh)
    hl = u32(ah * bl)
    hh = u32(ah * bh)
    mid = u32((ll >> 16) + (lh & 0xffff) + (hl & 0xffff))
    lo = u32((ll & 0xffff) | u32(mid << 16))
    hi = u32(hh + (lh >> 16) + (hl >> 16) + (mid >> 16))
    if (a < 0) != (b < 0):
        lo = u32(~lo + 1)
        hi = u32(~hi + (1 if lo == 0 else 0))
    return s32(u32(u32(hi << (32 - sh)) | (lo >> sh)))


def udivq16(x, y):
    rem, q = u32(x), 0
    for _ in range(16):
        rem = u32(rem << 1)
        q = u32(q << 1)
        if rem >= y:
            rem = u32(rem - y)
            q |= 1
    return q


def log2q8(v):
    p = 31
    while not (v >> p):
        p -= 1
    m = v >> (p - 8) if p >= 8 else v << (8 - p)
    return p * 256 + LOG2Q8[m & 255]


def bp_db2(fi, qi, x, wide):
    g = G27[fi] << 1 if wide else G27[fi]
    q = qi >> 1 if wide else qi
    k = (K27[q] >> 11) * (1 if wide else 3) // (2 if wide else 4)
    G = G27[x]
    t = udivq16(G, u32(g)) if G <= g else udivq16(u32(g), G)
    num = mulsh(k, t, 16)
    den = (1 << 16) - mulsh(t, t, 16)
    if den < 0:
        den = -den
    n = mulsh(num, num, 16)
    d = mulsh(den, den, 16) + n
    if n <= 0:
        return -200
    if d <= 0:
        return 200
    return ((log2q8(n) - log2q8(d)) * 1541 + 32768) >> 16


def comb_mag(c, g):
    gc = mulsh(g, c, 15)
    den = (1 << 15) + mulsh(g, g, 15) - 2 * gc
    if den < 0:
        den = -den
    if den < 1:
        den = 1
    l = log2q8(den)
    if l > 4600:
        l = 4600
    return 4600 - l


def comb_dstep(fi):
    return (5 << 12) + fi * ((64 << 12) - (5 << 12)) // 127


def comb_db2(fi, fb, x):
    idx = ((x * comb_dstep(fi)) >> 12) & 255
    return comb_mag(COS15[idx], fb * 235)


def trash_db2(fi, fb, x):
    idx = ((x * comb_dstep(fi) * 3 // 2) >> 12) & 255
    return comb_mag(COS15[idx], fb * 130)


def col2step(x):
    return (x - GX0 - 1) * 127 // (GX1 - GX0 - 2)


def smooth3(curve):
    for _ in range(3):
        prev = curve[0]
        for x in range(1, 127):
            cur = curve[x]
            curve[x] = (prev + 2 * cur + curve[x + 1]) // 4
            prev = cur


def lc_curve(mode, fi):
    slope = 4 if mode in ('LP4-', 'HP4-') else 2 if mode in ('LP2', 'HP2', 'NOTCH') else 1
    span = 96 // slope
    curve = [0] * 128
    for x in range(128):
        if mode == 'NOTCH':
            d = abs(x - fi)
            v = CH - ((10 - d) * CH // 10 if d < 10 else 0)
        elif mode in ('LP1', 'LP4-', 'LP2'):
            d = x - fi
            v = CH if d <= 0 else CH - d * CH // span
        else:
            d = fi - x
            v = CH if d <= 0 else CH - d * CH // span
        curve[x] = max(0, min(CH, v))
    smooth3(curve)
    return curve


def build_curve(mode, fi, qi):
    curve = [0] * 128
    if mode in ('BP', 'BP2'):
        wide = mode == 'BP2'
        depth = CH * (5 + qi) // 20
        for x in range(128):
            d = bp_db2(fi, qi, x, wide)
            if d > 0:
                d = 0
            curve[x] = depth * 50 // (50 - d)
        smooth3(curve)
        lo, hi = max(2, fi - 4), min(125, fi + 4)
        for x in range(lo, hi + 1):
            curve[x] = (curve[x - 2] + 2 * curve[x - 1] + 3 * curve[x] +
                        2 * curve[x + 1] + curve[x + 2]) // 9
        lowpass = False
    elif mode in ('COMB', 'TRASH', 'COMB-', 'TRASH-'):
        comb = mode in ('COMB', 'COMB-')
        base = 240 if comb else 120
        for x in range(128):
            v = comb_db2(fi, qi, x) if comb else trash_db2(fi, qi, x)
            d = v - base
            curve[x] = CH * max(0, min(1500, d)) // 1500
        lowpass = False
    else:
        curve = lc_curve(mode, fi)
        lowpass = mode in ('LP1', 'LP4-', 'LP2')
    return curve, lowpass


def curve_y(s, v, lowpass):
    if s < 8 and not lowpass:
        w = s
    elif s > 119:
        w = 127 - s
    else:
        w = 8
    return GY0 + 2 + v * w // 8


def glyph_order(mode):
    if mode in ('LP1', 'HP1'):
        return 1
    if mode in ('LP4-', 'HP4-'):
        return 4
    if mode in ('COMB', 'TRASH', 'COMB-', 'TRASH-'):
        return 0
    return 2


def glyph_px(d, ox, base, x, y):
    d.point((ox + x, base - y), fill=(240, 240, 240))


def draw_glyph(d, mode, ox, base):
    if mode in ('COMB', 'COMB-', 'TRASH', 'TRASH-'):
        g = GLYPHS['COMB_GLYPH'] if mode in ('COMB', 'COMB-') else GLYPHS['TRASH_GLYPH']
        for r in range(17):
            for x in range(17):
                if g[r] & (1 << x):
                    glyph_px(d, ox, base, x, r)
        return
    for x in range(1, 16):
        glyph_px(d, ox, base, x, 0)
        glyph_px(d, ox, base, x, 16)
    for y in range(1, 16):
        glyph_px(d, ox, base, 0, y)
        glyph_px(d, ox, base, 16, y)
    prev = None
    for x in range(1, 16):
        dlt = x - 8
        if mode in ('BP', 'BP2'):
            y = 10 - dlt * dlt // (6 if mode == 'BP2' else 3)
        elif mode in ('LP1', 'LP4-', 'LP2'):
            ord_ = 1 if mode == 'LP1' else 4 if mode == 'LP4-' else 2
            y = 10 if dlt <= 0 else 10 - dlt * ord_ // 2
        elif mode in ('HP1', 'HP4-', 'HP2'):
            ord_ = 1 if mode == 'HP1' else 4 if mode == 'HP4-' else 2
            y = 10 if -dlt <= 0 else 10 - (-dlt) * ord_ // 2
        else:
            ad = abs(dlt)
            y = 10 - (4 - ad) * (4 - ad) * 9 // 16 if ad < 4 else 10
        y = max(1, min(10, y))
        if prev is None:
            prev = y
        for yy in range(min(prev, y), max(prev, y) + 1):
            glyph_px(d, ox, base, x, yy)
        prev = y
    order = glyph_order(mode)
    if order:
        pat = DIGITS[order]
        for r in range(5):
            for c in range(3):
                if pat[r] & (4 >> c):
                    glyph_px(d, ox, base, 2 + c, 15 - r)


def panel(machine, ty, mode, fi=64, qi=10, fb=64):
    curve, lowpass = build_curve(mode, fi, qi if mode not in
                                 ('COMB', 'TRASH', 'COMB-', 'TRASH-') else fb)
    img = Image.new('RGB', (128, 64), (12, 12, 14))
    d = ImageDraw.Draw(img)
    font = ImageFont.load_default()
    d.rectangle([0, 0, 127, 63], outline=(72, 72, 78))
    d.text((2, 1), '%s  %02d  %s' % (machine, ty, mode), fill=(240, 240, 240), font=font)
    label = 'F%d  FB%d' % (fi, fb) if mode in ('COMB', 'TRASH', 'COMB-', 'TRASH-') else 'F%d  R%d' % (fi, qi)
    d.text((2, 10), label, fill=(150, 150, 155), font=font)
    d.rectangle([GX0, 63 - GY1, GX1, 63 - GY0], outline=(90, 90, 96))
    prev = None
    for x in range(GX0 + 1, GX1):
        y = curve_y(col2step(x), curve[col2step(x)], lowpass)
        if prev is not None:
            for yy in range(min(prev, y), max(prev, y) + 1):
                d.point((x, 63 - yy), fill=(255, 210, 96))
        prev = y
    draw_glyph(d, mode, TBX0, 63 - TBY0)
    return img


DT_MODES = [(8, 'BP'), (9, 'BP2'), (10, 'COMB'), (11, 'TRASH'),
            (12, 'LP1'), (13, 'HP1'), (14, 'LP4-'), (15, 'HP4-'),
            (16, 'LP2'), (17, 'HP2'), (18, 'NOTCH'), (19, 'COMB-'), (20, 'TRASH-')]
DN_MODES = [(4, 'BP'), (5, 'BP2'), (6, 'COMB'), (7, 'TRASH'),
            (8, 'LP1'), (9, 'HP1'), (10, 'LP4-'), (11, 'HP4-'),
            (12, 'LP2'), (13, 'HP2'), (14, 'NOTCH'), (15, 'COMB-'), (16, 'TRASH-')]


def save_set(machine, modes, folder):
    os.makedirs(folder, exist_ok=True)
    imgs = []
    for ty, mode in modes:
        img = panel(machine, ty, mode)
        img.resize((128 * 6, 64 * 6), Image.NEAREST).save(
            os.path.join(folder, '%02d-%s.png' % (ty, mode)))
        imgs.append((ty, mode, img))
    cols, scale, pad = 4, 3, 12
    pw, ph = 128 * scale, 64 * scale
    rows = (len(imgs) + cols - 1) // cols
    sheet = Image.new('RGB', (cols * (pw + pad) + pad, rows * (ph + 34) + pad + 20),
                      (238, 238, 238))
    d = ImageDraw.Draw(sheet)
    try:
        title_font = ImageFont.truetype('C:/Windows/Fonts/arial.ttf', 16)
    except OSError:
        title_font = ImageFont.load_default()
    d.text((pad, 4), '%s FLTR - low-CPU bank' % machine, fill=(20, 20, 20), font=title_font)
    for i, (ty, mode, img) in enumerate(imgs):
        x = pad + (i % cols) * (pw + pad)
        y = 24 + pad + (i // cols) * (ph + 34)
        sheet.paste(img.resize((pw, ph), Image.NEAREST), (x, y))
        d.text((x, y + ph + 4), '%02d  %s' % (ty, mode), fill=(20, 20, 20), font=title_font)
    sheet.save(os.path.join(folder, 'contact.png'))


def glyph_sheet():
    os.makedirs(OUT, exist_ok=True)
    items = [('BP', 'BP'), ('BP2', 'BP2'), ('COMB', 'COMB'), ('TRASH', 'TRASH'),
             ('LP1', 'LP1'), ('HP1', 'HP1'), ('LP4-', 'LP4-'), ('HP4-', 'HP4-'),
             ('LP2', 'LP2'), ('HP2', 'HP2'), ('NOTCH', 'NOTCH'),
             ('COMB-', 'COMB-'), ('TRASH-', 'TRASH-')]
    cell, pad = 17 * 12, 20
    cols = 4
    rows = (len(items) + cols - 1) // cols
    sheet = Image.new('RGB', (cols * (cell + pad) + pad, rows * (cell + 30) + pad + 24),
                      (238, 238, 238))
    d = ImageDraw.Draw(sheet)
    try:
        font = ImageFont.truetype('C:/Windows/Fonts/arial.ttf', 16)
    except OSError:
        font = ImageFont.load_default()
    d.text((pad, 4), 'Filter+ TYPE glyphs', fill=(20, 20, 20), font=font)
    for i, (name, mode) in enumerate(items):
        im = Image.new('RGB', (17, 17), (12, 12, 14))
        gd = ImageDraw.Draw(im)
        draw_glyph(gd, mode, 0, 16)
        x = pad + (i % cols) * (cell + pad)
        y = 24 + pad + (i // cols) * (cell + 30)
        sheet.paste(im.resize((cell, cell), Image.NEAREST), (x, y))
        d.text((x, y + cell + 4), name, fill=(20, 20, 20), font=font)
    sheet.save(os.path.join(OUT, 'glyphs.png'))


def main():
    os.makedirs(OUT, exist_ok=True)
    save_set('DT', DT_MODES, os.path.join(OUT, 'dt'))
    save_set('DN', DN_MODES, os.path.join(OUT, 'dn'))
    glyph_sheet()
    print('wrote', OUT)


if __name__ == '__main__':
    main()
