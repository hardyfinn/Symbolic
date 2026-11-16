// Symbolic: an acoustic cymbal model (no Rack code here, so test/ can run it offline).
//
// How it works, in plain words:
//  * A real cymbal is a thin bronze plate that rings at hundreds of unrelated
//    frequencies ("modes"). We model 228 of them as tiny ringing filters:
//      - 192 "plate" modes, sparse and low at the bottom, crowded at the top,
//      - 24 "twin" modes: the lowest plate modes come in near-identical pairs
//        on a real (never quite round) cymbal, and the pair beats slowly. That
//        is the wobble you hear on a ride,
//      - 12 "bell" modes for the dome in the middle, close to a church-bell series.
//  * The stick is a short half-sine push. A hard tip is short (bright),
//    a soft mallet is long (dark). WHERE you hit sets how much each mode is
//    excited (bell, bow, edge).
//  * The shimmer: on a real cymbal, loud low vibrations pass their energy up to
//    the high modes over the first tenths of a second (a nonlinear "energy
//    cascade", Touze, Chaigne, Ducceschi et al.). That is why a crash swells
//    after the hit and turns into a hiss. We follow the energy of the low modes,
//    and feed noise into the high modes in proportion to it, a little later
//    (BUILD), while draining the low modes. Soft hits barely do this, hard hits
//    do it a lot, just like the real thing.
//  * TRASH adds what thin, flanged (china) cymbals do: the pitch bends down as
//    the hit dies, the twins beat faster and the wash gets gritty.
//  * PEDAL is a hi-hat: it damps all modes, and half-way it lets the two plates
//    chatter (sizzle). Closing it past 80 % plays a foot "chick".
#pragma once
#include <cmath>
#include <cstdint>
#include <algorithm>

namespace symbolic {

static const int NP = 192;          // plate modes
static const int NT = 24;           // twins of the lowest plate modes
static const int NB = 12;           // bell modes
static const int N = NP + NT + NB;  // 228
static const int BLOCK = 16;        // coefficients are refreshed every 16 samples
static const float PI = 3.14159265358979f;

struct Controls {
	float size = 18.f;      // inches, 8..24
	float weight = 0.5f;    // 0 paper thin .. 1 heavy
	float bell = 0.5f;      // 0 no dome .. 1 big bell
	float wobble = 0.4f;    // 0 perfectly round .. 1 strong beating of the twin modes
	float trash = 0.f;      // 0 .. 1 china-ness
	int cast = 0;           // which individual cymbal, 0..15
	float tune = 0.f;       // octaves (knob + V/OCT)
	float position = 0.55f; // 0 bell .. 0.5 bow .. 1 edge
	float stick = 0.1f;     // 0 hard tip .. 1 soft mallet
	float decay = 0.55f;    // 0..1
	float damp = 0.35f;     // 0 bright ring .. 1 dark
	float shimmer = 0.5f;   // 0..1
	float build = 0.35f;    // 0..1 (how slowly the wash swells)
	float pedal = 0.f;      // 0 open .. 1 closed hi-hat
	float chick = 0.5f;     // foot chick level
	float width = 0.7f;     // stereo spread
	float level = 1.f;
	bool choke = false;     // held = hand on the cymbal
};

static inline float smoothstep(float a, float b, float x) {
	float t = std::min(std::max((x - a) / (b - a), 0.f), 1.f);
	return t * t * (3.f - 2.f * t);
}

struct Rng {
	uint32_t s = 0x9E3779B9u;
	void seed(uint32_t v) { s = v * 2654435761u + 0x6D2B79F5u; if (!s) s = 1; for (int i = 0; i < 4; i++) next(); }
	uint32_t next() { s ^= s << 13; s ^= s >> 17; s ^= s << 5; return s; }
	float uni() { return (next() >> 8) * (1.f / 16777216.f); }  // 0..1
	float bi() { return uni() * 2.f - 1.f; }                      // -1..1
};

static const float BELL_RATIO[NB] = {0.5f, 1.f, 1.19f, 1.5f, 2.f, 2.51f, 2.66f, 3.01f, 4.02f, 5.33f, 6.1f, 8.04f};

struct Engine {
	// resonator state and coefficients
	alignas(16) float x[N] = {}, y[N] = {};
	alignas(16) float e[N] = {}, r[N] = {};
	alignas(16) float a[N] = {};   // strike gain per mode (set at each hit)
	alignas(16) float h[N] = {};   // wash (cascade noise) gain per mode
	alignas(16) float oL[N] = {}, oR[N] = {};
	float f[N] = {};               // current frequencies (Hz)
	bool low[N] = {};              // part of the low set whose energy drives the wash

	// per-cymbal random numbers (fixed for a given CAST)
	float jit[N], phi[N], rad[N], tw[NT], sgnL[N], sgnR[N], magL[N], magR[N];
	int castNow = -1;

	float fs = 48000.f;
	int blockPos = 0;
	Rng rng, crackRng;

	// strike pulse
	int pulseLen = 0, pulsePos = 0;
	float pulseAmp = 0.f;
	float lastVel = 0.f;
	float hitEnv = 0.f, hitEnvK = 0.9999f;  // drives the half-open hi-hat chatter

	// cascade
	float energy = 0.f, cascade = 0.f, satFast = 0.f, sat = 0.f, washAmp = 0.f;
	// pedal / choke
	float pedalS = 0.f;
	bool chickArmed = true;
	int chokeHold = 0;
	float chokeS = 0.f;
	// air band and output filters
	float airL = 0.f, airR = 0.f, airPrevL = 0.f, airPrevR = 0.f;
	float dcL = 0.f, dcR = 0.f;
	float extGain = 0.f;

	// level calibration (see test/engine_test.cpp)
	static constexpr float E_REF = 6.0f;
	static constexpr float OUT_GAIN = 0.55f;

	float outL = 0.f, outR = 0.f;
	float meter = 0.f;

	void setSampleRate(float sr) { fs = sr; }

	void makeCast(int c) {
		castNow = c;
		Rng g;
		g.seed(1000u + (uint32_t) c * 7919u);
		for (int k = 0; k < N; k++) {
			jit[k] = g.bi() * 0.5f;
			phi[k] = g.uni() * PI;
			rad[k] = g.bi() * 0.3f;
			sgnL[k] = (g.next() & 1) ? 1.f : -1.f;
			sgnR[k] = (g.next() & 1) ? 1.f : -1.f;
			magL[k] = 0.55f + 0.45f * g.uni();
			magR[k] = 0.55f + 0.45f * g.uni();
		}
		for (int t = 0; t < NT; t++) tw[t] = g.uni();
	}

	float strikeWeight(int k, float rr, const Controls& c) const {
		if (k < NP + NT) {
			int pk = k < NP ? k : k - NP;
			float n = 0.6f * std::sqrt((float) pk) + rad[pk];
			float ph = phi[pk] + (k >= NP ? PI * 0.5f : 0.f);
			float s = 0.2f + 0.8f * std::fabs(std::sin(PI * (n + 0.5f) * rr + ph));
			float fk = f[k] > 0.f ? f[k] : 1000.f;
			float lowness = 1.f / (1.f + (fk / 1200.f) * (fk / 1200.f));
			s *= 1.f - lowness * (1.f - (0.12f + 0.88f * rr * rr));
			s *= 0.35f + 0.65f * smoothstep(0.1f, 0.35f, rr);
			if (k >= NP) s *= 0.7f;
			return s;
		}
		int j = k - NP - NT;
		float onBell = 1.f - smoothstep(0.1f, 0.45f, rr);
		return c.bell * 1.6f * (0.1f + 0.9f * onBell) / (1.f + 0.25f * j);
	}

	void setStrike(float pos, const Controls& c) {
		float rr = 0.06f + 0.92f * std::min(std::max(pos, 0.f), 1.f);
		for (int k = 0; k < N; k++) a[k] = strikeWeight(k, rr, c);
	}

	// a hit: vel 0..1 (more is allowed), position and stick from the controls
	void trigger(float vel, const Controls& c) { hit(vel, c.position, c.stick, c); }

	void hit(float vel, float pos, float stick, const Controls& c) {
		if (castNow != c.cast) makeCast(c.cast);
		if (f[0] == 0.f) updateBlock(c);
		setStrike(pos, c);
		float tau = 0.00012f * std::pow(70.f, std::min(std::max(stick, 0.f), 1.f));
		tau *= 1.3f - 0.5f * std::min(vel, 1.f);
		pulseLen = std::max(1, (int) (tau * fs));
		pulsePos = 0;
		pulseAmp = vel * PI / (2.f * pulseLen) * std::pow(tau / 0.0002f, 0.25f);  // area = vel, soft sticks a bit louder
		lastVel = vel;
		hitEnv = std::min(1.f, hitEnv + vel);
		hitEnvK = std::exp(-6.91f / (0.7f * fs));
	}

	void chokeNow() { chokeHold = (int) (0.15f * fs); }

	void updateBlock(const Controls& c) {
		if (castNow != c.cast) makeCast(c.cast);
		const float D = std::min(std::max(c.size, 6.f), 26.f);
		const float w = c.weight;
		const float dt = 1.f / fs;
		const float bdt = BLOCK * dt;

		// ---- energy of the low modes -> cascade envelope
		float E = 0.f;
		for (int k = 0; k < N; k++)
			if (low[k]) E += 0.5f * (x[k] * x[k] + y[k] * y[k]);
		energy = E;
		float tauB = 0.003f * std::pow(200.f, c.build);  // 3 ms .. 0.6 s
		float kB = std::min(1.f, bdt / tauB);
		cascade += (E - cascade) * kB;
		// amplitude^1.5: soft hits shimmer much less than hard ones, but the wash
		// still lasts as long as the low modes ring
		float cn = std::pow(cascade / E_REF, 0.75f);
		sat = cn / (1.f + cn);
		float en = std::pow(E / E_REF, 0.75f);
		satFast = en / (1.f + en);
		meter = std::min(1.f, std::sqrt(en));

		// ---- pedal (smoothed) and choke
		pedalS += (c.pedal - pedalS) * std::min(1.f, bdt / 0.004f);
		float chokeT = (c.choke || chokeHold > 0) ? 1.f : 0.f;
		chokeS += (chokeT - chokeS) * std::min(1.f, bdt / 0.02f);
		if (chokeHold > 0) chokeHold -= BLOCK;

		// ---- frequencies
		float f1 = 46.f * std::pow(18.f / D, 1.6f) * (0.6f + 0.8f * w);
		float fmax = 19000.f;
		float ratio = fmax / f1;
		float pitch = std::exp2(c.tune) * (1.f + c.trash * 0.05f * satFast);
		for (int k = 0; k < NP; k++) {
			float u = std::max(0.f, ((float) k + jit[k] * 0.8f) / (NP - 1));
			f[k] = f1 * std::pow(ratio, std::sqrt(u));
		}
		for (int t = 0; t < NT; t++) {
			float split = (0.0015f + 0.008f * tw[t]) * 2.5f * c.wobble * (1.f + 3.f * c.trash) + 0.0002f;
			f[NP + t] = f[t] * (1.f + split);
		}
		float fb = 560.f * std::pow(18.f / D, 0.7f) * (0.8f + 0.4f * w);
		for (int j = 0; j < NB; j++) f[NP + NT + j] = fb * BELL_RATIO[j] * (1.f + 0.01f * jit[NP + NT + j]);

		// ---- decay times
		float Tlow = 0.4f * std::pow(100.f, c.decay) * std::pow(D / 18.f, 0.7f) * (0.6f + 0.8f * w);
		float fd = 18000.f * std::pow(1000.f / 18000.f, c.damp);
		float fc = 1500.f * std::sqrt(18.f / D);
		float drain = c.shimmer * sat * 1.5f;
		float lamPedal = 80.f * pedalS * pedalS * pedalS;
		float lamPedalLow = 40.f * pedalS * pedalS;
		float lamChoke = 30.f * chokeS;
		float washScale = c.shimmer * sat;
		washAmp = washScale;

		for (int k = 0; k < N; k++) {
			float fk = f[k] * pitch;
			bool bellMode = k >= NP + NT;
			float T60 = bellMode ? Tlow * (1.f + c.bell) / (1.f + std::sqrt(fk / (2.f * fd)))
			                     : Tlow / (1.f + fk / fd);
			T60 = std::max(T60, 0.04f);
			bool isLow = !bellMode && f[k] < fc;
			low[k] = isLow;
			float lamOwn = 6.91f / T60;
			float lam = lamOwn + lamPedal + lamChoke + (isLow ? drain + lamPedalLow : 0.f);
			if (fk < 0.46f * fs) {
				e[k] = 2.f * std::sin(PI * fk * dt);
				r[k] = std::exp(-lam * dt);
				// wash: modes above fc, normalised so each settles at a similar level
				float hw = bellMode ? 0.f : smoothstep(0.5f * fc, 2.f * fc, f[k]) * std::pow(f[k] / fc, -0.3f);
				h[k] = hw * std::sqrt(2.f * lamOwn * dt);
			} else {
				e[k] = 0.f; r[k] = 0.f; h[k] = 0.f; x[k] = 0.f; y[k] = 0.f;
			}
			// low modes radiate less than the bright middle (and it keeps the body from booming)
			float tilt = f[k] < 700.f ? std::sqrt(f[k] / 700.f) : 1.f;
			oL[k] = sgnL[k] * magL[k] * tilt;
			oR[k] = oL[k] + (sgnR[k] * magR[k] * tilt - oL[k]) * c.width;
		}

		// ---- hi-hat foot chick: pedal closes past 80 %
		if (chickArmed && pedalS > 0.8f) {
			chickArmed = false;
			if (c.chick > 0.f) hit(0.45f * c.chick, 0.9f, 0.45f, c);
		}
		if (pedalS < 0.6f) chickArmed = true;
	}

	float crackle(float rate) {
		if (crackRng.uni() < rate / fs) return crackRng.bi() * 2.f;
		return 0.f;
	}

	// one sample; result in outL/outR (volts)
	void process(const Controls& c) {
		if (blockPos == 0) updateBlock(c);
		if (++blockPos >= BLOCK) blockPos = 0;

		// strike pulse
		float sA = 0.f;
		if (pulsePos < pulseLen) {
			sA = pulseAmp * std::sin(PI * (pulsePos + 0.5f) / pulseLen);
			pulsePos++;
		}
		// hi-hat chatter: half-open plates rattle against each other
		float p = pedalS;
		float chatter = (p > 0.1f && p < 0.9f) ? 4.f * p * (1.f - p) : 0.f;
		hitEnv *= hitEnvK;
		if (chatter > 0.f) sA += crackle(1200.f) * chatter * 0.06f * hitEnv;

		// wash noise (gritty with TRASH)
		float white = rng.bi() * 1.7f;
		float sN = (1.f - 0.6f * c.trash) * white + c.trash * crackle(2500.f) * 2.5f;
		sN *= washAmp * 1.6f;

		float L = 0.f, R = 0.f;
		for (int k = 0; k < N; k++) {
			float xk = r[k] * (x[k] + e[k] * y[k]) + sA * a[k] + sN * h[k];
			float yk = r[k] * y[k] - e[k] * xk;
			x[k] = xk;
			y[k] = yk;
			L += xk * oL[k];
			R += xk * oR[k];
		}

		// air: the hiss between the modes, follows the wash (own noise per side)
		float w2 = rng.bi() * 1.7f;
		float hl = white - airPrevL, hr = w2 - airPrevR;
		airPrevL = white; airPrevR = w2;
		airL += (hl - airL) * 0.55f;
		airR += (hr - airR) * 0.55f;
		float ag = washAmp * 0.05f;
		L += airL * ag;
		R += (airL + (airR - airL) * c.width) * ag;

		// gentle DC / rumble removal and soft ceiling
		dcL += (L - dcL) * 0.004f; L -= dcL;
		dcR += (R - dcR) * 0.004f; R -= dcR;
		float g = OUT_GAIN * c.level;
		outL = 10.f * std::tanh(L * g * 0.1f);
		outR = 10.f * std::tanh(R * g * 0.1f);
	}
};

}  // namespace symbolic
