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

    // During a sound change the input fades out and back in. Mode 1 (fading out, old
    // sound still heard): the slow envelope holds, so it does not fall with the fade.
    // Mode 2 (from the swap, output fully dry, through the fade-in): it may also rise
    // with the fast one. The returning input, at whatever level the new sound brings,
    // is then never mistaken for a new hit (that pinged the body on every preset
    // change, Nov 2026). Rising only after the swap keeps the old sound untouched.
    void setDetectorMode (int mode) noexcept { detectorMode = mode; }

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
            if (detectorMode == 0)
                slow[0] = a > slow[0] ? a + aSlow * (slow[0] - a) : a + rSlow * (slow[0] - a);
            else if (detectorMode == 2)
                slow[0] = std::max (slow[0], fast[0]);   // after the swap: may only rise (see setDetectorMode)
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
                // Low Cut fades out between 30 and 20 Hz rather than switching off at
                // one sample (a hard bypass at 21 Hz clicked whenever the cut was swept
                // down to off, by hand or by a preset, Nov 2026). The filter keeps running
                // so its state is ready; at 20 Hz (off) the signal passes bit-exact.
                const float hp = lowCut[c].highpass (x);
                if (cut > 20.0f) x += std::min (1.0f, (cut - 20.0f) * 0.1f) * (hp - x);
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
    int detectorMode = 0;
};

} // namespace vessel::dsp
