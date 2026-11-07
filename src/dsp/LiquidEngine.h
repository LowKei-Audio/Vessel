#pragma once
// LIQUID: the water inside the vessel moving.
//   The Liquid clock (free Hz or synced) drives two things:
//   - Slosh: every Body mode wobbles in pitch on its own phase (in BodyEngine)
//   - Ripple: a two-tap liquid chorus here, its sweep shaped like a wave
//     (fundamental + a third harmonic), the two sides moving in opposition
//   Turbulence swaps the smooth wave for random currents.

#include "DelayLine.h"
#include "Module.h"

namespace vessel::dsp
{

struct LiquidParams
{
    float ripple = 0.3f;      // 0..1 depth
    float turbulence = 0.0f;  // 0..1
    float mix = 0.5f;
    float phase = 0.0f;       // clock phase at block start, 0..1
    float phaseInc = 0.0f;    // per sample
    std::uint32_t seed = 1;
};

class LiquidEngine : public Module
{
public:
    void prepare (double sampleRate, int) override
    {
        fs = (float) sampleRate;
        for (auto& d : lines) d.prepare ((std::size_t) (0.03f * fs) + 8);
        for (auto* s : { &rippleS, &turbS }) s->prepare (sampleRate, 40.0f);
        for (int k = 0; k < 4; ++k) { currents[k].prepare (sampleRate); currents[k].setRate (0.7f + 0.37f * (float) k); }
        mixS.prepare (sampleRate);
        primed = false;
        reset();
    }

    void reset() override { for (auto& d : lines) d.reset(); }

    void setParams (const LiquidParams& p) noexcept
    {
        params = p;
        rippleS.setTarget (clamp01 (p.ripple));
        turbS.setTarget (clamp01 (p.turbulence));
        mixS.set (p.mix, ! primed);
        if (p.seed != seed) { seed = p.seed; for (int k = 0; k < 4; ++k) currents[k].reseed (p.seed * 31u + (std::uint32_t) k); }
        if (! primed) { rippleS.snapTo (rippleS.getTarget()); turbS.snapTo (turbS.getTarget()); primed = true; }
    }

    void process (float* L, float* R, int n, const Transport&) noexcept override
    {
        float* io[2] = { L, R };
        float ph = params.phase;
        const float base = 0.007f * fs;
        for (int i = 0; i < n; ++i)
        {
            const float depth = rippleS.next() * 0.0045f * fs, turb = turbS.next(), mix = mixS.next();
            ph += params.phaseInc;
            ph -= std::floor (ph);
            float wet[2];
            for (int c = 0; c < 2; ++c)
            {
                lines[c].push (io[c][i]);
                float acc = 0.0f;
                for (int t = 0; t < 2; ++t)
                {
                    const float off = 0.5f * (float) c + 0.25f * (float) t;
                    const float p = ph + off;
                    const float wave = 0.8f * std::sin (kTwoPi * p) + 0.2f * std::sin (3.0f * kTwoPi * p);
                    const float m = lerp (wave, currents[c * 2 + t].next(), turb);
                    acc += lines[c].read (base * (1.0f + 0.3f * (float) t) + depth * m);
                }
                wet[c] = 0.7071f * acc;   // two decorrelated taps: equal-power sum
            }
            if (mix != lastMix) { lastMix = mix; dryG = std::cos (0.5f * kPi * mix); wetG = std::sin (0.5f * kPi * mix); }
            for (int c = 0; c < 2; ++c) io[c][i] = io[c][i] * dryG + wet[c] * wetG;
        }
    }

private:
    float fs = 48000.0f;
    LiquidParams params;
    DelayLine lines[2];
    Drift currents[4];
    Smoother rippleS, turbS;
    MixSmoother mixS;
    std::uint32_t seed = 0;
    float lastMix = -1.0f, dryG = 1.0f, wetG = 0.0f;
    bool primed = false;
};

} // namespace vessel::dsp
