#!/usr/bin/env python3
# Builds the Symbolic guide PDF:  python3 docs/make_guide.py out.pdf
# (needs Playwright + Chromium; uses panel-preview.png from the repo root)
import os, sys, base64, subprocess
ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
out = os.path.abspath(sys.argv[1] if len(sys.argv) > 1 else "Symbolic-Guide.pdf")
img = base64.b64encode(open(os.path.join(ROOT, "panel-preview.png"), "rb").read()).decode()

html = f"""<!doctype html><html><head><meta charset="utf-8"><style>
@page {{ size: A4; margin: 16mm 16mm 16mm 16mm; }}
body {{ font-family: Inter, 'Helvetica Neue', Arial, sans-serif; color: #1d1d1f; font-size: 10.5pt; line-height: 1.45; }}
h1 {{ font-size: 26pt; margin: 0 0 2mm; letter-spacing: 0.04em; }}
h1 span {{ color: #a8742e; }}
h2 {{ font-size: 13.5pt; margin: 7mm 0 2mm; border-bottom: 1.5px solid #c8954a; padding-bottom: 1mm; }}
h3 {{ font-size: 11pt; margin: 4mm 0 1mm; }}
.lede {{ font-size: 11.5pt; color: #444; }}
.cols {{ display: flex; gap: 8mm; align-items: flex-start; }}
.cols img {{ width: 92mm; border-radius: 2mm; }}
table {{ border-collapse: collapse; width: 100%; font-size: 9.6pt; }}
td, th {{ border-bottom: 1px solid #e3e0d8; padding: 1.3mm 2mm; text-align: left; vertical-align: top; }}
th {{ background: #f4f1ea; }}
td:first-child {{ font-weight: 700; white-space: nowrap; }}
.recipe {{ background: #f7f5ef; border-left: 3px solid #c8954a; padding: 2.5mm 4mm; margin: 3mm 0; page-break-inside: avoid; }}
.recipe b.t {{ display: block; font-size: 11pt; margin-bottom: 1mm; }}
.flow {{ font-weight: 700; letter-spacing: 0.02em; background: #211d19; color: #f0e6d2; padding: 2.5mm 4mm; border-radius: 1.5mm; }}
.small {{ font-size: 9pt; color: #555; }}
.break {{ page-break-before: always; }}
</style></head><body>

<h1>SYMB<span>OLIC</span></h1>
<p class="lede">A real-sounding acoustic cymbal (and hi-hat) for VCV Rack. No samples: it models a bronze plate
ringing, the stick that hits it, and the shimmer that blooms after a hard hit.</p>

<div class="cols">
<img src="data:image/png;base64,{img}">
<div>
<h3>What's inside</h3>
<p class="flow">stick hit &rarr; 228 ringing tones of the plate &rarr; shimmer &rarr; stereo OUT</p>
<p>A cymbal rings at hundreds of unrelated pitches at once. Symbolic has 228 of them: a few low ones that give the
cymbal its body and slow "wobble", a crowd of high ones that make the sizzle, and 12 bell tones for the dome.</p>
<p>The stick decides which of those ring and how bright the hit is: <b>where</b> you hit (bell, bow, edge) and
<b>what</b> you hit with (hard tip or soft mallet).</p>
<p>Then the part that makes cymbals sound like cymbals: when a cymbal is hit hard, the big low vibrations pass their
energy up to the high tones over the first moments. That's the hiss that blooms just after a crash. Soft hits barely
do it, which is why a gently played ride "pings" and a hard one "washes". Symbolic does the same thing.</p>
<p class="small">How it differs from 808 Metal: that module copies the TR-808 circuit (six square waves through filters).
Symbolic models the real thing.</p>
</div></div>

<h2>Quick start</h2>
<p>Patch a gate into <b>TRIG</b>, <b>OUT L</b> (and <b>OUT R</b> for stereo) to your mixer, and press <b>HIT</b> to audition.
Right-click the module for three starting points: <i>Ride 20"</i>, <i>Crash 16"</i> and <i>Hi-hat 14"</i>.
Patch a velocity or accent CV into <b>VEL</b> (0-10 V). Velocity changes more than volume here: harder hits are
brighter and wash more, as on a real cymbal. Unpatched, every hit is 7.5 V.</p>

<h2>The controls</h2>
<table>
<tr><th>Control</th><th>What you hear</th></tr>
<tr><td>SIZE + CV</td><td>Diameter, 8" to 24". Bigger is lower, darker and rings longer. Small sizes are quick and splashy.</td></tr>
<tr><td>WEIGHT</td><td>Thin to heavy. Thin cymbals are low, quick and washy; heavy ones are higher, ring longer and "ping" more.</td></tr>
<tr><td>BELL</td><td>How big the dome in the middle is. Matters most when POSITION is near the bell: more BELL gives a clearer, longer bell tone.</td></tr>
<tr><td>WOBBLE</td><td>A real cymbal is never quite round, so its low tones come in close pairs that beat slowly against each other. That's the "wah-wah" under a ride. 0 is a perfect disc.</td></tr>
<tr><td>TRASH</td><td>Turns it into a china: the pitch bends down as the hit dies, the beating gets fast and the wash gets gritty.</td></tr>
<tr><td>CAST</td><td>Sixteen individual cymbals with the same settings, like picking another cymbal off the shelf. Each has its own pattern of tones.</td></tr>
<tr><td>POSITION + CV</td><td>Where the stick lands. Bell: a clear, bell-like tone. Bow (middle): defined ping. Edge: big, low and washy (crash). The CV is read at each hit, so a sequencer can move the stick around.</td></tr>
<tr><td>STICK + CV</td><td>What you hit with. Tip: bright and sharp. Turn it up for the shoulder of the stick, then rods and soft mallets: darker and rounder. Also read at each hit.</td></tr>
<tr><td>DECAY + CV</td><td>How long it rings, from a short tick to over 30 seconds.</td></tr>
<tr><td>DAMP</td><td>How fast the high tones die compared with the low ones. Left: bright, sizzling tail. Right: the tail turns dark (like an old, dirty cymbal or tape on it).</td></tr>
<tr><td>SHIMMER + CV</td><td>How much energy moves up into the high wash after hard hits. Low: pingy and tonal. High: explosive wash.</td></tr>
<tr><td>BUILD</td><td>How quickly that wash blooms: fast is an immediate hiss, slow lets you hear the hit first and the wash after.</td></tr>
<tr><td>TUNE + V/OCT</td><td>Shifts every tone up or down (±2 octaves).</td></tr>
<tr><td>PEDAL + CV</td><td>Turns it into a hi-hat. Open (left) rings freely; half-way the two plates rattle against each other (sizzle); closed (right) is a tight "tsst". The CV (0-10 V) adds to the knob, and negative CV opens it again.</td></tr>
<tr><td>CHICK</td><td>Level of the foot "chick" you hear when the pedal closes past 80 %.</td></tr>
<tr><td>WIDTH</td><td>Stereo width. The two outputs are like two microphones at different spots.</td></tr>
<tr><td>LEVEL / HIT</td><td>Output level, and a button that plays a hit (it lights up while the cymbal rings).</td></tr>
<tr><td>CHOKE</td><td>A hand grabbing the cymbal: a trigger grabs it for 150 ms; a held gate keeps holding it.</td></tr>
</table>
<p class="small">Hits overlap like on a real cymbal: a new hit adds to the ringing instead of restarting it. So a fast ride
pattern builds up a natural bed of wash.</p>

<h2 class="break">Recipes</h2>

<div class="recipe"><b class="t">1. Hi-hat, one module</b>
Right-click &rarr; <i>Hi-hat 14"</i>, then turn PEDAL fully left (open). Patch your hat trigger to TRIG, and a
"closed" gate to the <b>PEDAL</b> jack (10 V = closed). Where the gate is low, the hat is open; when it goes high,
you get the foot chick and the ring is cut. For accents, patch a velocity into VEL.
<br><i>Try:</i> a slow LFO (0-7 V) into PEDAL instead of a gate, for a foot that slowly opens and closes over bars.</div>

<div class="recipe"><b class="t">2. Detroit ride</b>
<i>Ride 20"</i>. Trigger on every quarter and the off-beat 16th before it (the "spang-a-lang"); velocity 7 V on the
beats, 4 V on the skips. Let STICK sit at 10 %.
Send a slow random voltage to <b>POSITION</b> CV (±0.5 V) so the stick wanders slightly on the bow. Every
4th bar, push POSITION fully left for bell hits.</div>

<div class="recipe"><b class="t">3. Crash with a hand choke</b>
<i>Crash 16"</i>. Trigger on bar 1, and patch a gate that rises on beat 4 of bar 2 into <b>CHOKE</b>: the classic
grabbed crash before a drop.</div>

<div class="recipe"><b class="t">4. Self-mutating cymbal (Autechre direction)</b>
Fast clock into TRIG. Feed slow, unrelated random voltages (Marbles X outputs, or a VCV Random module) into
POSITION, STICK and SHIMMER CVs, and a random velocity into VEL. Each hit lands somewhere different on a slightly
different stick, and because hits overlap, the wash keeps re-shaping itself without ever repeating.</div>

<div class="recipe"><b class="t">5. Tuned metal</b>
WOBBLE 0, DECAY high, DAMP right, SHIMMER low, POSITION near the bell. Sequence <b>V/OCT</b>: inharmonic, gamelan-ish
bell tones. Change CAST to get a different "instrument".</div>

<h2>Notes</h2>
<p class="small">Levels: a hard hit peaks around 5-7 V; the output is softly limited at 10 V. With OUT R unpatched,
OUT L carries a mono mix. CPU: about 2 % of one core, the same on any setting. Background: Perrin et al.,
<i>The normal modes of cymbals</i>; Touz&eacute;, Chaigne and Ducceschi on the energy cascade in cymbals and gongs.</p>
</body></html>"""

tmp = out + ".html"
open(tmp, "w").write(html)
js = f"""const {{chromium}} = require('playwright');
(async () => {{ const b = await chromium.launch(); const p = await b.newPage();
await p.goto('file://{tmp}'); await p.pdf({{path: '{out}', format: 'A4', printBackground: true}}); await b.close(); }})();"""
open(out + ".js", "w").write(js)
subprocess.run(["node", out + ".js"], check=True, cwd="/tmp")
os.remove(out + ".js"); os.remove(tmp)
print("wrote", out)
