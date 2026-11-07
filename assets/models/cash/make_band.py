"""Draws the paper band of the Ŧ10,000 brick (1024 x 256), modelled on real currency straps.

The band wraps the brick's short side, so u runs once around it: top face 0-448, side 448-512, bottom 512-960,
side 960-1024 (a 7 cm top, 1 cm side, 7 cm bottom, 1 cm side). v runs across the band's 4 cm width.
Run: python3 -I make_band.py [output.png]
"""
import math
import sys

from PIL import Image, ImageDraw, ImageFont

W, H = 1024, 256
FONTS = "/usr/share/fonts/noto/"

# Colour-coded kraft, as real straps are by denomination; printed in the Ŧ100 note's teal.
KRAFT = (201, 164, 84)
KRAFT_DARK = (176, 140, 66)
INK = (31, 92, 96)
PEN = (32, 46, 120)
STAMP = (112, 58, 128)

TOP = (0, 448)
SIDE_A = (448, 512)
BOTTOM = (512, 960)
SIDE_B = (960, 1024)


def font(name, size):
    return ImageFont.truetype(FONTS + name, size)


def centred(d, xy, text, f, fill):
    l, t, r, b = d.textbbox((0, 0), text, font=f)
    d.text((xy[0] - (r - l) / 2 - l, xy[1] - (b - t) / 2 - t), text, font=f, fill=fill)


def paper(img):
    """Kraft with faint fibres, so it doesn't read as a flat colour."""
    d = ImageDraw.Draw(img)
    d.rectangle([0, 0, W, H], fill=KRAFT)
    seed = 7
    for _ in range(900):
        seed = (seed * 1103515245 + 12345) & 0x7FFFFFFF
        x = seed % W
        seed = (seed * 1103515245 + 12345) & 0x7FFFFFFF
        y = seed % H
        length = 6 + seed % 14
        d.line([(x, y), (x + length, y + (seed % 3) - 1)], fill=KRAFT_DARK, width=1)


def rules(d, x0, x1):
    """Double rules along both edges of the band, as on printed straps."""
    for y in (10, 16, H - 17, H - 11):
        d.line([(x0, y), (x1, y)], fill=INK, width=2 if y in (10, H - 11) else 1)


def draw_top(d):
    x0, x1 = TOP
    cx = (x0 + x1) / 2
    rules(d, x0, x1)
    centred(d, (cx, 44), "BANQUE D'EVERON", font("NotoSerif-Bold.ttf", 24), INK)
    centred(d, (cx, 120), "Ŧ 10 000", font("NotoSerif-Bold.ttf", 82), INK)
    centred(d, (cx, 196), "100 × Ŧ 100", font("NotoSans-Bold.ttf", 28), INK)


def handwriting(d, x, y, width, wobble, seed):
    """A short pen stroke standing in for a cashier's initials or a date."""
    pts = []
    for i in range(61):
        t = i / 60
        pts.append((x + width * t, y + wobble * math.sin(t * (9 + seed)) + 3 * math.sin(t * 27 + seed)))
    d.line(pts, fill=PEN, width=2)


def stamp(img, cx, cy, r):
    """A round cashier's stamp in violet ink, slightly tilted and uneven like a real impression."""
    layer = Image.new("RGBA", (2 * r + 8, 2 * r + 8), (0, 0, 0, 0))
    ld = ImageDraw.Draw(layer)
    c = r + 4
    ld.ellipse([c - r, c - r, c + r, c + r], outline=(*STAMP, 200), width=3)
    ld.ellipse([c - r + 9, c - r + 9, c + r - 9, c + r - 9], outline=(*STAMP, 200), width=1)
    text = "BANQUE D'EVERON · CAISSE · "
    f = font("NotoSans-Bold.ttf", 11)
    for i, ch in enumerate(text):
        a = 2 * math.pi * i / len(text)
        glyph = Image.new("RGBA", (16, 16), (0, 0, 0, 0))
        ImageDraw.Draw(glyph).text((3, 1), ch, font=f, fill=(*STAMP, 210))
        glyph = glyph.rotate(-math.degrees(a), resample=Image.BICUBIC)
        layer.paste(glyph, (int(c + (r - 6) * math.sin(a) - 8), int(c - (r - 6) * math.cos(a) - 8)), glyph)
    centred(ld, (c, c), "Ŧ", font("NotoSerif-Bold.ttf", 30), (*STAMP, 210))
    layer = layer.rotate(-12, resample=Image.BICUBIC)
    img.paste(layer, (int(cx - layer.width / 2), int(cy - layer.height / 2)), layer)


def draw_bottom(img, d):
    x0, x1 = BOTTOM
    rules(d, x0, x1)
    f = font("NotoSans-Regular.ttf", 20)
    d.text((x0 + 24, 40), "Date", font=f, fill=INK)
    d.line([(x0 + 80, 64), (x0 + 230, 64)], fill=INK, width=1)
    handwriting(d, x0 + 88, 52, 120, 4, 1)
    d.text((x0 + 24, 96), "Caissier", font=f, fill=INK)
    d.line([(x0 + 110, 120), (x0 + 230, 120)], fill=INK, width=1)
    handwriting(d, x0 + 118, 106, 70, 7, 3)
    d.text((x0 + 24, 152), "Vérifié", font=f, fill=INK)
    d.line([(x0 + 100, 176), (x0 + 230, 176)], fill=INK, width=1)
    handwriting(d, x0 + 108, 162, 60, 6, 5)
    centred(d, (x0 + 224, 214), "Sans garantie · à compter à la réception", font("NotoSans-Regular.ttf", 15), INK)
    stamp(img, x0 + 340, 108, 62)


def draw_sides(d):
    for x0, x1 in (SIDE_A, SIDE_B):
        rules(d, x0, x1)
        centred(d, ((x0 + x1) / 2, H / 2), "Ŧ", font("NotoSerif-Bold.ttf", 34), INK)


def main():
    out = sys.argv[1] if len(sys.argv) > 1 else "TolarBand.png"
    img = Image.new("RGB", (W, H), KRAFT)
    paper(img)
    d = ImageDraw.Draw(img)
    draw_top(d)
    draw_sides(d)
    draw_bottom(img, d)
    img.save(out)
    print("saved", out)


if __name__ == "__main__":
    main()
