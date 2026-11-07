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
        for (auto& x : bassSplit) x.setCutoff (std::clamp (p.bassMonoHz, 20.0f, 500.0f));
        if (! primed)
        {
            for (auto* s : { &widthS, &mixS, &gainS, &deltaS }) s->snapTo (s->getTarget());
            primed = true;
        }
    }

    // Peak gain reduction of the last block (1 = none), for the UI meter.
    float gainReduction() const noexcept { return gr; }

    void process (const float* dryL, const float* dryR, float* L, float* R, int n) noexcept
    {
        const bool bassMono = params.bassMonoHz > 21.0f;
        float minGain = 1.0f;

        for (int i = 0; i < n; ++i)
        {
            float l = L[i], r = R[i];

            if (bassMono)
            {
                const float ll = bassSplit[0].lowpass (l), rl = bassSplit[1].lowpass (r);
                const float m = 0.5f * (ll + rl);
                l = l - ll + m;
                r = r - rl + m;
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
    bool primed = false;
};

} // namespace vessel::dsp
