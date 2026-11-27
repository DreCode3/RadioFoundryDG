# SPDX-License-Identifier: MIT
# Copyright (c) 2026 The RadioFoundry Authors
# /// script
# dependencies = ["pillow==13.0.0"]
# ///
"""Generate the panels theme's icon family (core/ui/themes/panels/panels_icons.h).

One family, drawn here from first principles: a 24 px canvas with a 20 px live area (2..22), 2 px strokes with round
ends and joins, filled shapes only where a stroke can't read at this size. Each icon becomes a 2-bit coverage mask
trimmed to its ink, so the theme draws every icon, in the strip and in the menus, with one blitter (panels_icon).
The scan zigzag also gets the step of its animation for each pixel (a small gap moves along the stroke, then the
arrowhead); roaming is a ring with a dot that steps round it.

A second set draws the same pictures at 0.8x for a display under 200 px high (the compact layout): a 20 px canvas with
a 16 px live area (2..18), every coordinate, radius and stroke of the 24 px drawing scaled about the live area's corner.

    python3 tools/profile_icons.py [OUT]
"""

import math
import sys
from pathlib import Path

from PIL import Image, ImageDraw

OUT = Path(__file__).resolve().parents[1] / "core/ui/themes/panels/panels_icons.h"
S = 8		# supersampling
N = 24		# canvas
SW = 2.0	# stroke
# The sets: (name suffix, canvas, scale, offset). The drawings are in the 24 px canvas's coordinates; a set maps x to
# x * scale + offset, so the live area's corner (2, 2) stays where it is
SETS = [("", 24, 1.0, 0.0), ("_s", 20, 0.8, 0.4)]


class Canvas:
    def __init__(self, n=N, k=1.0, o=0.0):
        self.n, self.k, self.o = n, k, o
        self.im = Image.new("L", (n * S, n * S), 0)
        self.d = ImageDraw.Draw(self.im)

    def _x(self, v):
        return (v * self.k + self.o) * S

    def _p(self, pts):
        return [(self._x(x), self._x(y)) for x, y in pts]

    def disk(self, x, y, r, ink=255):
        r *= self.k
        self.d.ellipse((self._x(x) - r * S, self._x(y) - r * S, self._x(x) + r * S, self._x(y) + r * S), fill=ink)

    def line(self, pts, w=SW, ink=255):
        for (x0, y0), (x1, y1) in zip(pts, pts[1:]):
            length = math.hypot(x1 - x0, y1 - y0) or 1
            dx, dy = (y1 - y0) * w / (2 * length), (x0 - x1) * w / (2 * length)
            self.d.polygon(self._p([(x0 + dx, y0 + dy), (x1 + dx, y1 + dy), (x1 - dx, y1 - dy), (x0 - dx, y0 - dy)]),
                           fill=ink)
        for x, y in pts:
            self.disk(x, y, w / 2, ink)

    def ring(self, x, y, r, w=SW):
        self.disk(x, y, r + w / 2)
        self.disk(x, y, r - w / 2, 0)

    def arc(self, x, y, r, a0, a1, w=SW, ink=255):
        """Degrees, 0 = right, clockwise (y down)."""
        pts = [(x + r * math.cos(math.radians(a)), y + r * math.sin(math.radians(a)))
               for a in [a0 + (a1 - a0) * i / 48 for i in range(49)]]
        self.line(pts, w, ink)

    def poly(self, pts, ink=255):
        self.d.polygon(self._p(pts), fill=ink)

    def rrect(self, x0, y0, x1, y1, r, ink=255):
        self.d.rounded_rectangle((self._x(x0), self._x(y0), self._x(x1), self._x(y1)), r * self.k * S, fill=ink)

    def rrect_stroke(self, x0, y0, x1, y1, r, w=SW):
        self.rrect(x0 - w / 2, y0 - w / 2, x1 + w / 2, y1 + w / 2, r + w / 2)
        self.rrect(x0 + w / 2, y0 + w / 2, x1 - w / 2, y1 - w / 2, max(0, r - w / 2), 0)

    def coverage(self):
        n = self.n
        small = self.im.resize((n, n), Image.Resampling.BOX)
        return [[min(3, (small.getpixel((x, y)) * 3 + 127) // 255) for x in range(n)] for y in range(n)]


# --- The family ---

def contacts(c):
    c.ring(12, 8, 3.6)
    c.arc(12, 21.5, 7.5, 200, 340)
    c.line([(4.9, 18.9), (4.6, 20)])
    c.line([(19.1, 18.9), (19.4, 20)])


def envelope(c):
    c.rrect_stroke(3.5, 6, 20.5, 18, 2)
    c.line([(4.5, 7.5), (12, 13), (19.5, 7.5)])


def clock(c):
    c.ring(12, 12, 8.5)
    c.line([(12, 7.5), (12, 12), (15.5, 14)])


def folder(c):
    c.line([(3.5, 8), (3.5, 5), (9.3, 5), (11.3, 7.5)])
    c.rrect_stroke(3.5, 7.5, 20.5, 19, 2)


# The scan zigzag: a top stroke, a diagonal back, a bottom stroke ending in a filled arrowhead
SCAN_PATH = [(3.5, 4.5), (17.5, 4.5), (5, 16), (15.5, 16)]
SCAN_HEAD = [(21.5, 16), (15, 10.2), (15, 21.8)]
SCAN_STROKE_STEPS = 12	# the gap's steps along the stroke; one more lights the arrowhead


def scan(c):
    c.line(SCAN_PATH)
    c.poly(SCAN_HEAD)


def pin(c):
    c.poly([(12, 21.5), (6.2, 13.5), (17.8, 13.5)])
    c.disk(12, 9.5, 7)
    c.disk(12, 9.5, 2.6, 0)


def arrows(c):
    c.line([(4, 8), (19.5, 8)])
    c.line([(15.5, 4.2), (19.5, 8), (15.5, 11.8)])
    c.line([(20, 16), (4.5, 16)])
    c.line([(8.5, 12.2), (4.5, 16), (8.5, 19.8)])


def gear(c):
    for i in range(8):
        a = math.radians(i * 45)
        ca, sa = math.cos(a), math.sin(a)
        hw, r0, r1 = 1.9, 5, 9.6
        c.poly([(12 + r0 * ca - hw * sa, 12 + r0 * sa + hw * ca), (12 + r1 * ca - hw * sa, 12 + r1 * sa + hw * ca),
                (12 + r1 * ca + hw * sa, 12 + r1 * sa - hw * ca), (12 + r0 * ca + hw * sa, 12 + r0 * sa - hw * ca)])
    c.disk(12, 12, 7.6)
    c.disk(12, 12, 3.1, 0)


def handset(c, arrow=False):
    c.poly([(4.3, 4.6), (8.6, 3.6), (10.6, 8.6), (8.2, 10.6), (13.4, 15.8), (15.4, 13.4), (20.4, 15.4),
            (19.4, 19.7), (17.4, 21), (14, 20.6), (8.6, 17.2), (6.8, 15.4), (3.4, 9.9), (3, 6.6)])
    if arrow:
        c.line([(20.5, 3.5), (14.5, 9.5)])
        c.line([(14.5, 5), (14.5, 9.5), (19, 9.5)])


def caller(c):
    handset(c, True)


def display(c):
    c.rrect_stroke(3.5, 4.5, 20.5, 15.5, 2)
    c.line([(12, 16.5), (12, 19.5)])
    c.line([(7.5, 20), (16.5, 20)])


def bell(c, dy=-0.4):	# 0.4 px up: the clapper ends inside the live area (row 21)
    c.poly([(6.5, 16.5 + dy), (6.5, 10.5 + dy), (17.5, 10.5 + dy), (17.5, 16.5 + dy)])
    c.disk(12, 10.5 + dy, 5.5)
    c.rrect(4, 15.5 + dy, 20, 18 + dy, 1.2)
    c.disk(12, 20.3 + dy, 1.9)
    c.disk(12, 3.8 + dy, 1.4)


def headphones(c):
    c.arc(12, 13, 8, 180, 360)
    c.line([(4, 13), (4, 15)])
    c.line([(20, 13), (20, 15)])
    c.rrect(3, 13.5, 8.2, 21, 1.6)
    c.rrect(15.8, 13.5, 21, 21, 1.6)


def dial(c):
    """A tuning dial: a round knob with a pointer, tick marks over its top."""
    cx, cy = 12, 14
    for a in (-150, -120, -90, -60, -30):
        r = math.radians(a)
        c.line([(cx + 8.7 * math.cos(r), cy + 8.7 * math.sin(r)), (cx + 10.6 * math.cos(r), cy + 10.6 * math.sin(r))],
               1.8)
    c.ring(cx, cy, 5.8)
    r = math.radians(-60)
    c.line([(cx, cy), (cx + 3.9 * math.cos(r), cy + 3.9 * math.sin(r))])


def key(c):
    c.ring(7.4, 12, 3.6)
    c.line([(11, 12), (20.5, 12)])
    c.line([(17.5, 12), (17.5, 15.5)])
    c.line([(20.5, 12), (20.5, 14.5)])


def padlock(c):
    c.arc(12, 9, 4.6, 180, 360)
    c.line([(7.4, 9), (7.4, 11)])
    c.line([(16.6, 9), (16.6, 11)])
    c.rrect(4.5, 10.5, 19.5, 21, 2)
    c.disk(12, 15, 1.8, 0)
    c.rrect(11.2, 15, 12.8, 18.2, 0.6, 0)


def keypad(c):
    for y in (5, 11, 17):
        for x in (5.5, 12, 18.5):
            c.rrect(x - 2.2, y - 2, x + 2.2, y + 2, 1)


def sliders(c):
    for y, k in ((5.5, 15), (12, 8), (18.5, 16.5)):
        c.line([(3.5, y), (20.5, y)])
        c.disk(k, y, 3.4, 0)
        c.ring(k, y, 2.2, 1.8)


def info(c):
    c.ring(12, 12, 8.5)
    c.disk(12, 7.7, 1.4)
    c.line([(12, 11), (12, 16.5)])


def speaker(c):
    """The strip's monitor: a speaker with one wave."""
    c.poly([(3, 9), (7, 9), (12.5, 4.5), (12.5, 19.5), (7, 15), (3, 15)])
    c.arc(12.5, 12, 5, -50, 50)


ROAM_R = 7.5	# the roaming ring's radius
ROAM_DOT_R = 7.2	# its dot's 8 places, rounded to whole pixels on a circle a little inside it


def ring(c):
    c.ring(12, 12, ROAM_R)
    c.disk(12, 12, 1.6)


def dot(c):
    c.disk(12, 12, 2.7)


def halo(c):
    c.disk(12, 12, 4.2)


# (enum name, drawing): the Menu and Settings icons by ui_menu_icon_t (core/ui/ui_screens.h), then the strip's own
ICONS = [
    ("UI_MENU_ICON_AUDIO", headphones), ("UI_MENU_ICON_DISPLAY", display), ("UI_MENU_ICON_CALL", handset),
    ("UI_MENU_ICON_PRIVACY", key), ("UI_MENU_ICON_LOCATION", pin), ("UI_MENU_ICON_CONNECTIONS", arrows),
    ("UI_MENU_ICON_KEYS", keypad), ("UI_MENU_ICON_LOCK", padlock), ("UI_MENU_ICON_RADIO", info),
    ("UI_MENU_ICON_TONES", bell), ("UI_MENU_ICON_ADVANCED", sliders), ("UI_MENU_ICON_CALLER", caller),
    ("UI_MENU_ICON_CONTACTS", contacts), ("UI_MENU_ICON_MESSAGES", envelope), ("UI_MENU_ICON_RECENT", clock),
    ("UI_MENU_ICON_ZONES", folder), ("UI_MENU_ICON_RECEIVE", dial), ("UI_MENU_ICON_SCAN", scan),
    ("UI_MENU_ICON_SETTINGS", gear),
    ("PANELS_ICON_MONITOR", speaker), ("PANELS_ICON_RING", ring), ("PANELS_ICON_DOT", dot),
    ("PANELS_ICON_HALO", halo),
]


def trim(cov):
    n = len(cov)
    xs = [x for y in range(n) for x in range(n) if cov[y][x]]
    ys = [y for y in range(n) for x in range(n) if cov[y][x]]
    return min(xs), min(ys), max(xs) - min(xs) + 1, max(ys) - min(ys) + 1


def packed(cov, x0, y0, w, h):
    out = []
    for y in range(y0, y0 + h):
        row = cov[y][x0:x0 + w] + [0] * (-w % 4)
        out.append([sum(row[i + j] << (2 * j) for j in range(4)) for i in range(0, len(row), 4)])
    return out


def scan_steps(x0, y0, w, h, cov, n=N, k=1.0, o=0.0):
    """Each inked pixel's step: 0..SCAN_STROKE_STEPS-1 along the stroke, SCAN_STROKE_STEPS on the arrowhead."""
    head, stroke = Canvas(n, k, o), Canvas(n, k, o)
    head.poly(SCAN_HEAD)
    stroke.line(SCAN_PATH)
    head_cov, stroke_cov = head.coverage(), stroke.coverage()
    path = [(x * k + o, y * k + o) for x, y in SCAN_PATH]
    head_x = SCAN_HEAD[1][0] * k + o
    lens = [math.dist(a, b) for a, b in zip(path, path[1:])]
    total = sum(lens)

    def along(px, py):
        best, best_t, t0 = None, 0, 0
        for (a, b), ln in zip(zip(path, path[1:]), lens):
            ux, uy = (b[0] - a[0]) / ln, (b[1] - a[1]) / ln
            u = max(0, min(ln, (px - a[0]) * ux + (py - a[1]) * uy))
            d = math.hypot(a[0] + ux * u - px, a[1] + uy * u - py)
            if best is None or d < best:
                best, best_t = d, t0 + u
            t0 += ln
        return best_t

    rows = []
    for y in range(y0, y0 + h):
        row = []
        for x in range(x0, x0 + w):
            if not cov[y][x]:
                row.append(15)
            elif head_cov[y][x] and head_cov[y][x] >= stroke_cov[y][x] and x + 0.5 >= head_x:
                # As much arrowhead as stroke, its faint edge included: it goes out with the arrowhead, no speck left
                row.append(SCAN_STROKE_STEPS)
            else:
                row.append(min(SCAN_STROKE_STEPS - 1, int(along(x + 0.5, y + 0.5) * SCAN_STROKE_STEPS / total)))
        row += [15] * (w % 2)
        rows.append([row[i] | (row[i + 1] << 4) for i in range(0, len(row), 2)])
    return rows


def hexrows(rows):
    return "".join("\t" + ", ".join(f"0x{b:02X}" for b in r) + ",\n" for r in rows)


def icon_set(suffix, n, k, o):
    """One set's tables: the masks, the descriptors and the scan steps, as C text; and the bytes of its masks."""
    blob, table = [], []
    scan_rows = None
    at = 0
    for name, draw in ICONS:
        c = Canvas(n, k, o)
        draw(c)
        cov = c.coverage()
        x0, y0, w, h = trim(cov)
        rows = packed(cov, x0, y0, w, h)
        blob.append(f"\t// {name} ({draw.__name__}), {w} x {h}\n" + hexrows(rows))
        table.append(f"\t[{name}] = {{ {w}, {h}, {x0}, {y0}, {at} }},\n")
        at += sum(len(r) for r in rows)
        if draw is scan:
            scan_rows = scan_steps(x0, y0, w, h, cov, n, k, o)
    return [f"static const uint8_t panels_icon_bits{suffix}[] = {{\n" + "".join(blob) + "};\n",
            f"static const panels_icon_t panels_icons{suffix}[PANELS_ICON_COUNT] = {{\n" + "".join(table) + "};\n",
            scan_rows], at


def roam_dots(k):
    r = ROAM_DOT_R * k
    return [(round(r * math.cos(math.radians(i * 45))), round(r * math.sin(math.radians(i * 45)))) for i in range(8)]


def main():
    out = Path(sys.argv[1]) if len(sys.argv) > 1 else OUT
    lines = ["// SPDX-License-Identifier: MIT\n// Copyright (c) 2026 The RadioFoundry Authors\n"
             "// Generated by tools/profile_icons.py. The panels icon family: a 24 px canvas, each icon's 2-bit coverage\n"
             "// trimmed to its ink (w x h at dx, dy in the canvas), four pixels a byte, low bits first.\n"]
    lines.append("#define PANELS_ICON_CANVAS 24\n")
    lines.append("enum { PANELS_ICON_MONITOR = UI_MENU_ICON_COUNT, PANELS_ICON_RING, PANELS_ICON_DOT, PANELS_ICON_HALO,\n"
                 "\tPANELS_ICON_COUNT };\n")
    lines.append("typedef struct { uint8_t w, h, dx, dy; uint16_t at; } panels_icon_t;\n")
    (bits, table, scan_rows), at = icon_set(*SETS[0])
    lines += [bits, table]
    lines.append(f"// The scan zigzag's pixels by step of its animation, two a byte, low nibble first (15: no ink)\n"
                 f"#define PANELS_SCAN_STEPS {SCAN_STROKE_STEPS + 1}\n"
                 "static const uint8_t panels_scan_steps[] = {\n" + hexrows(scan_rows) + "};\n")
    lines.append("#if CONFIG_DMR_ROAMING\n// The roaming dot's 8 places on its ring, from the ring's center\n"
                 "static const int8_t panels_roam_dot[8][2] = {\n" +
                 "".join(f"\t{{ {x}, {y} }},\n" for x, y in roam_dots(1.0)) + "};\n#endif")
    suffix, n, k, o = SETS[1]
    (bits, table, scan_rows), small_at = icon_set(suffix, n, k, o)
    lines.append(f"// The same pictures at {k}x for the compact layout (a display under 200 px high): a {n} px canvas, its live\n"
                 f"// area 2..{2 + round(20 * k)}, the same tables and order\n"
                 f"#define PANELS_ICON_CANVAS{suffix.upper()} {n}\n" + bits)
    lines.append(table)
    lines.append(f"static const uint8_t panels_scan_steps{suffix}[] = {{\n" + hexrows(scan_rows) + "};\n")
    lines.append(f"#if CONFIG_DMR_ROAMING\nstatic const int8_t panels_roam_dot{suffix}[8][2] = {{\n" +
                 "".join(f"\t{{ {x}, {y} }},\n" for x, y in roam_dots(k)) + "};\n#endif")
    out.write_text("\n".join(lines) + "\n", encoding="ascii")
    print(f"{out}: {len(ICONS)} icons, {at} bytes of masks, {small_at} in the {n} px set")


if __name__ == "__main__":
    main()
