#!/usr/bin/env python3
# Draws res/Symbolic.svg. VCV Rack can't draw SVG <text>, so every label is
# turned into outline paths with fontTools. Positions are in mm and must match
# src/Symbolic.cpp (the numbers in the LAYOUT block are the same in both).
#   pip install fonttools
#   python3 res/make_panel.py
import os
from fontTools.ttLib import TTFont
from fontTools.pens.svgPathPen import SVGPathPen
from fontTools.pens.transformPen import TransformPen

HERE = os.path.dirname(os.path.abspath(__file__))
FONT_BOLD = os.environ.get("PANEL_FONT_BOLD", "/usr/share/fonts/opentype/inter/Inter-Bold.otf")
FONT_MED = os.environ.get("PANEL_FONT_MED", "/usr/share/fonts/opentype/inter/Inter-Medium.otf")

W, H = 142.24, 128.5  # 28 HP

BG = "#211D19"
BG_DARK = "#17140F"
LINE = "#4A4036"
INK = "#F0E6D2"
SOFT = "#A39682"
BRASS = "#C8954A"

# ---- LAYOUT (same numbers in src/Symbolic.cpp) ----
COL = [12.0, 35.6, 59.2, 82.8, 106.4, 130.0]
ROW_PLATE, ROW_STRIKE, ROW_HAT, ROW_CV, ROW_IO = 30.0, 51.0, 72.0, 94.5, 113.5

CONTROL = 3.2   # control label size (project standard)
JACK = 2.9      # jack label size (project standard)
SMALL = 2.4

fonts = {}


def text(s, x, y, size, font=FONT_BOLD, fill=INK, anchor="middle", spacing=0.04):
    """SVG path for string s; (x, y) is the baseline point in mm."""
    if font not in fonts:
        f = TTFont(font)
        fonts[font] = (f, f.getGlyphSet(), f.getBestCmap(), f["head"].unitsPerEm)
    f, gs, cmap, upm = fonts[font]
    scale = size / upm
    track = spacing * upm
    names = [cmap[ord(c)] for c in s]
    width = sum(gs[n].width + track for n in names) - track
    x0 = {"middle": x - width * scale / 2, "end": x - width * scale}.get(anchor, x)
    pen = SVGPathPen(gs)
    cx = 0.0
    for n in names:
        gs[n].draw(TransformPen(pen, (scale, 0, 0, -scale, x0 + cx * scale, y)))
        cx += gs[n].width + track
    text.width = width * scale
    return f'<path fill="{fill}" d="{pen.getCommands()}"/>'


parts = []
add = parts.append
add(f'<rect width="{W}" height="{H}" fill="{BG}"/>')

# title + a cymbal seen from the side (bell, bow, edge)
add(text("SYMBOLIC", 6.0, 11.6, 5.0, FONT_BOLD, INK, anchor="start", spacing=0.08))
cx, cy = 71.0, 9.6
add(f'<path d="M{cx-11} {cy+1.0} Q{cx-5} {cy-0.6} {cx-2.6} {cy-0.9} Q{cx-2.2} {cy-2.8} {cx} {cy-2.9} Q{cx+2.2} {cy-2.8} {cx+2.6} {cy-0.9} Q{cx+5} {cy-0.6} {cx+11} {cy+1.0}" stroke="{BRASS}" stroke-width="0.55" fill="none" stroke-linecap="round"/>')
add(f'<line x1="{cx}" y1="{cy-2.9}" x2="{cx}" y2="{cy+3.2}" stroke="{SOFT}" stroke-width="0.35"/>')
add(text("acoustic cymbal model", W - 6.0, 11.2, SMALL, FONT_MED, SOFT, anchor="end"))


def section(name, y):
    """Small heading with a rule, sitting above a row whose knobs are at y."""
    hy = y - 12.6
    add(text(name, 4.0, hy, SMALL, FONT_BOLD, BRASS, anchor="start", spacing=0.1))
    add(f'<line x1="{4.0 + text.width + 2.0:.2f}" y1="{hy - 0.85}" x2="{W - 4.0}" y2="{hy - 0.85}" stroke="{LINE}" stroke-width="0.3"/>')


def hints(x, y, left, right):
    add(text(left, x - 5.3, y + 5.0, 2.2, FONT_MED, SOFT, anchor="end"))
    add(text(right, x + 5.3, y + 5.0, 2.2, FONT_MED, SOFT, anchor="start"))


LBL = 7.8   # label baseline above a knob/jack centre

section("PLATE", ROW_PLATE)
for x, s in zip(COL, ["SIZE", "WEIGHT", "BELL", "WOBBLE", "TRASH", "CAST"]):
    add(text(s, x, ROW_PLATE - LBL, CONTROL))
hints(COL[0], ROW_PLATE, "8\"", "24\"")
hints(COL[1], ROW_PLATE, "thin", "heavy")
hints(COL[5], ROW_PLATE, "1", "16")

section("STRIKE & RING", ROW_STRIKE)
for x, s in zip(COL, ["POSITION", "STICK", "DECAY", "DAMP", "SHIMMER", "BUILD"]):
    add(text(s, x, ROW_STRIKE - LBL, CONTROL))
hints(COL[0], ROW_STRIKE, "bell", "edge")
hints(COL[1], ROW_STRIKE, "tip", "mallet")
hints(COL[3], ROW_STRIKE, "bright", "dark")
hints(COL[5], ROW_STRIKE, "fast", "slow")

section("TUNE  ·  HI-HAT  ·  OUTPUT", ROW_HAT)
for x, s in zip(COL, ["TUNE", "PEDAL", "CHICK", "WIDTH", "LEVEL", "HIT"]):
    add(text(s, x, ROW_HAT - LBL, CONTROL))
hints(COL[1], ROW_HAT, "open", "closed")

section("CV IN", ROW_CV)
for x, s in zip(COL, ["V/OCT", "SIZE", "POSITION", "STICK", "DECAY", "SHIMMER"]):
    add(text(s, x, ROW_CV - LBL + 0.6, JACK))

add(f'<line x1="4.0" y1="{ROW_IO - 12.6 - 0.85}" x2="{W - 4.0}" y2="{ROW_IO - 12.6 - 0.85}" stroke="{LINE}" stroke-width="0.3"/>')
for x, s in zip(COL[:4], ["TRIG", "VEL", "CHOKE", "PEDAL"]):
    add(text(s, x, ROW_IO - LBL + 0.6, JACK))
add(f'<rect x="{COL[4] - 8.0}" y="{ROW_IO - 11.0}" width="{COL[5] - COL[4] + 16.0}" height="17.0" rx="1.4" fill="{BRASS}"/>')
add(text("OUT L", COL[4], ROW_IO - LBL + 0.6, JACK, fill=BG_DARK))
add(text("OUT R", COL[5], ROW_IO - LBL + 0.6, JACK, fill=BG_DARK))

svg = (f'<svg xmlns="http://www.w3.org/2000/svg" width="{W}mm" height="{H}mm" viewBox="0 0 {W} {H}">\n'
       + "\n".join(parts) + "\n</svg>\n")
open(os.path.join(HERE, "Symbolic.svg"), "w").write(svg)
print("ok")

# --- Haer maker logo (res/haer_logo.py) on the bottom rail
import sys as _sys
_sys.path.insert(0, HERE)
import haer_logo
haer_logo.add_to_file(os.path.join(HERE, "Symbolic.svg"), x=W / 2, y=126.0, cap=3.0, bg=BG)
