#pragma once
// Modulation sources for the Vessel matrix. Evaluated once per control
// block (32 samples, ~0.7 ms at 48 kHz); every destination is smoothed per
// sample inside its engine, so block-rate modulation never zippers.
//
//   ModLfo     - 7 shapes incl. sample & hold and smooth random; free (Hz)
//                or locked to the host grid. Random shapes are seeded per
//                cycle, so a synced pattern repeats identically on every pass.
//   EnvFollower- input loudness -> 0..1
//   Chaos      - random drift: stepped (S&H) through to smooth gliding

#include "Module.h"

#include <array>
#include <cstdint>

namespace vessel::dsp
{

enum class LfoShape { sine = 0, triangle, sawUp, sawDown, square, sampleHold, smoothRandom };

struct LfoParams
{
    LfoShape shape = LfoShape::sine;
    float rateHz = 1.0f;
    bool sync = false;
    int division = Div::d4;
    float phase = 0.0f;   // 0..1 offset
};

class ModLfo
{
public:
    void setIndex (int i) noexcept { index = (std::uint32_t) i; }
    void reset() noexcept { freePhase = 0.0; value = 0.0f; }

    // Advance by `samples` and return the value (-1..1) for this block.
    float advance (const LfoParams& p, const Transport& t, int samples, std::uint32_t seed) noexcept
    {
        double pos;   // position in cycles
        if (p.sync)
        {
            pos = t.ppq / divisionBeats (p.division);
        }
        else
        {
            freePhase += (double) std::clamp (p.rateHz, 0.001f, 100.0f) * samples / t.sampleRate;
            pos = freePhase;
        }
        pos += p.phase;
        const auto cycle = (std::int64_t) std::floor (pos);
        const float ph = (float) (pos - std::floor (pos));

        switch (p.shape)
        {
            case LfoShape::sine:     value = std::sin (kTwoPi * ph); break;
            case LfoShape::triangle: value = 1.0f - 4.0f * std::abs (ph - 0.5f); break;
            case LfoShape::sawUp:    value = 2.0f * ph - 1.0f; break;
            case LfoShape::sawDown:  value = 1.0f - 2.0f * ph; break;
            case LfoShape::square:   value = ph < 0.5f ? 1.0f : -1.0f; break;
            case LfoShape::sampleHold:
                value = 2.0f * hash01 (seed + index * 7919u, (std::uint32_t) cycle) - 1.0f;
                break;
            case LfoShape::smoothRandom:
            {
                const float a = 2.0f * hash01 (seed + index * 7919u, (std::uint32_t) cycle) - 1.0f;
                const float b = 2.0f * hash01 (seed + index * 7919u, (std::uint32_t) (cycle + 1)) - 1.0f;
                const float s = 0.5f - 0.5f * std::cos (kPi * ph);
                value = a + s * (b - a);
                break;
            }
        }
        return value;
    }

    float current() const noexcept { return value; }

private:
    std::uint32_t index = 0;
    double freePhase = 0.0;
    float value = 0.0f;
};

struct EnvParams
{
    float attackMs = 10.0f;
    float releaseMs = 200.0f;
    float gainDb = 0.0f;   // sensitivity
};

class EnvFollower
{
public:
    void reset() noexcept { env = 0.0f; }

    // peak = the block's input peak level.
    float advance (const EnvParams& p, float peak, int samples, double fs) noexcept
    {
        const float timeMs = peak > env ? p.attackMs : p.releaseMs;
        const float coeff = std::exp (-(float) samples / (std::max (0.05f, timeMs) * 0.001f * (float) fs));
        env = peak + coeff * (env - peak);
        return value = clamp01 (env * dbToGain (p.gainDb) * 1.4f);
    }

    float current() const noexcept { return value; }

private:
    float env = 0.0f, value = 0.0f;
};

struct ChaosParams
{
    float rateHz = 1.0f;
    float smooth = 0.5f;   // 0 = stepped, 1 = gliding
};

class Chaos
{
public:
    void reset() noexcept { phase = 0.0; a = b = 0.0f; counter = 0; }

    float advance (const ChaosParams& p, int samples, double fs, std::uint32_t seed) noexcept
    {
        phase += (double) std::clamp (p.rateHz, 0.01f, 50.0f) * samples / fs;
        while (phase >= 1.0)
        {
            phase -= 1.0;
            a = b;
            b = 2.0f * hash01 (seed ^ 0xC4A05u, counter++) - 1.0f;
        }
        const float ph = (float) phase;
        const float glide = std::clamp (ph / std::max (0.001f, p.smooth), 0.0f, 1.0f);
        const float s = 0.5f - 0.5f * std::cos (kPi * glide);
        return value = a + s * (b - a);
    }

    float current() const noexcept { return value; }

private:
    double phase = 0.0;
    float a = 0.0f, b = 0.0f, value = 0.0f;
    std::uint32_t counter = 0;
};

} // namespace vessel::dsp
