// Static / click regression tests (Serato report, Nov 2026). Each one isolates a
// cause found by `vessel_check static` and fails on Vessel 1.0.1. The whole-plugin
// checks (preset switches, loops, Seal/Shatter) live in vessel_check static.

#include "TestUtil.h"
#include "dsp/Chain.h"
#include "dsp/EchoEngine.h"
#include "dsp/OutputStage.h"
#include "dsp/PourEngine.h"
#include "dsp/ReverbEngine.h"

#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <cmath>
#include <functional>

using namespace vessel::dsp;
using namespace testutil;

namespace
{
// Sharpest sample-to-sample bend (|second difference|) from `from` on: a click is a
// bend far above what the sound does on its own.
float maxBend (const std::vector<float>& x, size_t from = 2, size_t to = 0)
{
    to = to == 0 ? x.size() : std::min (to, x.size());
    float m = 0.0f;
    for (size_t i = std::max<size_t> (from, 2); i < to; ++i)
        m = std::max (m, std::abs (x[i] - 2.0f * x[i - 1] + x[i - 2]));
    return m;
}

struct Setup
{
    ChainParams chain;
    BodyParams body;
    FractureParams fracture;
    LiquidParams liquid;
    EchoParams echo;
    ReverbParams space;
    PourParams pour;
};

// Renders like the processor: params every 32-sample control block.
void renderChain (Chain& c, Setup& s, Stereo& y, Transport& t, std::function<void (size_t)> perBlock = {})
{
    for (size_t i = 0; i < y.size(); i += 32)
    {
        const int n = (int) std::min<size_t> (32, y.size() - i);
        if (perBlock) perBlock (i);
        c.setParams (s.chain);
        float ph = 0.0f, inc = 0.0f;
        c.clock (t, n, ph, inc);
        s.body.sloshPhase = ph;
        s.liquid.phase = ph;
        s.liquid.phaseInc = inc;
        c.pourE.setParams (s.pour);
        c.bodyE.setParams (s.body, n);
        c.fractureE.setParams (s.fracture);
        c.liquidE.setParams (s.liquid);
        c.echoE.setParams (s.echo);
        c.spaceE.setParams (s.space);
        c.process (&y.l[i], &y.r[i], n, t);
        t.ppq += t.beatsPerSample() * n;
    }
}

Transport transport()
{
    Transport t;
    t.sampleRate = kFs;
    t.bpm = 120.0;
    return t;
}

// A long-ringing Body on a held tone, with one other stage switched on halfway.
float stageWakeRatio (int stage, Setup s)
{
    Chain c;
    c.prepare (kFs, 32);
    s.chain.on = { false, true, false, false, false, false };
    s.body.mix = 1.0f;
    s.body.decaySec = 4.0f;
    auto y = sine (233.08, 2.0, 0.3f);
    auto t = transport();
    const size_t at = (size_t) kFs;
    renderChain (c, s, y, t, [&] (size_t i) { if (i == at) s.chain.on[(size_t) stage] = true; });
    REQUIRE (allFinite (y));
    const float before = maxBend (y.l, at / 2, at);
    const float after = maxBend (y.l, at + (size_t) (0.3 * kFs));
    const float sw = maxBend (y.l, at, at + (size_t) (0.3 * kFs));
    std::printf ("  stage %d on: switch %.5f, sound before %.5f, after %.5f\n", stage, sw, before, after);
    return sw / std::max (before, after);
}
} // namespace

// Wine Glass (Nov 2026): a waking stage started with empty delay lines and was fed
// the ringing body at full level, so the chorus tap read silence, then the signal
// in one step. Its input now fades in with its output.
TEST_CASE ("Static: switching Liquid on over a ringing body is click-free")
{
    Setup s;
    s.liquid.mix = 1.0f;
    s.liquid.ripple = 1.0f;
    REQUIRE (stageWakeRatio (liquid, s) < 1.25f);
}

TEST_CASE ("Static: switching Echo on over a ringing body is click-free")
{
    Setup s;
    s.echo.mix = 0.8f;
    s.echo.timeMs = 15.0f;
    s.echo.feedback = 0.5f;
    REQUIRE (stageWakeRatio (echo, s) < 1.25f);
}

// Marimba Body (Nov 2026): Decay changed the reverb's feedback gains in one step
// while its tail rang. It is smoothed now.
TEST_CASE ("Static: a Space Decay change on a ringing tail is click-free")
{
    ReverbEngine r;
    r.prepare (kFs, 32);
    ReverbParams p;
    p.mix = 1.0f;
    p.decaySec = 1.2f;
    auto y = withSilence (sine (233.08, 0.5, 0.3f), 1.5);
    auto t = transport();
    const size_t at = (size_t) (0.8 * kFs);
    for (size_t i = 0; i < y.size(); i += 32)
    {
        if (i == at) p.decaySec = 6.0f;
        r.setParams (p);
        r.process (&y.l[i], &y.r[i], (int) std::min<size_t> (32, y.size() - i), t);
    }
    REQUIRE (allFinite (y));
    // The tail alone, well after the tone stopped at 0.5 s (its own cut-off is a bend).
    const float before = maxBend (y.l, (size_t) (0.7 * kFs), at);
    const float sw = maxBend (y.l, at, at + (size_t) (0.25 * kFs));
    std::printf ("  decay change: switch %.6f, tail before %.6f\n", sw, before);
    REQUIRE (sw < 1.25f * before);
}

// Underwater (Nov 2026): Liquid sync switched off jumped the slosh from the grid
// to an old free-running phase. The clock now carries on from where it was heading,
// both ways and on a new division.
TEST_CASE ("Static: the slosh clock is continuous across sync and division changes")
{
    Chain c;
    c.prepare (kFs, 32);
    ChainParams cp;
    cp.liquidSync = true;
    cp.liquidDivision = 9;
    cp.liquidRateHz = 0.7f;
    auto t = transport();
    t.playing = true;
    t.ppq = 3.3;
    float last = 0.0f, lastInc = 0.0f, worst = 0.0f;
    for (int b = 0; b < 4000; ++b)
    {
        if (b == 1000) cp.liquidSync = false;
        if (b == 2000) cp.liquidSync = true;
        if (b == 3000) cp.liquidDivision = 4;
        c.setParams (cp);
        float ph = 0.0f, inc = 0.0f;
        c.clock (t, 32, ph, inc);
        if (b > 0)
        {
            float d = ph - (last + 32.0f * lastInc);
            d -= std::round (d);
            worst = std::max (worst, std::abs (d));
        }
        last = ph;
        lastInc = inc;
        t.ppq += t.beatsPerSample() * 32;
    }
    std::printf ("  slosh clock: largest phase jump %.6f cycles\n", worst);
    REQUIRE (worst < 0.01f);
}

// Sloshing Bowl (Nov 2026): a loop wrap moved the synced slosh in one sample.
// PhaseGlide eases a jump out, keeps the position continuous inside a block, and
// is an exact pass-through when nothing jumped (playback still matches a bounce).
TEST_CASE ("Static: PhaseGlide eases a playhead jump and is exact otherwise")
{
    PhaseGlide g;
    const double inc = 1.0 / 24000.0;
    double pos = 0.0, incOut = 0.0, prevEnd = 0.0;
    bool first = true;
    for (int b = 0; b < 400; ++b)
    {
        if (b == 200) { pos -= 0.37; g.arm(); }
        const double out = g.process (pos, inc, 32, kFs, incOut);
        if (! first)
        {
            double d = out - prevEnd;
            d -= std::round (d);
            REQUIRE (std::abs (d) < 1.0e-9);   // continuous at every block edge
        }
        if (b < 200) REQUIRE (out == pos);     // exact until something jumps
        if (b == 399) REQUIRE (std::abs (out - pos) < 1.0e-3);   // glided out (133 ms later)
        first = false;
        prevEnd = out + incOut * 32;
        pos += inc * 32;
    }
}

// Pour Me 4 (Nov 2026): on a preset change, Echo's time glided (tape style) and swept
// its read point through the old sound's echoes. At a sound change it now jumps, and
// the echoes fade back in so the jump is never fed back into the line.
TEST_CASE ("Static: Echo jumps to a new time at a sound change without a step")
{
    EchoEngine e;
    e.prepare (kFs, 32);
    EchoParams p;
    p.mix = 1.0f;
    p.feedback = 0.6f;
    p.timeMs = 125.0f;
    auto y = sine (233.08, 2.0, 0.3f);
    auto t = transport();
    const size_t at = (size_t) kFs;
    for (size_t i = 0; i < y.size(); i += 32)
    {
        if (i == at) { p.timeMs = 500.0f; e.jumpOnNextParams(); }
        e.setParams (p);
        e.process (&y.l[i], &y.r[i], (int) std::min<size_t> (32, y.size() - i), t);
    }
    REQUIRE (allFinite (y));
    const float before = maxBend (y.l, at / 2, at);
    // The jump itself is covered by the processor's crossfade to dry; what must not
    // happen is a step fed back into the line and replayed one echo later.
    const float replay = maxBend (y.l, at + 4800, at + (size_t) (0.75 * kFs));
    std::printf ("  echo jump: after the jump %.5f, before %.5f\n", replay, before);
    REQUIRE (replay < 1.25f * before);
}

// Echo Body Loop -> Tribal Clay (Nov 2026): switching ping-pong changed what is
// written into each line in one sample, replayed one echo later. The routing now
// crossfades, whether a preset or a hand flips it.
TEST_CASE ("Static: switching Echo ping-pong does not write a step into the lines")
{
    EchoEngine e;
    e.prepare (kFs, 32);
    EchoParams p;
    p.mix = 1.0f;
    p.feedback = 0.6f;
    p.timeMs = 140.0f;
    p.pingPong = true;
    auto y = sine (233.08, 2.0, 0.3f);
    for (size_t i = 0; i < y.size(); ++i) y.r[i] *= 0.3f;   // stereo, so the routings differ
    auto t = transport();
    const size_t at = (size_t) kFs;
    for (size_t i = 0; i < y.size(); i += 32)
    {
        if (i == at) p.pingPong = false;
        e.setParams (p);
        e.process (&y.l[i], &y.r[i], (int) std::min<size_t> (32, y.size() - i), t);
    }
    REQUIRE (allFinite (y));
    const float before = maxBend (y.l, at / 2, at);
    const float after = maxBend (y.l, at, at + (size_t) (0.6 * kFs));
    std::printf ("  ping-pong off: after %.5f, before %.5f\n", after, before);
    REQUIRE (after < 1.25f * before);
}

// Hat Shimmer -> Tuned Kick (Nov 2026): Pour's Low Cut switched its filter out at
// 21 Hz in one sample, so sweeping it down to off clicked. It now fades out over
// 30-20 Hz, and off still leaves the signal untouched.
TEST_CASE ("Static: sweeping Pour Low Cut down to off is click-free, and off is transparent")
{
    PourEngine e;
    e.prepare (kFs, 32);
    PourParams p;
    p.lowCutHz = 600.0f;
    auto y = sine (233.08, 1.5, 0.3f);
    auto t = transport();
    const size_t at = (size_t) (0.5 * kFs);
    for (size_t i = 0; i < y.size(); i += 32)
    {
        if (i == at) p.lowCutHz = 20.0f;
        e.setParams (p);
        e.process (&y.l[i], &y.r[i], (int) std::min<size_t> (32, y.size() - i), t);
    }
    // The level rises as the cut opens up, so the sweep is compared with the settled
    // sound on both sides of it.
    const float before = maxBend (y.l, (size_t) (0.2 * kFs), at);
    const float sweep = maxBend (y.l, at, at + (size_t) (0.4 * kFs));
    const float after = maxBend (y.l, at + (size_t) (0.6 * kFs), y.size());
    std::printf ("  low cut to off: sweep %.5f, before %.5f, after %.5f\n", sweep, before, after);
    REQUIRE (sweep < 1.25f * std::max (before, after));

    // Low Cut off leaves the signal untouched (the Low Cut step is skipped at 20 Hz).
    // The tilt stage still rounds (lo + (x - lo)), as in 1.0.1, so the check is to
    // float rounding, not bit-exact.
    PourEngine off;
    off.prepare (kFs, 32);
    PourParams q;   // Low Cut at 20 Hz (off), no drive, strike or tilt
    auto in = music (0.5);
    auto z = in;
    for (size_t i = 0; i < z.size(); i += 32)
    {
        off.setParams (q);
        off.process (&z.l[i], &z.r[i], (int) std::min<size_t> (32, z.size() - i), t);
    }
    float worstErr = 0.0f;
    for (size_t i = 0; i < z.size(); ++i) worstErr = std::max (worstErr, std::abs (z.l[i] - in.l[i]));
    REQUIRE (worstErr < 1.0e-6f);
}

// Bass Mono switched on or off at one sample (20 Hz = off, hard threshold at 21 Hz).
// Switching it off dropped the filtered low end in one sample. It now fades, and off
// still passes the signal unchanged.
TEST_CASE ("Static: turning Bass Mono off while playing is click-free, and off is bit-exact")
{
    OutputStage o;
    o.prepare (kFs);
    OutputParams p;
    p.bassMonoHz = 150.0f;
    auto y = sine (80.0, 1.5, 0.4f);
    for (size_t i = 0; i < y.size(); ++i) y.r[i] = -y.r[i];   // all side: Bass Mono matters
    const auto dry = y;
    const size_t at = (size_t) (0.5 * kFs);
    for (size_t i = 0; i < y.size(); i += 32)
    {
        if (i == at) p.bassMonoHz = 20.0f;
        o.setParams (p);
        o.process (&dry.l[i], &dry.r[i], &y.l[i], &y.r[i], (int) std::min<size_t> (32, y.size() - i));
    }
    // Bass Mono changes this signal, so the switch is compared with the settled sound on
    // both sides of it.
    const float before = maxBend (y.l, (size_t) (0.2 * kFs), at);
    const float sw = maxBend (y.l, at, at + 4800);
    const float after = maxBend (y.l, at + (size_t) (0.3 * kFs), y.size());
    std::printf ("  bass mono off: switch %.5f, before %.5f, after %.5f\n", sw, before, after);
    REQUIRE (sw < 1.25f * std::max (before, after));
    // Once it has faded out (25 ms smoother), off passes the signal unchanged.
    for (size_t i = at + (size_t) (0.3 * kFs); i < y.size(); ++i) REQUIRE (y.l[i] == dry.l[i]);
}
