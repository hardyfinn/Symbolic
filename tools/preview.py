#!/usr/bin/env python3
# Mock-up picture of the panel with knobs, jacks and the switch drawn in, so
# the layout can be checked without Rack.  python3 tools/preview.py out.png
import os, sys, math, subprocess
HERE = os.path.dirname(os.path.abspath(__file__))
svg = open(os.path.join(HERE, "..", "res", "Symbolic.svg")).read()
L = {}
src = open(os.path.join(HERE, "..", "res", "make_panel.py")).read()
exec(src.split("# ---- LAYOUT")[1].split("CONTROL =")[0].split("\n", 1)[1], L)
C = L["COL"]
g = []
def jack(x, y):
    g.append(f'<circle cx="{x}" cy="{y}" r="4.1" fill="#C9CCD1"/><circle cx="{x}" cy="{y}" r="3.1" fill="#7C8088"/><circle cx="{x}" cy="{y}" r="1.9" fill="#0B0B0C"/>')
def knob(x, y, a, r=4.8):
    g.append(f'<circle cx="{x}" cy="{y}" r="{r}" fill="#26272B" stroke="#3A3C42" stroke-width="0.5"/>')
    ang = math.radians(-135 + 270 * a)
    g.append(f'<line x1="{x}" y1="{y}" x2="{x + (r-0.8)*math.sin(ang):.2f}" y2="{y - (r-0.8)*math.cos(ang):.2f}" stroke="#EEE" stroke-width="0.7" stroke-linecap="round"/>')
for i, v in enumerate([0.62, 0.5, 0.5, 0.4, 0.0, 0.0]): knob(C[i], L["ROW_PLATE"], v)
for i, v in enumerate([0.55, 0.1, 0.55, 0.35, 0.5, 0.35]): knob(C[i], L["ROW_STRIKE"], v)
for i, v in enumerate([0.5, 0.0, 0.5, 0.7, 1.0]): knob(C[i], L["ROW_HAT"], v)
x, y = C[5], L["ROW_HAT"]
g.append(f'<circle cx="{x}" cy="{y}" r="3.6" fill="#2B2D33"/><circle cx="{x}" cy="{y}" r="2.6" fill="#5a4a22"/>')
for c in C: jack(c, L["ROW_CV"])
for c in C: jack(c, L["ROW_IO"])
W = 142.24
for x in (7.6, W - 7.6 - 5.08):
    for y in (2.5, 126.0): g.append(f'<circle cx="{x}" cy="{y}" r="1.6" fill="#8A8D93"/>')
svg = svg.replace("</svg>", "\n".join(g) + "\n</svg>")
out = os.path.abspath(sys.argv[1] if len(sys.argv) > 1 else "preview.png")
tmp = out + ".svg"; open(tmp, "w").write(svg)
pw, ph = 712, 643
open(tmp + ".html", "w").write(f'<html><body style="margin:0;background:#111"><img src="{os.path.basename(tmp)}" style="width:{pw}px;height:{ph}px"></body></html>')
js = f"""const {{chromium}} = require('playwright');
(async () => {{ const b = await chromium.launch(); const p = await b.newPage({{viewport: {{width: {pw}, height: {ph}}}, deviceScaleFactor: 2}});
await p.goto('file://{tmp}.html'); await p.waitForTimeout(300);
await p.screenshot({{path: '{out}'}}); await b.close(); }})();"""
open(out + ".js", "w").write(js)
subprocess.run(["node", out + ".js"], check=True, cwd="/tmp")
os.remove(out + ".js"); os.remove(tmp); os.remove(tmp + ".html")
print("wrote", out)
