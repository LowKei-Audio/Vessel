#pragma once
// ECHO: a tape-flavoured stereo delay around the vessel.
//   Feedback runs through a darkening Tone filter, a fixed 80 Hz low cut and
//   a soft saturator (safe at any setting). Ping-Pong crosses the repeats.
//   Wobble adds tape wow. Return sends the echoes back INTO the Body, so each
//   repeat rings the vessel again (the chain reads returns()).

#include "DelayLine.h"
#include "Module.h"

#include <array>

namespace vessel::dsp
{

struct EchoParams
{
    float timeMs = 375.0f;    // 10..2000 (synced times are converted by the caller)
    float feedback = 0.35f;   // 0..0.95
    bool pingPong = false;
    float toneHz = 6000.0f;   // 500..20000
    float wobble = 0.0f;      // 0..1
    float mix = 0.25f;
};

class EchoEngine : public Module
{
public:
    void prepare (double sampleRate, int) override
    {
        fs = (float) sampleRate;
        for (auto& d : lines) d.prepare ((std::size_t) (2.1f * fs) + 64);
        for (auto& f : tone) f.prepare (sampleRate);
        for (auto& f : lowCut) { f.prepare (sampleRate); f.setCutoff (80.0f); }
        timeS.prepare (sampleRate, 120.0f);
        fbS.prepare (sampleRate, 25.0f);
        wobS.prepare (sampleRate, 25.0f);
        mixS.prepare (sampleRate);
        wow.prepare (sampleRate);
        wow.setRate (0.8f);
        primed = false;
        reset();
    }

    void reset() override
    {
        for (auto& d : lines) d.reset();
        for (auto& f : tone) f.reset();
        for (auto& f : lowCut) f.reset();
        ret[0].fill (0.0f);
        ret[1].fill (0.0f);
    }

    void setParams (const EchoParams& p) noexcept
    {
        params = p;
        timeS.setTarget (std::clamp (p.timeMs, 10.0f, 2000.0f) * 0.001f * fs);
        fbS.setTarget (std::clamp (p.feedback, 0.0f, 0.95f));
        wobS.setTarget (clamp01 (p.wobble));
        for (auto& f : tone) f.setCutoff (std::clamp (p.toneHz, 500.0f, 20000.0f));
        mixS.set (p.mix, ! primed);
        if (! primed) { timeS.snapTo (timeS.getTarget()); fbS.snapTo (fbS.getTarget()); wobS.snapTo (wobS.getTarget()); primed = true; }
    }

    void process (float* L, float* R, int n, const Transport&) noexcept override
    {
        float* io[2] = { L, R };
        for (int i = 0; i < n; ++i)
        {
            const float wob = wobS.next();
            const float t = timeS.next() * (1.0f + 0.012f * wob * wow.next());
            const float fb = fbS.next(), mix = mixS.next();
            const float yl = lines[0].read (t), yr = lines[1].read (t);
            const float xl = io[0][i], xr = io[1][i];
            float fl = lowCut[0].highpass (tone[0].lowpass (yl)) * fb;
            float fr = lowCut[1].highpass (tone[1].lowpass (yr)) * fb;
            if (params.pingPong)
            {
                lines[0].push (saturate (0.5f * (xl + xr) + fr, 0.3f));
                lines[1].push (saturate (fl, 0.3f));
            }
            else
            {
                lines[0].push (saturate (xl + fl, 0.3f));
                lines[1].push (saturate (xr + fr, 0.3f));
            }
            if (i < kRetMax) { ret[0][(size_t) i] = yl; ret[1][(size_t) i] = yr; }
            // Dry stays (dipping a little only near full mix), echoes are added on top.
            const float dry = 1.0f - 0.3f * mix * mix;
            io[0][i] = xl * dry + mix * yl;
            io[1][i] = xr * dry + mix * yr;
        }
    }

    // The wet echoes of the last processed block (the Body re-excitation path).
    static constexpr int kRetMax = 64;
    const float* returns (int c) const noexcept { return ret[c & 1].data(); }

private:
    float fs = 48000.0f;
    EchoParams params;
    DelayLine lines[2];
    OnePole tone[2], lowCut[2];
    Smoother timeS, fbS, wobS;
    MixSmoother mixS;
    Drift wow { 0x7A9Eu };
    std::array<float, kRetMax> ret[2] {};
    bool primed = false;
};

} // namespace vessel::dsp
