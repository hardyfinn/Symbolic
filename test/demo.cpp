// Renders the demo:  g++ -O3 -funsafe-math-optimizations -std=c++17 -Isrc test/demo.cpp -o test/demo && test/demo out.wav
// 125 BPM, 16 bars: hi-hat (pedal), ride, crash, a china, then the cymbal-only tail.
#include "SymbolicEngine.hpp"
#include <cstdio>
#include <vector>
#include <string>
using namespace symbolic;
static const int SR = 48000;

struct Voice {
	Engine e; Controls c; float gain;
	Voice(float g) : gain(g) { e.setSampleRate(SR); }
};

int main(int argc, char** argv) {
	const char* out = argc > 1 ? argv[1] : "symbolic-demo.wav";
	const double bpm = 125, step = 60.0 / bpm / 4;  // 16th
	const int bars = 16, steps = bars * 16;
	const int n = (int) ((steps * step + 4.0) * SR);
	std::vector<float> L(n), R(n);

	Voice hat(0.55f), ride(0.6f), crash(0.5f), china(0.45f);
	hat.c.size = 14; hat.c.weight = 0.6f; hat.c.bell = 0.3f; hat.c.position = 0.75f; hat.c.stick = 0.08f;
	hat.c.decay = 0.45f; hat.c.damp = 0.3f; hat.c.shimmer = 0.4f; hat.c.build = 0.1f; hat.c.pedal = 1.f; hat.c.cast = 3;
	ride.c.size = 20; ride.c.weight = 0.7f; ride.c.bell = 0.6f; ride.c.wobble = 0.45f; ride.c.position = 0.5f; ride.c.stick = 0.08f;
	ride.c.decay = 0.6f; ride.c.damp = 0.3f; ride.c.shimmer = 0.3f; ride.c.build = 0.3f; ride.c.cast = 7;
	crash.c.size = 16; crash.c.weight = 0.35f; crash.c.bell = 0.4f; crash.c.position = 0.95f; crash.c.stick = 0.2f;
	crash.c.decay = 0.5f; crash.c.damp = 0.35f; crash.c.shimmer = 0.75f; crash.c.build = 0.45f; crash.c.cast = 1;
	china.c.size = 18; china.c.weight = 0.3f; china.c.trash = 0.9f; china.c.position = 0.9f; china.c.stick = 0.15f;
	china.c.decay = 0.45f; china.c.shimmer = 0.8f; china.c.build = 0.3f; china.c.cast = 11;
	Voice* vs[4] = {&hat, &ride, &crash, &china};
	for (auto v : vs) v->e.process(v->c);

	uint32_t rnd = 12345;
	auto rf = [&]() { rnd = rnd * 1664525u + 1013904223u; return (rnd >> 8) / 16777216.f; };

	int s = 0;
	for (int i = 0; i < n; i++) {
		double t = (double) i / SR;
		int st = (int) (t / step);
		if (st < steps && i == (int) (st * step * SR)) {
			int bar = st / 16, pos = st % 16;
			// hi-hat: bars 0-7. closed 16ths with accents, open on the off-beat 8th, closed again on the next 16th
			if (bar < 8) {
				bool open = (pos % 4 == 2);
				hat.c.pedal = open ? 0.15f : 1.f;
				float v = (pos % 4 == 0) ? 0.85f : (pos % 2 == 0 ? 0.7f : 0.45f);
				hat.c.position = 0.72f + 0.06f * rf();
				hat.e.trigger(v + 0.1f * rf(), hat.c);
			}
			// ride: bars 4-15, quarter notes plus a skip; bell on bar 12-15 off-beats
			if (bar >= 4 && (pos % 4 == 0 || pos % 8 == 7)) {
				bool bellHit = bar >= 12 && pos % 8 == 4;
				ride.c.position = bellHit ? 0.06f : 0.48f + 0.06f * rf();
				ride.e.trigger(pos % 8 == 7 ? 0.45f : 0.7f + 0.1f * rf(), ride.c);
			}
			// crash on bars 0, 4, 8; china on bar 10 beat 3 and bar 14 beat 1
			if (pos == 0 && (bar == 0 || bar == 4 || bar == 8)) crash.e.trigger(1.f, crash.c);
			if ((bar == 10 && pos == 8) || (bar == 14 && pos == 0)) china.e.trigger(1.f, china.c);
			// choke the crash at the end of bar 15
			if (bar == 15 && pos == 12) crash.e.chokeNow();
			if (bar == 15 && pos == 14) { crash.e.trigger(1.f, crash.c); }
		}
		// pedal comes back down shortly after an open hat (foot), giving the chick
		float l = 0, r = 0;
		for (auto v : vs) { v->e.process(v->c); l += v->e.outL * v->gain; r += v->e.outR * v->gain; }
		L[i] = l; R[i] = r;
		s++;
	}
	float peak = 0;
	for (int i = 0; i < n; i++) peak = std::max(peak, std::max(std::fabs(L[i]), std::fabs(R[i])));
	float g = 0.89f / peak;
	FILE* fp = fopen(out, "wb");
	uint32_t data = n * 4, riff = 36 + data, sr = SR, br = SR * 4, fmt = 16;
	uint16_t pcm = 1, ch = 2, ba = 4, bits = 16;
	fwrite("RIFF", 1, 4, fp); fwrite(&riff, 4, 1, fp); fwrite("WAVEfmt ", 1, 8, fp);
	fwrite(&fmt, 4, 1, fp); fwrite(&pcm, 2, 1, fp); fwrite(&ch, 2, 1, fp); fwrite(&sr, 4, 1, fp);
	fwrite(&br, 4, 1, fp); fwrite(&ba, 2, 1, fp); fwrite(&bits, 2, 1, fp); fwrite("data", 1, 4, fp); fwrite(&data, 4, 1, fp);
	for (int i = 0; i < n; i++) {
		int16_t a = (int16_t) (L[i] * g * 32767), b = (int16_t) (R[i] * g * 32767);
		fwrite(&a, 2, 1, fp); fwrite(&b, 2, 1, fp);
	}
	fclose(fp);
	printf("wrote %s (%.1f s, peak %.2f V before normalising)\n", out, (float) n / SR, peak);
	return 0;
}
