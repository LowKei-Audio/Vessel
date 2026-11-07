// Engine-level verification: the Body rings where it is tuned, Fill and Chord
// tuning move the modes correctly, Seal holds a ring, and every engine stays
// finite and bounded at its most extreme settings.

#include "TestUtil.h"
#include "dsp/BodyEngine.h"
#include "dsp/EchoEngine.h"
#include "dsp/FractureEngine.h"
#include "dsp/LiquidEngine.h"
#include "dsp/PourEngine.h"

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

using namespace vessel::dsp;
using namespace testutil;
using Catch::Approx;

namespace
{
Stereo noise (double seconds, float amp = 0.2f)
{
    Stereo s ((size_t) (seconds * kFs));
    Noise n (11);
    for (size_t i = 0; i < s.size(); ++i) { s.l[i] = amp * n.next(); s.r[i] = amp * n.next(); }
    return s;
}

BodyParams bodyAt (int material, TuneMode tune = TuneMode::note)
{
    BodyParams p;
    p.material = material;
    p.tune = tune;
    p.root = 9;      // A
    p.octave = 3;    // A3 = 220 Hz
    p.mix = 1.0f;
    p.spread = 0.0f;
    p.glideMs = 0.0f;
    return p;
}

// Body needs its params once per control block (its slosh/glide clock).
void renderBody (BodyEngine& b, const BodyParams& p, Stereo& s)
{
    Transport t;
    t.sampleRate = kFs;
    for (size_t i = 0; i < s.size(); i += 32)
    {
        const int n = (int) std::min<size_t> (32, s.size() - i);
        b.setParams (p, n);
        b.process (&s.l[i], &s.r[i], n, t);
    }
}
} // namespace

TEST_CASE ("Body: Note tuning puts the fundamental on the note")
{
    BodyParams p = bodyAt (0);
    REQUIRE (BodyEngine::baseFrequency (p) == Approx (220.0f).epsilon (1e-4));
    p.root = 0; p.octave = 4;   // C4
    REQUIRE (BodyEngine::baseFrequency (p) == Approx (261.63f).epsilon (1e-3));
    p.fineCents = 100.0f;       // + one semitone
    REQUIRE (BodyEngine::baseFrequency (p) == Approx (277.18f).epsilon (1e-3));
}

TEST_CASE ("Body: white noise comes out peaked at the tuned modes")
{
    BodyEngine b;
    b.prepare (kFs, 32);
    auto p = bodyAt (6);   // Tube: odd harmonics 220, 660, 1100...
    p.decaySec = 1.5f;
    auto y = noise (3.0);
    renderBody (b, p, y);
    REQUIRE (allFinite (y));
    const size_t from = (size_t) kFs;
    const float atMode = toneLevel (y.l, 220.0, from), between = toneLevel (y.l, 440.0, from);
    std::printf ("  tube body: 220 Hz %.1f dB, 440 Hz %.1f dB\n", db (atMode), db (between));
    REQUIRE (atMode > between * 4.0f);
    REQUIRE (toneLevel (y.l, 660.0, from) > between * 2.0f);
}

TEST_CASE ("Body: Chord tuning re-tunes the modes to chord tones")
{
    BodyEngine b;
    b.prepare (kFs, 32);
    auto p = bodyAt (0, TuneMode::chord);
    p.chord = 3;   // Minor: 0, 3, 7, then up an octave
    b.setParams (p, 32);
    REQUIRE (b.modeHz (0) == Approx (220.0f).epsilon (1e-3));
    REQUIRE (b.modeHz (1) == Approx (220.0f * std::exp2 (3.0f / 12.0f)).epsilon (1e-3));
    REQUIRE (b.modeHz (2) == Approx (220.0f * std::exp2 (7.0f / 12.0f)).epsilon (1e-3));
    REQUIRE (b.modeHz (3) == Approx (440.0f).epsilon (1e-3));
}

TEST_CASE ("Body: Fill lowers the vessel like water in a glass")
{
    BodyEngine b;
    b.prepare (kFs, 32);
    auto p = bodyAt (0);
    p.fill = 1.0f;
    b.setParams (p, 32);
    REQUIRE (b.modeHz (0) == Approx (220.0f / std::sqrt (2.8f)).epsilon (1e-3));
    p.fill = 0.0f;
    p.glideMs = 0.0f;
    b.setParams (p, 32);
    REQUIRE (b.modeHz (0) == Approx (220.0f).epsilon (1e-3));
}

TEST_CASE ("Body: Seal holds the ring, without Seal it dies away")
{
    for (const bool sealed : { false, true })
    {
        BodyEngine b;
        b.prepare (kFs, 32);
        auto p = bodyAt (1);
        p.decaySec = 0.4f;
        auto y = withSilence (noise (1.0), 3.0);
        Transport t;
        t.sampleRate = kFs;
        for (size_t i = 0; i < y.size(); i += 32)
        {
            p.seal = (sealed && i >= (size_t) kFs) ? 1.0f : 0.0f;
            b.setParams (p, 32);
            b.process (&y.l[i], &y.r[i], 32, t);
        }
        const float atSeal = rms (y.l, (size_t) (1.1 * kFs), (size_t) (1.3 * kFs));
        const float later = rms (y.l, (size_t) (3.5 * kFs), (size_t) (4.0 * kFs));
        std::printf ("  %s: %.1f dB after 2.5 s\n", sealed ? "sealed" : "open", db (later) - db (atSeal));
        if (sealed) REQUIRE (db (later) > db (atSeal) - 6.0f);
        else        REQUIRE (db (later) < db (atSeal) - 60.0f);
    }
}

TEST_CASE ("Body: every material and tuning stays finite and bounded at extremes")
{
    for (int m = 0; m < kNumMaterials; ++m)
        for (int tune = 0; tune < 3; ++tune)
        {
            BodyEngine b;
            b.prepare (kFs, 32);
            auto p = bodyAt (m, (TuneMode) tune);
            p.decaySec = 20.0f;
            p.damping = 0.0f;
            p.brightness = 1.0f;
            p.inharm = 1.0f;
            p.spread = 1.0f;
            p.slosh = 1.0f;
            p.turbulence = 1.0f;
            p.fill = 0.7f;
            p.pitchHz = 2000.0f;
            p.octave = 6;
            p.excite = (ExciteMode) (m % 3);
            Stereo y = sine (220.0, 2.0, 0.9f);   // loud tone right on the fundamental
            renderBody (b, p, y);
            REQUIRE (allFinite (y));
            REQUIRE (peak (y) < 1.3f);
        }
}

TEST_CASE ("Pour: drive keeps music near its level")
{
    PourEngine e;
    e.prepare (kFs, 32);
    PourParams p;
    p.drive = 1.0f;
    e.setParams (p);
    auto in = music (2.0);
    auto y = in;
    render (e, y);
    const float d = db (rms (y.l)) - db (rms (in.l));
    std::printf ("  pour drive 100%%: %+.1f dB\n", d);
    REQUIRE (std::abs (d) < 6.0f);
}

TEST_CASE ("Fracture: everything at maximum stays bounded")
{
    FractureEngine e;
    e.prepare (kFs, 32);
    FractureParams p;
    p.crack = p.grit = p.fold = p.shard = 1.0f;
    p.tone = 1.0f;
    e.setParams (p);
    auto y = music (3.0, 0.3f);
    render (e, y);
    REQUIRE (allFinite (y));
    REQUIRE (peak (y) < 2.5f);
}

TEST_CASE ("Echo: maximum feedback never runs away")
{
    EchoEngine e;
    e.prepare (kFs, 32);
    EchoParams p;
    p.feedback = 0.95f;
    p.timeMs = 30.0f;
    p.mix = 1.0f;
    p.wobble = 1.0f;
    p.toneHz = 20000.0f;
    e.setParams (p);
    auto y = withSilence (music (2.0, 0.3f), 8.0);
    render (e, y);
    REQUIRE (allFinite (y));
    REQUIRE (peak (y) < 3.0f);
}

TEST_CASE ("Liquid: mix 0 is transparent")
{
    LiquidEngine e;
    e.prepare (kFs, 32);
    LiquidParams p;
    p.mix = 0.0f;
    p.ripple = 1.0f;
    e.setParams (p);
    auto in = music (1.0);
    auto y = in;
    render (e, y);
    for (size_t i = 0; i < in.size(); ++i) REQUIRE (y.l[i] == Approx (in.l[i]).margin (1e-6));
}
