#pragma once
// Shared helpers for the offline DSP tests: signal generators, block-wise
// rendering (like a host), and measurements (RMS, short-time tone level).

#include "dsp/Module.h"

#include <cmath>
#include <vector>

namespace testutil
{
using namespace vessel::dsp;

inline constexpr double kFs = 48000.0;
inline constexpr double kPiD = 3.14159265358979323846;

struct Stereo
{
    std::vector<float> l, r;
    explicit Stereo (size_t n = 0) : l (n, 0.0f), r (n, 0.0f) {}
    size_t size() const { return l.size(); }
};

inline Stereo sine (double hz, double seconds, float amp = 0.5f)
{
    Stereo s ((size_t) (seconds * kFs));
    for (size_t i = 0; i < s.size(); ++i)
        s.l[i] = s.r[i] = amp * (float) std::sin (2.0 * kPiD * hz * (double) i / kFs);
    return s;
}

// A chord with a little noise: closer to music than a single sine.
inline Stereo music (double seconds, float amp = 0.15f)
{
    Stereo s ((size_t) (seconds * kFs));
    Noise n (7);
    for (size_t i = 0; i < s.size(); ++i)
    {
        const double t = (double) i / kFs;
        const float v = amp * (float) (std::sin (2 * kPiD * 110 * t) + std::sin (2 * kPiD * 220.5 * t)
                                       + std::sin (2 * kPiD * 329.6 * t) + 0.5 * std::sin (2 * kPiD * 1318 * t));
        s.l[i] = v + 0.03f * n.next();
        s.r[i] = v + 0.03f * n.next();
    }
    return s;
}

inline Stereo withSilence (Stereo s, double seconds)
{
    s.l.resize (s.size() + (size_t) (seconds * kFs), 0.0f);
    s.r.resize (s.l.size(), 0.0f);
    return s;
}

// Processes in place, 32-sample blocks, with a running transport at `bpm`.
template <typename Engine>
void render (Engine& e, Stereo& s, double bpm = 120.0, int block = 32)
{
    Transport t;
    t.bpm = bpm;
    t.sampleRate = kFs;
    t.playing = true;
    for (size_t i = 0; i < s.size(); i += (size_t) block)
    {
        const int n = (int) std::min ((size_t) block, s.size() - i);
        e.process (&s.l[i], &s.r[i], n, t);
        t.ppq += t.beatsPerSample() * n;
    }
}

inline bool allFinite (const Stereo& s)
{
    for (size_t i = 0; i < s.size(); ++i)
        if (! std::isfinite (s.l[i]) || ! std::isfinite (s.r[i])) return false;
    return true;
}

inline float peak (const Stereo& s, size_t from = 0, size_t to = SIZE_MAX)
{
    float p = 0.0f;
    to = std::min (to, s.size());
    for (size_t i = from; i < to; ++i) p = std::max ({ p, std::abs (s.l[i]), std::abs (s.r[i]) });
    return p;
}

inline float rms (const std::vector<float>& x, size_t from = 0, size_t to = SIZE_MAX)
{
    to = std::min (to, x.size());
    double acc = 0.0;
    for (size_t i = from; i < to; ++i) acc += (double) x[i] * x[i];
    return (float) std::sqrt (acc / (double) std::max<size_t> (1, to - from));
}

inline float db (float x) { return 20.0f * std::log10 (std::max (x, 1.0e-12f)); }

// Average short-time magnitude of a tone at `hz` (1024-sample Hann frames).
// Short frames make it robust to the phase resets of grain-based effects.
inline float toneLevel (const std::vector<float>& x, double hz, size_t from = 0, size_t to = SIZE_MAX)
{
    to = std::min (to, x.size());
    double acc = 0.0;
    int frames = 0;
    for (size_t s = from; s + 1024 <= to; s += 512)
    {
        double re = 0, im = 0, ws = 0;
        for (int i = 0; i < 1024; ++i)
        {
            const double w = 0.5 - 0.5 * std::cos (2 * kPiD * i / 1023.0);
            const double a = 2 * kPiD * hz * (double) (s + (size_t) i) / kFs;
            re += w * x[s + (size_t) i] * std::cos (a);
            im += w * x[s + (size_t) i] * std::sin (a);
            ws += w;
        }
        acc += 2.0 * std::sqrt (re * re + im * im) / ws;
        ++frames;
    }
    return frames ? (float) (acc / frames) : 0.0f;
}

// Largest sample-to-sample jump: clicks show up as spikes far above what a
// smooth signal of the same level produces.
inline float maxStep (const std::vector<float>& x, size_t from = 1, size_t to = SIZE_MAX)
{
    to = std::min (to, x.size());
    float m = 0.0f;
    for (size_t i = std::max<size_t> (1, from); i < to; ++i) m = std::max (m, std::abs (x[i] - x[i - 1]));
    return m;
}

} // namespace testutil
