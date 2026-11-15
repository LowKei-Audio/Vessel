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
        tapFade = 1.0f;
        routeFade = 1.0f;
        tapFadeStep = 1.0f / (0.010f * fs);
        jumpPending = false;
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
        if (primed && p.pingPong != params.pingPong)
        {
            // Ping-pong switched (by hand or by a preset): what is written into each line
            // changes in one sample, and that step replays one echo later as a click
            // (Echo Body Loop -> Tribal Clay, Nov 2026). Crossfade the routing over 10 ms.
            routeFromPing = params.pingPong;
            routeFade = 0.0f;
        }
        params = p;
        timeS.setTarget (std::clamp (p.timeMs, 10.0f, 2000.0f) * 0.001f * fs);
        fbS.setTarget (std::clamp (p.feedback, 0.0f, 0.95f));
        wobS.setTarget (clamp01 (p.wobble));
        for (auto& f : tone) f.setCutoff (std::clamp (p.toneHz, 500.0f, 20000.0f));
        mixS.set (p.mix, ! primed);
        if (! primed) { timeS.snapTo (timeS.getTarget()); fbS.snapTo (fbS.getTarget()); wobS.snapTo (wobS.getTarget()); primed = true; }
        if (jumpPending)
        {
            jumpPending = false;
            if (std::abs (timeS.getTarget() - timeS.getCurrent()) > 1.0f)
            {
                oldTime = timeS.getCurrent();
                timeS.snapTo (timeS.getTarget());
                tapFade = 0.0f;   // crossfade from the old tap to the new one
            }
        }
    }

    void process (float* L, float* R, int n, const Transport&) noexcept override
    {
        float* io[2] = { L, R };
        for (int i = 0; i < n; ++i)
        {
            const float wob = wobS.next();
            const float t = timeS.next() * (1.0f + 0.012f * wob * wow.next());
            const float fb = fbS.next(), mix = mixS.next();
            float yl = lines[0].read (t), yr = lines[1].read (t);
            if (tapFade < 1.0f)
            {
                // Old and new taps crossfaded (S-curve, 10 ms): what is heard and what is
                // fed back stay continuous, so no step is written into the line to be
                // replayed one echo later.
                tapFade = std::min (1.0f, tapFade + tapFadeStep);
                const float g = tapFade * tapFade * (3.0f - 2.0f * tapFade);
                const float to = oldTime * (t / std::max (1.0f, timeS.getCurrent()));   // same wobble
                const float ol = lines[0].read (to), orr = lines[1].read (to);
                yl = ol + g * (yl - ol);
                yr = orr + g * (yr - orr);
            }
            const float xl = io[0][i], xr = io[1][i];
            float fl = lowCut[0].highpass (tone[0].lowpass (yl)) * fb;
            float fr = lowCut[1].highpass (tone[1].lowpass (yr)) * fb;
            const float pingL = 0.5f * (xl + xr) + fr, pingR = fl;   // ping-pong routing
            const float strL = xl + fl, strR = xr + fr;                  // straight routing
            float wl = params.pingPong ? pingL : strL, wr = params.pingPong ? pingR : strR;
            if (routeFade < 1.0f)
            {
                routeFade = std::min (1.0f, routeFade + tapFadeStep);
                const float g = routeFade * routeFade * (3.0f - 2.0f * routeFade);
                const float fromL = routeFromPing ? pingL : strL, fromR = routeFromPing ? pingR : strR;
                wl = fromL + g * (wl - fromL);
                wr = fromR + g * (wr - fromR);
            }
            lines[0].push (saturate (wl, 0.3f));
            lines[1].push (saturate (wr, 0.3f));
            if (i < kRetMax) { ret[0][(size_t) i] = yl; ret[1][(size_t) i] = yr; }
            // Dry stays (dipping a little only near full mix), echoes are added on top.
            const float dry = 1.0f - 0.3f * mix * mix;
            io[0][i] = xl * dry + mix * yl;
            io[1][i] = xr * dry + mix * yr;
        }
    }

    // A new sound is being swapped in while the output is fully dry. Time normally glides
    // (tape style): on a preset change that swept the read point through the old sound's
    // echoes, heard as a pitch-swept buzz (Nov 2026). The next setParams jumps to the new
    // time instead, crossfading from the old tap to the new one over 10 ms.
    void jumpOnNextParams() noexcept { jumpPending = true; }

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
    float tapFade = 1.0f, tapFadeStep = 1.0f / 480.0f, oldTime = 0.0f, routeFade = 1.0f;
    bool routeFromPing = false;
    bool primed = false, jumpPending = false;
};

} // namespace vessel::dsp
