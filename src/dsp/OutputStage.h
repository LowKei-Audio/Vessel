#pragma once
// Final stage, after Space:
//
//   wet -> Bass Mono -> Width (M/S)
//   out = dry + Mix * (wet - dry)       (or Delta: only what Vessel adds)
//   -> Output gain -> Safety limiter (instant attack, -0.3 dBFS ceiling)

#include "Module.h"

namespace vessel::dsp
{

struct OutputParams
{
    float width = 1.0f;        // 0..2
    float bassMonoHz = 20.0f;  // 20 = off .. 500
    float mix = 1.0f;
    bool delta = false;        // listen to the difference only
    float outGain = 1.0f;      // linear
    bool limiter = true;
};

class OutputStage
{
public:
    static constexpr float kCeiling = 0.9661f;   // -0.3 dBFS

    void prepare (double sampleRate)
    {
        fs = (float) sampleRate;
        for (auto& x : bassSplit) x.prepare (sampleRate);
        for (auto* s : { &widthS, &mixS, &gainS, &deltaS })
            s->prepare (sampleRate, 25.0f);
        release = std::exp (-1.0f / (0.12f * fs));
        monoStep = 1.0f / (0.030f * fs);
        primed = false;
        reset();
    }

    void reset()
    {
        for (auto& x : bassSplit) x.reset();
        env = 0.0f;
        gr = 1.0f;
    }

    void setParams (const OutputParams& p) noexcept
    {
        params = p;
        widthS.setTarget (std::clamp (p.width, 0.0f, 2.0f));
        mixS.setTarget (clamp01 (p.mix));
        gainS.setTarget (p.outGain);
        deltaS.setTarget (p.delta ? 1.0f : 0.0f);
        // Bass Mono fades over 20-30 Hz, along a 30 ms S-curve, rather than switching at
        // one sample: switching it off dropped the low end in one step (a loud click
        // whenever it was turned off while playing, Nov 2026). At 20 Hz (off) the signal
        // passes unchanged.
        monoTarget = std::clamp ((p.bassMonoHz - 20.0f) * 0.1f, 0.0f, 1.0f);
        // Switched off, the split keeps its last frequency while Bass Mono fades out
        // (a jump to 20 Hz changed the split's output in one sample).
        if (p.bassMonoHz > 20.0f || ! primed)
            for (auto& x : bassSplit) x.setCutoff (std::clamp (p.bassMonoHz, 20.0f, 500.0f));
        if (! primed)
        {
            for (auto* s : { &widthS, &mixS, &gainS, &deltaS }) s->snapTo (s->getTarget());
            monoLin = monoTarget;
            primed = true;
        }
    }

    // The next setParams jumps every smoother to its target (used while the output is
    // fully crossfaded to dry during a sound change, so the jump is never heard).
    void snapOnNextParams() noexcept { primed = false; }

    // Peak gain reduction of the last block (1 = none), for the UI meter.
    float gainReduction() const noexcept { return gr; }

    void process (const float* dryL, const float* dryR, float* L, float* R, int n) noexcept
    {
        float minGain = 1.0f;

        for (int i = 0; i < n; ++i)
        {
            float l = L[i], r = R[i];

            // The split runs all the time so it is ready when Bass Mono fades in.
            const float ll = bassSplit[0].lowpass (l), rl = bassSplit[1].lowpass (r);
            if (monoLin != monoTarget)
                monoLin = monoTarget > monoLin ? std::min (monoTarget, monoLin + monoStep)
                                               : std::max (monoTarget, monoLin - monoStep);
            if (monoLin > 0.0f)
            {
                const float mono = monoLin * monoLin * (3.0f - 2.0f * monoLin);   // S-curve
                const float m = 0.5f * (ll + rl);
                l += mono * (m - ll);
                r += mono * (m - rl);
            }

            const float w = widthS.next();
            const float mid = 0.5f * (l + r), side = 0.5f * (l - r) * w;
            l = mid + side;
            r = mid - side;

            const float mix = mixS.next(), d = deltaS.next(), g = gainS.next();
            const float outL = dryL[i] + mix * (l - dryL[i]);
            const float outR = dryR[i] + mix * (r - dryR[i]);
            const float deltaL = mix * (l - dryL[i]);
            const float deltaR = mix * (r - dryR[i]);
            float yl = g * lerp (outL, deltaL, d);
            float yr = g * lerp (outR, deltaR, d);

            if (params.limiter)
            {
                const float peak = std::max (std::abs (yl), std::abs (yr));
                env = std::max (peak, env * release);
                const float gain = env > kCeiling ? kCeiling / env : 1.0f;
                minGain = std::min (minGain, gain);
                yl = std::clamp (yl * gain, -kCeiling, kCeiling);
                yr = std::clamp (yr * gain, -kCeiling, kCeiling);
            }
            L[i] = std::isfinite (yl) ? yl : 0.0f;
            R[i] = std::isfinite (yr) ? yr : 0.0f;
        }
        gr = minGain;
    }

private:
    float fs = 48000.0f, release = 0.999f, env = 0.0f, gr = 1.0f;
    OutputParams params;
    LinkwitzRiley bassSplit[2];
    Smoother widthS, mixS, gainS, deltaS;
    float monoLin = 0.0f, monoTarget = 0.0f, monoStep = 1.0f / 1440.0f;
    bool primed = false;
};

} // namespace vessel::dsp
