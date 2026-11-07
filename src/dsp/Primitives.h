#pragma once
// Small real-time-safe building blocks shared by every Vessel engine:
// smoothing, one-pole and state-variable filters, crossovers, noise, seeded
// hashing and the saturation curves. Plain C++ (no JUCE) so the DSP can be
// unit-tested on its own.

#include <algorithm>
#include <cmath>
#include <cstdint>

namespace vessel::dsp
{

inline constexpr float kPi    = 3.14159265358979323846f;
inline constexpr float kTwoPi = 6.28318530717958647692f;

// Flushes tiny values to zero so filter and feedback states never go denormal.
inline float flushDenormal (float x) noexcept { return std::abs (x) < 1.0e-15f ? 0.0f : x; }

inline float dbToGain (float db) noexcept { return std::pow (10.0f, db * 0.05f); }
inline float lerp (float a, float b, float t) noexcept { return a + t * (b - a); }
inline float clamp01 (float x) noexcept { return std::clamp (x, 0.0f, 1.0f); }

// Hann window value at position p in [0, 1].
inline float hann (float p) noexcept { const float s = std::sin (kPi * p); return s * s; }

// ---------------------------------------------------------------------------
// One-pole exponential smoother. timeMs is the time to cover ~63% of a jump.
class Smoother
{
public:
    void prepare (double sampleRate, float timeMs) noexcept { fs = (float) sampleRate; setTime (timeMs); }
    void setTime (float timeMs) noexcept { coeff = timeMs <= 0.0f ? 0.0f : std::exp (-1.0f / (0.001f * timeMs * fs)); }
    void snapTo (float v) noexcept     { current = target = v; }
    void setTarget (float v) noexcept  { target = v; }
    float getCurrent() const noexcept  { return current; }
    float getTarget() const noexcept   { return target; }
    float next() noexcept              { current = target + coeff * (current - target); return current; }

private:
    float fs = 48000.0f, coeff = 0.0f, current = 0.0f, target = 0.0f;
};

// ---------------------------------------------------------------------------
// Topology-preserving one-pole: stable under fast cutoff changes.
class OnePole
{
public:
    void prepare (double sampleRate) noexcept { fs = (float) sampleRate; }
    void reset() noexcept { s = 0.0f; }
    void setCutoff (float hz) noexcept
    {
        hz = std::clamp (hz, 5.0f, 0.45f * fs);
        const float g = std::tan (kPi * hz / fs);
        G = g / (1.0f + g);
    }
    float lowpass (float x) noexcept
    {
        const float v = (x - s) * G;
        const float lp = v + s;
        s = flushDenormal (lp + v);
        return lp;
    }
    float highpass (float x) noexcept { return x - lowpass (x); }

private:
    float fs = 48000.0f, G = 0.5f, s = 0.0f;
};

// ---------------------------------------------------------------------------
// State-variable filter (Simper / Cytomic TPT). All outputs from one tick.
// k = 1/Q; Butterworth is k = sqrt(2).
class Svf
{
public:
    struct Out { float lp, bp, hp; };

    void prepare (double sampleRate) noexcept { fs = (float) sampleRate; }
    void reset() noexcept { ic1 = ic2 = 0.0f; }

    void set (float hz, float k) noexcept
    {
        hz = std::clamp (hz, 10.0f, 0.45f * fs);
        g = std::tan (kPi * hz / fs);
        kk = std::max (k, 0.005f);
        a1 = 1.0f / (1.0f + g * (g + kk));
        a2 = g * a1;
        a3 = g * a2;
    }

    Out tick (float x) noexcept
    {
        const float v3 = x - ic2;
        const float v1 = a1 * ic1 + a2 * v3;
        const float v2 = ic2 + a2 * ic1 + a3 * v3;
        ic1 = flushDenormal (2.0f * v1 - ic1);
        ic2 = flushDenormal (2.0f * v2 - ic2);
        return { v2, v1, x - kk * v1 - v2 };
    }

    float k() const noexcept { return kk; }

private:
    float fs = 48000.0f, g = 0.1f, kk = 1.414f, a1 = 1.0f, a2 = 0.0f, a3 = 0.0f;
    float ic1 = 0.0f, ic2 = 0.0f;
};

// Linkwitz-Riley 4th-order low/high pair (two cascaded Butterworth SVFs each).
class LinkwitzRiley
{
public:
    void prepare (double sampleRate) noexcept { for (auto* f : { &a, &b, &c, &d }) f->prepare (sampleRate); }
    void reset() noexcept { for (auto* f : { &a, &b, &c, &d }) f->reset(); }
    void setCutoff (float hz) noexcept { for (auto* f : { &a, &b, &c, &d }) f->set (hz, 1.41421356f); }
    float lowpass (float x) noexcept  { return b.tick (a.tick (x).lp).lp; }
    float highpass (float x) noexcept { return d.tick (c.tick (x).hp).hp; }

private:
    Svf a, b, c, d;
};

// DC blocker (~8 Hz), used after asymmetric shaping and inside feedback loops.
class DcBlocker
{
public:
    void prepare (double sampleRate) noexcept { r = 1.0f - (kTwoPi * 8.0f / (float) sampleRate); }
    void reset() noexcept { x1 = y1 = 0.0f; }
    float process (float x) noexcept
    {
        const float y = x - x1 + r * y1;
        x1 = x;
        y1 = flushDenormal (y);
        return y;
    }

private:
    float r = 0.999f, x1 = 0.0f, y1 = 0.0f;
};

// ---------------------------------------------------------------------------
// Deterministic noise / random numbers (xorshift32). Real-time safe.
class Noise
{
public:
    explicit Noise (std::uint32_t seed = 0x9E3779B9u) : state (seed ? seed : 1u) {}
    void reseed (std::uint32_t seed) noexcept { state = seed ? seed : 1u; }
    std::uint32_t nextInt() noexcept
    {
        state ^= state << 13;
        state ^= state >> 17;
        state ^= state << 5;
        return state;
    }
    float next() noexcept    { return (float) nextInt() * (2.0f / 4294967295.0f) - 1.0f; }   // -1..+1
    float next01() noexcept  { return (float) nextInt() * (1.0f / 4294967295.0f); }          //  0..1

private:
    std::uint32_t state;
};

// Stateless hash (splitmix-style). Used for repeatable, seed-driven decisions
// such as "does step 37 glitch?": same seed + same step = same answer.
inline std::uint32_t hash32 (std::uint32_t a, std::uint32_t b) noexcept
{
    std::uint32_t x = a * 0x9E3779B1u ^ (b + 0x7F4A7C15u + (a << 6) + (a >> 2));
    x ^= x >> 16; x *= 0x85EBCA6Bu;
    x ^= x >> 13; x *= 0xC2B2AE35u;
    x ^= x >> 16;
    return x;
}
inline float hash01 (std::uint32_t a, std::uint32_t b) noexcept { return (float) (hash32 (a, b) >> 8) * (1.0f / 16777216.0f); }

// ---------------------------------------------------------------------------
// Soft saturator with slope <= 1 everywhere: safe inside feedback loops.
inline float saturate (float x, float drive) noexcept
{
    if (drive <= 1.0e-4f) return x;
    const float g = 1.0f + 2.0f * drive;
    return std::tanh (g * x) / g;
}

// Gentle limiter curve: linear below 0.7, then bends smoothly to a 1.0 ceiling.
inline float softLimit (float x) noexcept
{
    const float a = std::abs (x);
    if (a <= 0.7f) return x;
    const float over = (a - 0.7f) / 0.3f;
    const float y = 0.7f + 0.3f * std::tanh (over);
    return x < 0.0f ? -y : y;
}

// Smooth random "drift" source: glides between random targets. Output -1..1.
class Drift
{
public:
    explicit Drift (std::uint32_t seed = 0x51ED270Bu) : noise (seed) {}
    void prepare (double sampleRate) noexcept { fs = (float) sampleRate; glide.prepare (sampleRate, 100.0f); glide.snapTo (0.0f); }
    void reseed (std::uint32_t s) noexcept { noise.reseed (s); }
    void setRate (float hz) noexcept
    {
        rate = std::clamp (hz, 0.01f, 50.0f);
        glide.setTime (1000.0f / rate * 0.5f);
    }
    float next() noexcept
    {
        if (--countdown <= 0)
        {
            glide.setTarget (noise.next());
            countdown = std::max (1, (int) (fs / rate));
        }
        return glide.next();
    }

private:
    float fs = 48000.0f, rate = 1.0f;
    int countdown = 0;
    Smoother glide;
    Noise noise;
};

} // namespace vessel::dsp
