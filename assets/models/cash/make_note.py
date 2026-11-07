"""Draws the tolar banknote texture atlas (1024 x 1024).

Rows: front 0-478, back 478-956, paper edges 956-990, rubber band 990-1024. The brick's paper band has its own texture (make_band.py).
Run: python3 -I make_note.py [value] [output.png]   value is 20, 50 or 100 (default 100)
The back's island comes from everon_mask.png, traced from the in-game map of Everon.
"""
import math
import os
import sys

from PIL import Image, ImageChops, ImageDraw, ImageFilter, ImageFont

W, H = 1024, 1024
NOTE_H = 478
FONTS = "/usr/share/fonts/noto/"

PAPER = (233, 228, 210)
RED = (150, 40, 36)
RUBBER = (128, 54, 42)

#: Per note: (ink, light ink, accent). Same design, one colour each, as on euro notes.
PALETTES = {
    "20": ((34, 62, 118), (128, 150, 196), (176, 128, 52)),
    "50": ((138, 74, 22), (206, 160, 104), (110, 58, 34)),
    "100": ((31, 92, 96), (120, 166, 160), (176, 128, 52)),
}
#: Per note: what differs besides colour, as on a real series. Layout, frame and security features stay shared.
#: rosette: (loops, waves, amp) for the outer and inner guilloche; pattern: background; motif: back.
DESIGNS = {
    "20": {"rosette": ((26, 12, 0.12), (10, 5, 0.30)), "pattern": "circles", "motif": "sea", "serial": "EA 2158406"},
    "50": {"rosette": ((18, 7, 0.22), (16, 11, 0.15)), "pattern": "diagonal", "motif": "compass", "serial": "EB 5093172"},
    "100": {"rosette": ((22, 9, 0.18), (12, 6, 0.25)), "pattern": "waves", "motif": "island", "serial": "EV 0473921"},
}
VALUE = "100"
INK, INK_LIGHT, ACCENT = PALETTES[VALUE]
DESIGN = DESIGNS[VALUE]


def serif_b(size):
    return ImageFont.truetype(FONTS + "NotoSerif-Bold.ttf", size)


def serif(size):
    return ImageFont.truetype(FONTS + "NotoSerif-Regular.ttf", size)


def sans_b(size):
    return ImageFont.truetype(FONTS + "NotoSans-Bold.ttf", size)


def guilloche(d, cx, cy, r, colour, loops=18, waves=7, amp=0.18, width=1):
    for k in range(loops):
        phase = k * math.pi / loops
        pts = []
        for i in range(721):
            t = i / 720 * 2 * math.pi
            rr = r * (1 - amp + amp * math.sin(waves * t + phase))
            pts.append((cx + rr * math.cos(t), cy + rr * math.sin(t)))
        d.line(pts, fill=colour, width=width)


def wave_band(d, x0, y0, x1, y1, colour, lines=10):
    for k in range(lines):
        y = y0 + (y1 - y0) * k / max(lines - 1, 1)
        pts = [(x, y + 6 * math.sin((x - x0) / 23 + k * 0.7)) for x in range(x0, x1 + 1, 3)]
        d.line(pts, fill=colour, width=1)


def note_base(img, top):
    d = ImageDraw.Draw(img)
    for y in range(NOTE_H):
        t = y / NOTE_H
        c = tuple(int(PAPER[i] * (1 - 0.06 * t) + INK_LIGHT[i] * 0.06 * t) for i in range(3))
        d.line([(0, top + y), (W, top + y)], fill=c)
    background_pattern(img, top)
    d = ImageDraw.Draw(img)
    d.rectangle([14, top + 14, W - 15, top + NOTE_H - 15], outline=INK, width=4)
    microtext_frame(img, top)
    return ImageDraw.Draw(img)


def background_pattern(img, top):
    """The note's own fine background: waves, diagonal lines or concentric circles."""
    pattern = DESIGN["pattern"]
    if pattern == "waves":
        wave_band(ImageDraw.Draw(img), 0, top + 20, W, top + NOTE_H - 20, INK_LIGHT, lines=34)
        return

    layer = Image.new("RGBA", (W, H), (0, 0, 0, 0))
    if pattern == "diagonal":
        layer = hatch_layer((*INK_LIGHT, 255), 13, 24)
    else:
        ld = ImageDraw.Draw(layer)
        cx, cy = W * 0.62, top + NOTE_H / 2
        for r in range(14, 700, 13):
            ld.ellipse([cx - r, cy - r, cx + r, cy + r], outline=(*INK_LIGHT, 255), width=1)

    mask = Image.new("L", (W, H), 0)
    ImageDraw.Draw(mask).rectangle([20, top + 20, W - 20, top + NOTE_H - 20], fill=255)
    img.paste(layer.convert("RGB"), (0, 0), ImageChops.multiply(mask, layer.getchannel("A")))


def microtext_frame(img, top):
    """BANQUE D'EVERON in tiny letters where a plain inner line would be."""
    text = "BANQUE D'EVERON · "
    font = sans_b(9)
    inset = 20

    def run(length):
        strip = Image.new("RGBA", (length, 12), (0, 0, 0, 0))
        sd = ImageDraw.Draw(strip)
        step = sd.textlength(text, font=font)
        x = 0
        while x < length:
            sd.text((x, 0), text, font=font, fill=(*ACCENT, 255))
            x += step
        return strip

    horizontal = run(W - 2 * inset)
    img.paste(horizontal, (inset, top + inset - 1), horizontal)
    img.paste(horizontal, (inset, top + NOTE_H - inset - 11), horizontal)
    vertical = run(NOTE_H - 2 * inset - 24).rotate(90, expand=True)
    img.paste(vertical, (inset - 1, top + inset + 12), vertical)
    img.paste(vertical, (W - inset - 11, top + inset + 12), vertical)


def centred(d, xy, text, font, fill):
    l, t, r, b = d.textbbox((0, 0), text, font=font)
    d.text((xy[0] - (r - l) / 2 - l, xy[1] - (b - t) / 2 - t), text, font=font, fill=fill)


def hatch_layer(colour, spacing, angle_deg, width=1):
    layer = Image.new("RGBA", (W, H), (0, 0, 0, 0))
    ld = ImageDraw.Draw(layer)
    a = math.radians(angle_deg)
    dx, dy = math.cos(a), math.sin(a)
    for k in range(-W * 2, W * 2, spacing):
        cx, cy = W / 2 - dy * k, H / 2 + dx * k
        ld.line([(cx - dx * 2000, cy - dy * 2000), (cx + dx * 2000, cy + dy * 2000)], fill=colour, width=width)
    return layer


def hatch_into(img, polygon, colour, spacing, angle_deg):
    mask = Image.new("L", (W, H), 0)
    ImageDraw.Draw(mask).polygon(polygon, fill=255)
    layer = hatch_layer((*colour, 255), spacing, angle_deg)
    img.paste(layer.convert("RGB"), (0, 0), ImageChops.multiply(mask, layer.getchannel("A")))


def draw_front(img):
    top = 0
    d = note_base(img, top)
    d.ellipse([60, top + 70, 400, top + 410], fill=PAPER)
    (outer_loops, outer_waves, outer_amp), (inner_loops, inner_waves, inner_amp) = DESIGN["rosette"]
    guilloche(d, 230, top + 240, 165, INK, loops=outer_loops, waves=outer_waves, amp=outer_amp)
    guilloche(d, 230, top + 240, 110, ACCENT, loops=inner_loops, waves=inner_waves, amp=inner_amp)
    centred(d, (230, top + 240), "Ŧ", serif_b(150), INK)
    watermark_window(img, 905, top + 245)
    d = ImageDraw.Draw(img)
    centred(d, (615, top + 92), "BANQUE D'EVERON", serif_b(46), INK)
    centred(d, (615, top + 145), "BILLET DE BANQUE", serif(24), INK)
    centred(d, (615, top + 262), VALUE, serif_b(176), INK)
    centred(d, (615, top + 372), "TOLARS", serif_b(40), ACCENT)
    signature(d, 485, top + 418)
    d.text((640, top + NOTE_H - 70), DESIGN["serial"], font=sans_b(26), fill=RED)
    d.text((50, top + 40), "Ŧ " + VALUE, font=serif_b(30), fill=INK)
    d.text((50, top + NOTE_H - 72), "Ŧ " + VALUE, font=serif_b(30), fill=INK)


def watermark_window(img, cx, cy):
    """A calm, lighter oval with a faint rosette, like the portrait window on euro notes."""
    rx, ry = 70, 150
    mask = Image.new("L", (W, H), 0)
    ImageDraw.Draw(mask).ellipse([cx - rx, cy - ry, cx + rx, cy + ry], fill=255)
    mask = mask.filter(ImageFilter.GaussianBlur(10))
    img.paste(Image.new("RGB", (W, H), (242, 239, 226)), (0, 0), mask)
    layer = Image.new("RGBA", (W, H), (0, 0, 0, 0))
    guilloche(ImageDraw.Draw(layer), cx, cy, 58, (*INK_LIGHT, 70), loops=16, waves=9)
    img.paste(layer, (0, 0), layer)


def signature(d, x, y):
    """A short, handwritten-looking line above the governor's title."""
    pts = []
    for i in range(141):
        t = i / 140
        pts.append((x - 70 + 140 * t, y - 16 + 9 * math.sin(t * 13) * (1 - t) - 6 * t + 4 * math.sin(t * 31)))
    d.line(pts, fill=INK, width=2)
    d.line([(x - 40, y - 4), (x + 60, y - 12)], fill=INK, width=1)
    centred(d, (x, y + 10), "Le Gouverneur", serif(15), INK)


def draw_back(img):
    """Everything in the front's line style: no flat fills."""
    top = NOTE_H
    d = note_base(img, top)

    # Security thread: two hairlines with micro text between them.
    tx = 150
    d.line([(tx - 7, top + 30), (tx - 7, top + NOTE_H - 30)], fill=INK, width=1)
    d.line([(tx + 7, top + 30), (tx + 7, top + NOTE_H - 30)], fill=INK, width=1)
    strip = Image.new("RGBA", (NOTE_H - 60, 14), (0, 0, 0, 0))
    sd = ImageDraw.Draw(strip)
    x = 0
    while x < strip.width:
        sd.text((x, -1), "ŦEVERON", font=sans_b(11), fill=(*INK, 255))
        x += 62
    strip = strip.rotate(90, expand=True)
    img.paste(strip, (tx - 7, top + 30), strip)

    motif = DESIGN["motif"]
    if motif == "island":
        draw_island(img, top)
    elif motif == "compass":
        draw_compass_ring(img, top)
    else:
        draw_sea(img, top)
    d = ImageDraw.Draw(img)

    # The value large and alone, bottom right; the bank's name small at the top.
    centred(d, (868, top + NOTE_H - 90), VALUE, serif_b(120), INK)
    centred(d, (790, top + 62), "BANQUE D'EVERON", serif_b(30), INK)
    d.text((176, top + NOTE_H - 70), "Ŧ " + VALUE, font=serif_b(30), fill=INK)


def island_mask(size, ox, oy):
    """Everon, traced from the in-game map, placed on a full-size canvas."""
    here = os.path.dirname(os.path.abspath(__file__))
    land = Image.open(os.path.join(here, "everon_mask.png")).convert("L")
    land = land.resize((size, size), Image.LANCZOS).point(lambda v: 255 if v > 127 else 0)
    full = Image.new("L", (W, H), 0)
    full.paste(land, (ox, oy))
    return full


def depth_contours(img, land, steps, colour):
    for grow in steps:
        ring = land.filter(ImageFilter.MaxFilter(grow)).filter(ImageFilter.FIND_EDGES)
        img.paste(Image.new("RGB", (W, H), colour), (0, 0), ring)


def coastline(img, land):
    coast = land.filter(ImageFilter.FIND_EDGES).filter(ImageFilter.MaxFilter(3))
    img.paste(Image.new("RGB", (W, H), INK), (0, 0), coast)


def fill_hatched(img, land, spacing, angle):
    layer = hatch_layer((*INK_LIGHT, 255), spacing, angle)
    img.paste(layer.convert("RGB"), (0, 0), ImageChops.multiply(land, layer.getchannel("A")))


def compass_rose(d, cx, cy, radius, outer, inner):
    guilloche(d, cx, cy, outer, INK_LIGHT, loops=14, waves=8)
    guilloche(d, cx, cy, inner, ACCENT, loops=8, waves=4, amp=0.3)
    for k in range(8):
        a = k * math.pi / 4
        length = radius if k % 2 == 0 else radius * 0.6
        d.line([(cx, cy), (cx + length * math.sin(a), cy - length * math.cos(a))], fill=INK, width=2 if k % 2 == 0 else 1)


def draw_island(img, top):
    """Ŧ100: the whole island as a hatched chart, with a compass rose at sea."""
    land = island_mask(380, 240, top + 52)
    depth_contours(img, land, (13, 7), INK_LIGHT)
    fill_hatched(img, land, 6, 30)
    coastline(img, land)
    d = ImageDraw.Draw(img)
    cx, cy = 790, top + 205
    compass_rose(d, cx, cy, 80, 62, 38)
    centred(d, (cx, cy - 98), "N", serif_b(22), INK)


def draw_compass_ring(img, top):
    """Ŧ50: a large compass rose inside a bezel with degree marks, like a ship's compass."""
    d = ImageDraw.Draw(img)
    cx, cy = 500, top + 236
    for r in (150, 172):
        d.ellipse([cx - r, cy - r, cx + r, cy + r], outline=INK, width=2)
    for deg in range(0, 360, 5):
        a = math.radians(deg)
        inner = 160 if deg % 30 else 152
        d.line([(cx + inner * math.sin(a), cy - inner * math.cos(a)), (cx + 172 * math.sin(a), cy - 172 * math.cos(a))], fill=INK, width=1)
    guilloche(d, cx, cy, 140, INK_LIGHT, loops=20, waves=10, amp=0.12)
    compass_rose(d, cx, cy, 128, 100, 60)
    for letter, deg in (("N", 0), ("E", 90), ("S", 180), ("O", 270)):
        a = math.radians(deg)
        centred(d, (cx + 190 * math.sin(a), cy - 190 * math.cos(a)), letter, serif_b(24), INK)


def draw_sea(img, top):
    """Ŧ20: sunrise over the sea, engraved: rays in the sky, the sun on the horizon, scalloped waves below."""
    x0, y0, x1, y1 = 180, top + 96, 760, top + 392
    horizon = y0 + int((y1 - y0) * 0.5)
    sun_x = (x0 + x1) // 2
    layer = Image.new("RGB", (W, H), PAPER)
    ld = ImageDraw.Draw(layer)

    # Sky: fine rays fanning out from the sun.
    for deg in range(-84, 85, 4):
        a = math.radians(deg)
        ld.line([(sun_x, horizon), (sun_x + 700 * math.sin(a), horizon - 700 * math.cos(a))], fill=INK_LIGHT, width=1)

    # The sun: half a disc of concentric arcs, cut by the horizon.
    for k, rr in enumerate(range(96, 20, -7)):
        ld.ellipse([sun_x - rr, horizon - rr, sun_x + rr, horizon + rr], fill=PAPER, outline=ACCENT if k % 2 else INK, width=2 if k == 0 else 1)

    # Sea: scalloped waves, each row overlapping the one above, so only the upper arcs show.
    # Drawn apart and pasted below the horizon only, so no wave crest pokes into the sky.
    sea = Image.new("RGB", (W, H), PAPER)
    sd = ImageDraw.Draw(sea)
    r = 26
    row = 0
    for cy in range(horizon, y1 + r, r // 2):
        offset = (row % 2) * r
        for cx in range(x0 - 2 * r + offset, x1 + 2 * r, 2 * r):
            sd.ellipse([cx - r, cy - r, cx + r, cy + r], fill=PAPER)
            for k, rr in enumerate(range(r, 4, -6)):
                sd.ellipse([cx - rr, cy - rr, cx + rr, cy + rr], outline=INK if k == 0 else INK_LIGHT, width=1)
        row += 1
    below = Image.new("L", (W, H), 0)
    ImageDraw.Draw(below).rectangle([0, horizon, W, H], fill=255)
    layer.paste(sea, (0, 0), below)
    ld.line([(x0, horizon), (x1, horizon)], fill=INK, width=2)

    mask = Image.new("L", (W, H), 0)
    ImageDraw.Draw(mask).rounded_rectangle([x0, y0, x1, y1], radius=24, fill=255)
    img.paste(layer, (0, 0), mask)
    ImageDraw.Draw(img).rounded_rectangle([x0, y0, x1, y1], radius=24, outline=INK, width=2)


def see_through_register(img, x, y, size=46):
    """Left half of the Ŧ on the front; the right half, mirrored, at the matching spot on the back."""
    glyph = Image.new("L", (size, size), 0)
    gd = ImageDraw.Draw(glyph)
    font = serif_b(int(size * 0.95))
    l, t, r, b = gd.textbbox((0, 0), "Ŧ", font=font)
    gd.text(((size - (r - l)) / 2 - l, (size - (b - t)) / 2 - t), "Ŧ", font=font, fill=255)
    half = size // 2
    front = Image.new("L", (size, size), 0)
    front.paste(glyph.crop((0, 0, half, size)), (0, 0))
    back = Image.new("L", (size, size), 0)
    back.paste(glyph.crop((half, 0, size, size)), (half, 0))
    back = back.transpose(Image.FLIP_LEFT_RIGHT)
    ink = Image.new("RGB", (size, size), INK)
    img.paste(ink, (x, y), front)
    img.paste(ink, (W - x - size, NOTE_H + y), back)
    d = ImageDraw.Draw(img)
    d.ellipse([x - 4, y - 4, x + size + 4, y + size + 4], outline=INK_LIGHT, width=1)
    d.ellipse([W - x - size - 4, NOTE_H + y - 4, W - x + 4, NOTE_H + y + size + 4], outline=INK_LIGHT, width=1)


def draw_edges_and_band(img):
    d = ImageDraw.Draw(img)
    top = 2 * NOTE_H
    d.rectangle([0, top, W, top + 34], fill=PAPER)
    for y in range(top, top + 34, 2):
        shade = 210 + (y % 6) * 4
        d.line([(0, y), (W, y)], fill=(shade, shade - 4, shade - 20))

    # Rubber band for the medium stack: red-brown with a lighter sheen along the middle, as on stretched rubber.
    top = 2 * NOTE_H + 34
    for y in range(top, H):
        t = (y - top) / (H - top - 1)
        sheen = 1 - abs(t - 0.45) * 2
        d.line([(0, y), (W, y)], fill=tuple(int(c + 34 * max(sheen, 0) ** 2) for c in RUBBER))


def main():
    global VALUE, INK, INK_LIGHT, ACCENT, DESIGN
    VALUE = sys.argv[1] if len(sys.argv) > 1 else "100"
    INK, INK_LIGHT, ACCENT = PALETTES[VALUE]
    DESIGN = DESIGNS[VALUE]
    default_out = "TolarNote_Atlas.png" if VALUE == "100" else "TolarNote_Atlas_%s.png" % VALUE
    out = sys.argv[2] if len(sys.argv) > 2 else default_out
    img = Image.new("RGB", (W, H), PAPER)
    draw_front(img)
    draw_back(img)
    see_through_register(img, 880, 36)
    draw_edges_and_band(img)
    img.save(out)
    print("saved", out)


if __name__ == "__main__":
    main()
