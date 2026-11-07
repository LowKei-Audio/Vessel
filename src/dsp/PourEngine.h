#pragma once
// POUR: conditions what goes into the vessel.
//   Low Cut - keeps rumble from booming the low modes
//   Tilt    - dark .. bright (+/-6 dB shelf pair around 800 Hz)
//   Strike  - transient shaper: + sharpens attacks (more ping), - softens them
//   Drive   - warm saturation, auto-levelled so it adds colour, not volume

#include "Module.h"

namespace vessel::dsp
{

struct PourParams
{
    float drive = 0.0f;     // 0..1
    float strike = 0.0f;    // -1..1
    float tilt = 0.0f;      // -1..1
    float lowCutHz = 20.0f; // 20..1000
};

class PourEngine : public Module
{
public:
    void prepare (double sampleRate, int) override
    {
        fs = (float) sampleRate;
        for (auto& f : lowCut) f.prepare (sampleRate);
        for (auto& f : tiltSplit) { f.prepare (sampleRate); f.setCutoff (800.0f); }
        for (auto* s : { &driveS, &strikeS, &tiltS, &cutS }) s->prepare (sampleRate, 25.0f);
        aFast = std::exp (-1.0f / (0.001f * fs));
        rFast = std::exp (-1.0f / (0.030f * fs));
        aSlow = std::exp (-1.0f / (0.025f * fs));
        rSlow = std::exp (-1.0f / (0.120f * fs));
        primed = false;
        reset();
    }

    void reset() override
    {
        for (auto& f : lowCut) f.reset();
        for (auto& f : tiltSplit) f.reset();
        for (auto& e : fast) e = 0.0f;
        for (auto& e : slow) e = 0.0f;
    }

    void setParams (const PourParams& p) noexcept
    {
        driveS.setTarget (clamp01 (p.drive));
        strikeS.setTarget (std::clamp (p.strike, -1.0f, 1.0f));
        tiltS.setTarget (std::clamp (p.tilt, -1.0f, 1.0f));
        cutS.setTarget (std::clamp (p.lowCutHz, 20.0f, 1000.0f));
        if (! primed)
        {
            for (auto* s : { &driveS, &strikeS, &tiltS, &cutS }) s->snapTo (s->getTarget());
            primed = true;
        }
    }

    void process (float* L, float* R, int n, const Transport&) noexcept override
    {
        float* io[2] = { L, R };
        for (int i = 0; i < n; ++i)
        {
            const float drive = driveS.next(), strike = strikeS.next(), tilt = tiltS.next(), cut = cutS.next();
            if ((i & 7) == 0)
            {
                for (auto& f : lowCut) f.setCutoff (cut);
                driveNorm = std::pow (1.0f + 6.0f * drive, -0.85f);
            }

            // Transient gain from the gap between a fast and a slow envelope (linked).
            const float a = std::max (std::abs (io[0][i]), std::abs (io[1][i]));
            fast[0] = a > fast[0] ? a + aFast * (fast[0] - a) : a + rFast * (fast[0] - a);
            slow[0] = a > slow[0] ? a + aSlow * (slow[0] - a) : a + rSlow * (slow[0] - a);
            const float ratio = (fast[0] + 1.0e-5f) / (slow[0] + 1.0e-5f);   // > 1 on attacks
            const float shape = std::clamp (std::log2 (ratio), -1.0f, 2.0f);
            const float tGain = dbToGain (std::clamp (strike * shape * 9.0f, -12.0f, 12.0f));

            const float hiG = dbToGain (6.0f * tilt), loG = dbToGain (-6.0f * tilt);
            // tanh(g x) / g^0.85: small signals gain a little, loud ones are
            // held back, so typical music keeps roughly the same level.
            const float g = 1.0f + 6.0f * drive;
            const float norm = driveNorm;
            const float blend = std::min (1.0f, drive * 3.0f);

            for (int c = 0; c < 2; ++c)
            {
                float x = io[c][i];
                if (cut > 21.0f) x = lowCut[c].highpass (x);
                const float lo = tiltSplit[c].lowpass (x);
                x = lo * loG + (x - lo) * hiG;
                x *= tGain;
                if (drive > 1.0e-3f)
                {
                    x = lerp (x, std::tanh (g * x) * norm, blend);
                }
                io[c][i] = x;
            }
        }
    }

private:
    float fs = 48000.0f;
    OnePole lowCut[2], tiltSplit[2];
    Smoother driveS, strikeS, tiltS, cutS;
    float fast[2] {}, slow[2] {};
    float aFast = 0, rFast = 0, aSlow = 0, rSlow = 0, driveNorm = 1.0f;
    bool primed = false;
};

} // namespace vessel::dsp
