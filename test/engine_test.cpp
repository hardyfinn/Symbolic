// Engine checks without Rack:  make test
// Renders each preset, checks level / silence / NaN, prints brightness over
// time (spectral centroid) and CPU cost, and writes test/out/*.wav to listen to.
#include "SymbolicEngine.hpp"
#include <complex>
#include <cstdio>
#include <vector>
#include <string>
#include <chrono>
#include <functional>
#include <sys/stat.h>
using namespace symbolic;
static int fails = 0;
#define CHECK(c, ...) do { if (!(c)) { fails++; printf("FAIL: " __VA_ARGS__); printf("\n"); } } while (0)
static const int SR = 48000;

void writeWav(const std::string& path, const std::vector<float>& L, const std::vector<float>& R) {
	FILE* fp = fopen(path.c_str(), "wb");
	if (!fp) return;
	uint32_t n = L.size(), data = n * 4, riff = 36 + data, sr = SR, br = SR * 4, fmt = 16;
	uint16_t pcm = 1, ch = 2, ba = 4, bits = 16;
	fwrite("RIFF", 1, 4, fp); fwrite(&riff, 4, 1, fp); fwrite("WAVEfmt ", 1, 8, fp);
	fwrite(&fmt, 4, 1, fp); fwrite(&pcm, 2, 1, fp); fwrite(&ch, 2, 1, fp); fwrite(&sr, 4, 1, fp);
	fwrite(&br, 4, 1, fp); fwrite(&ba, 2, 1, fp); fwrite(&bits, 2, 1, fp); fwrite("data", 1, 4, fp); fwrite(&data, 4, 1, fp);
	for (uint32_t i = 0; i < n; i++) {
		int16_t a = (int16_t) std::max(-32767.f, std::min(32767.f, L[i] / 10.f * 32767.f));
		int16_t b = (int16_t) std::max(-32767.f, std::min(32767.f, R[i] / 10.f * 32767.f));
		fwrite(&a, 2, 1, fp); fwrite(&b, 2, 1, fp);
	}
	fclose(fp);
}

void fft(std::vector<std::complex<float>>& a) {
	int n = a.size();
	for (int i = 1, j = 0; i < n; i++) {
		int bit = n >> 1;
		for (; j & bit; bit >>= 1) j ^= bit;
		j ^= bit;
		if (i < j) std::swap(a[i], a[j]);
	}
	for (int len = 2; len <= n; len <<= 1) {
		std::complex<float> wl = std::polar(1.f, -2.f * PI / len);
		for (int i = 0; i < n; i += len) {
			std::complex<float> w(1);
			for (int j = 0; j < len / 2; j++) {
				auto u = a[i + j], v = a[i + j + len / 2] * w;
				a[i + j] = u + v; a[i + j + len / 2] = u - v; w *= wl;
			}
		}
	}
}

// spectral centroid (Hz) and RMS of a window starting at t seconds
void analyse(const std::vector<float>& x, float t, float& centroid, float& rms, float lowFrac[1]) {
	const int n = 4096;
	int s0 = (int) (t * SR);
	std::vector<std::complex<float>> a(n);
	double r = 0;
	for (int i = 0; i < n; i++) {
		float v = (s0 + i < (int) x.size()) ? x[s0 + i] : 0.f;
		r += v * v;
		a[i] = v * (0.5f - 0.5f * std::cos(2 * PI * i / n));
	}
	rms = std::sqrt(r / n);
	fft(a);
	double num = 0, den = 0, lo = 0;
	for (int i = 1; i < n / 2; i++) {
		double m = std::norm(a[i]), fr = (double) i * SR / n;
		num += m * fr; den += m;
		if (fr < 1500) lo += m;
	}
	centroid = den > 0 ? num / den : 0;
	lowFrac[0] = den > 0 ? lo / den : 0;
}

struct Preset { const char* name; std::function<void(Controls&)> set; float vel; };

int main() {
	mkdir("test/out", 0755);
	std::vector<Preset> presets = {
		{"ride-20", [](Controls& c) { c.size = 20; c.weight = 0.7f; c.bell = 0.6f; c.position = 0.5f; c.stick = 0.08f; c.decay = 0.6f; c.damp = 0.3f; c.shimmer = 0.3f; c.build = 0.3f; }, 0.7f},
		{"ride-bell", [](Controls& c) { c.size = 20; c.weight = 0.7f; c.bell = 0.8f; c.position = 0.05f; c.stick = 0.05f; c.decay = 0.6f; c.damp = 0.3f; c.shimmer = 0.2f; }, 0.8f},
		{"crash-16", [](Controls& c) { c.size = 16; c.weight = 0.35f; c.bell = 0.4f; c.position = 0.95f; c.stick = 0.2f; c.decay = 0.5f; c.damp = 0.35f; c.shimmer = 0.75f; c.build = 0.45f; }, 1.f},
		{"splash-10", [](Controls& c) { c.size = 10; c.weight = 0.3f; c.bell = 0.3f; c.position = 0.9f; c.stick = 0.1f; c.decay = 0.3f; c.damp = 0.3f; c.shimmer = 0.6f; c.build = 0.2f; }, 0.9f},
		{"china-18", [](Controls& c) { c.size = 18; c.weight = 0.3f; c.bell = 0.3f; c.trash = 0.9f; c.position = 0.9f; c.stick = 0.15f; c.decay = 0.45f; c.damp = 0.4f; c.shimmer = 0.8f; c.build = 0.3f; }, 1.f},
		{"hat-closed", [](Controls& c) { c.size = 14; c.weight = 0.6f; c.bell = 0.3f; c.position = 0.75f; c.stick = 0.08f; c.decay = 0.45f; c.damp = 0.3f; c.shimmer = 0.4f; c.build = 0.1f; c.pedal = 1.f; c.chick = 0.f; }, 0.8f},
		{"hat-half", [](Controls& c) { c.size = 14; c.weight = 0.6f; c.bell = 0.3f; c.position = 0.75f; c.stick = 0.08f; c.decay = 0.45f; c.damp = 0.3f; c.shimmer = 0.4f; c.build = 0.1f; c.pedal = 0.5f; c.chick = 0.f; }, 0.8f},
		{"mallet", [](Controls& c) { c.size = 18; c.weight = 0.4f; c.position = 0.9f; c.stick = 0.95f; c.decay = 0.6f; c.shimmer = 0.6f; c.build = 0.7f; }, 1.f},
		{"default-soft", [](Controls&) {}, 0.25f},
		{"default-hard", [](Controls&) {}, 1.f},
	};

	for (auto& p : presets) {
		Engine eng;
		eng.setSampleRate(SR);
		Controls c;
		p.set(c);
		int n = SR * 4;
		std::vector<float> L(n), R(n);
		eng.process(c);
		eng.trigger(p.vel, c);
		float peak = 0;
		bool nan = false;
		for (int i = 0; i < n; i++) {
			eng.process(c);
			L[i] = eng.outL; R[i] = eng.outR;
			if (!std::isfinite(L[i]) || !std::isfinite(R[i])) nan = true;
			peak = std::max(peak, std::max(std::fabs(L[i]), std::fabs(R[i])));
		}
		CHECK(!nan, "%s: NaN", p.name);
		printf("%-13s peak %5.2f V |", p.name, peak);
		for (float t : {0.0f, 0.1f, 0.3f, 1.0f, 2.5f}) {
			float cen, rms, lo[1];
			analyse(L, t, cen, rms, lo);
			printf(" %.1fs:%5.0fHz %5.2fV lo%2.0f%% |", t, cen, rms, lo[0] * 100);
		}
		printf("\n");
		CHECK(peak < 10.5f, "%s: too loud", p.name);
		CHECK(peak > 0.5f, "%s: too quiet", p.name);
		writeWav(std::string("test/out/") + p.name + ".wav", L, R);
	}

	// choke: after a choke, the sound must be gone within 0.4 s
	{
		Engine eng; eng.setSampleRate(SR);
		Controls c; c.decay = 0.8f;
		eng.process(c); eng.trigger(1.f, c);
		for (int i = 0; i < SR / 2; i++) eng.process(c);
		eng.chokeNow();
		for (int i = 0; i < SR * 4 / 10; i++) eng.process(c);
		float m = 0;
		for (int i = 0; i < SR / 10; i++) { eng.process(c); m = std::max(m, std::fabs(eng.outL)); }
		printf("choke: residual %.4f V\n", m);
		CHECK(m < 0.05f, "choke leaves %.3f V", m);
	}

	// CPU
	{
		Engine eng; eng.setSampleRate(SR);
		Controls c;
		eng.process(c); eng.trigger(1.f, c);
		auto t0 = std::chrono::steady_clock::now();
		int n = SR * 10;
		float s = 0;
		for (int i = 0; i < n; i++) { eng.process(c); s += eng.outL; }
		double sec = std::chrono::duration<double>(std::chrono::steady_clock::now() - t0).count();
		printf("CPU: %.2f %% of one core at 48 kHz (%g)\n", sec / 10.0 * 100.0, s * 0);
	}

	printf(fails ? "%d FAILED\n" : "all ok\n", fails);
	return fails ? 1 : 0;
}
