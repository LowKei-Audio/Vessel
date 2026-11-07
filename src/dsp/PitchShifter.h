#pragma once
// Delay-line pitch shifter: two read heads sweep through a short window in
// opposite half-cycles, each faded with a Hann window, so the crossfades sum
// to unity gain. Ratio 2 = octave up, 0.5 = octave down. Mono; use one per
// channel. Works on any material, sounds best on sustained tones.

#include "DelayLine.h"
#include "Primitives.h"

namespace vessel::dsp
{

class PitchShifter
{
public:
    static constexpr float kMaxWindowMs = 160.0f;

    void prepare (double sampleRate)
    {
        fs = (float) sampleRate;
        line.prepare ((std::size_t) (kMaxWindowMs * 2.0f * 0.001f * fs) + 16);
        windowS.prepare (sampleRate, 50.0f);
        windowS.snapTo (60.0f * 0.001f * fs);
        reset();
    }

    void reset()
    {
        line.reset();
        phase = 0.0f;
    }

    void setWindowMs (float ms) noexcept { windowS.setTarget (std::clamp (ms, 8.0f, kMaxWindowMs) * 0.001f * fs); }
    void setRatio (float r) noexcept     { ratio = std::clamp (r, 0.125f, 4.0f); }
    void setPhase (float p) noexcept     { phase = p - std::floor (p); }

    float process (float x) noexcept
    {
        line.push (x);
        const float w = windowS.next();

        // The read point moves at (1 - ratio) samples per sample relative to
        // the write head; phase covers the window once per cycle.
        phase += (1.0f - ratio) / w;
        phase -= std::floor (phase);

        float p2 = phase + 0.5f;
        p2 -= std::floor (p2);

        const float a = line.read (2.0f + phase * w) * hann (phase);
        const float b = line.read (2.0f + p2 * w) * hann (p2);
        return a + b;
    }

private:
    float fs = 48000.0f, ratio = 1.0f, phase = 0.0f;
    DelayLine line;
    Smoother windowS;
};

// 90-degree phase splitter (Olli Niemitalo's 4+4 all-pass design), used for
// single-sideband frequency shifting. Outputs an in-phase / quadrature pair.
class Hilbert
{
public:
    void reset() { for (auto& s : sa) s = {}; for (auto& s : sb) s = {}; delayed = 0.0f; }

    void process (float x, float& re, float& im) noexcept
    {
        static constexpr float ca[4] = { 0.6923878f, 0.9360654322959f, 0.9882295226860f, 0.9987488452737f };
        static constexpr float cb[4] = { 0.4021921162426f, 0.8561710882420f, 0.9722909545651f, 0.9952884791278f };
        float a = x, b = x;
        for (int i = 0; i < 4; ++i) a = sa[i].tick (a, ca[i] * ca[i]);
        for (int i = 0; i < 4; ++i) b = sb[i].tick (b, cb[i] * cb[i]);
        re = delayed;   // path A is delayed one sample to line up with path B
        delayed = a;
        im = b;
    }

private:
    // y[n] = c * (x[n] + y[n-2]) - x[n-2]
    struct Stage
    {
        float x1 = 0, x2 = 0, y1 = 0, y2 = 0;
        float tick (float x, float c) noexcept
        {
            const float y = c * (x + y2) - x2;
            x2 = x1; x1 = x;
            y2 = y1; y1 = flushDenormal (y);
            return y;
        }
    };
    Stage sa[4], sb[4];
    float delayed = 0.0f;
};

} // namespace vessel::dsp
