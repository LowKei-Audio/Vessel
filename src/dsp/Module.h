#pragma once
// Common interface for every Vessel stage. Each stage processes stereo audio
// in place and applies its own dry/wet Mix, so the chain only handles on/off.

#include "Primitives.h"

namespace vessel::dsp
{

// Musical time for the current block. ppq is the position (in quarter notes)
// of the block's first sample; it always advances, even when the host is
// stopped (the processor runs a free clock), so synced effects keep moving.
struct Transport
{
    double bpm = 120.0;
    double ppq = 0.0;
    bool playing = false;
    double sampleRate = 48000.0;

    double beatsPerSample() const noexcept { return bpm / (60.0 * sampleRate); }
};

// Tempo divisions shared by every synced control (length in quarter notes).
struct Division { const char* name; double beats; };
inline constexpr Division kDivisions[] = {
    { "4 Bars", 16.0 }, { "2 Bars", 8.0 }, { "1 Bar", 4.0 },  { "1/2D", 3.0 },
    { "1/2", 2.0 },     { "1/2T", 4.0 / 3.0 }, { "1/4D", 1.5 }, { "1/4", 1.0 },
    { "1/4T", 2.0 / 3.0 }, { "1/8D", 0.75 }, { "1/8", 0.5 },  { "1/8T", 1.0 / 3.0 },
    { "1/16D", 0.375 }, { "1/16", 0.25 },  { "1/16T", 1.0 / 6.0 }, { "1/32", 0.125 },
    { "1/64", 0.0625 },
};
inline constexpr int kNumDivisions = (int) (sizeof (kDivisions) / sizeof (kDivisions[0]));
inline double divisionBeats (int index) noexcept
{
    return kDivisions[std::clamp (index, 0, kNumDivisions - 1)].beats;
}
namespace Div { enum : int { bars4 = 0, bars2, bar1, d2D, d2, d2T, d4D, d4, d4T, d8D, d8, d8T, d16D, d16, d16T, d32, d64 }; }

class Module
{
public:
    virtual ~Module() = default;
    virtual void prepare (double sampleRate, int maxBlock) = 0;
    virtual void reset() = 0;
    virtual void process (float* L, float* R, int n, const Transport& t) noexcept = 0;
};

// Per-sample smoothed dry/wet helper used by most modules.
struct MixSmoother
{
    Smoother s;
    void prepare (double fs) { s.prepare (fs, 25.0f); }
    void set (float v, bool snap) { s.setTarget (clamp01 (v)); if (snap) s.snapTo (s.getTarget()); }
    float next() noexcept { return s.next(); }
};

} // namespace vessel::dsp
