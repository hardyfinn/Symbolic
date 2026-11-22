# Symbolic

An acoustic cymbal for VCV Rack 2, by Haer. Not a sample and not an 808-style
square-wave cymbal: it models a bronze plate ringing at 228 frequencies, how the
stick hits it, and the shimmer that blooms after a hard hit. Includes a hi-hat pedal.

* **Plate:** SIZE, WEIGHT, BELL, WOBBLE (beating of the low tones), TRASH (china), CAST (16 individual cymbals)
* **Strike & ring:** POSITION (bell / bow / edge), STICK (tip to mallet), DECAY, DAMP, SHIMMER, BUILD
* **Hi-hat & output:** TUNE, PEDAL (open to closed, with half-open sizzle), CHICK (foot chick), WIDTH, LEVEL, HIT
* **Jacks:** V/OCT, SIZE, POSITION, STICK, DECAY, SHIMMER CVs; TRIG, VEL, CHOKE, PEDAL; stereo OUT

Right-click for starting points (ride, crash, hi-hat).

Builds: every push builds a Mac (Apple Silicon) `.vcvplugin` with GitHub Actions
(see the *preview* / *latest* releases). `make test` runs the engine offline and
writes example hits to `test/out/`.

## How the sound is made
See the comments at the top of `src/SymbolicEngine.hpp`. Background reading:
Perrin et al., *The normal modes of cymbals* (IOA); Touzé, Chaigne, Ducceschi on the
nonlinear energy cascade in thin plates and cymbals; Dan Stowell's cymbal synthesis tutorial.

Panel: `python3 res/make_panel.py` (needs fonttools); mock-up: `python3 tools/preview.py`.

## Licence

MIT (see `LICENSE.txt`): you're free to use, change, port (VST/CLAP, other platforms, hardware) and share this code, as long as the copyright notice and licence text come along. Fonts on the panel (Inter, Climate Crisis) are under the SIL Open Font Licence.
