// Whole-chain verification: transparency, click-free stage switching, the
// Echo Return loop staying safe, limiter ceiling and CPU.

#include "TestUtil.h"
#include "dsp/Chain.h"
#include "dsp/OutputStage.h"

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <chrono>
#include <functional>

using namespace vessel::dsp;
using namespace testutil;
using Catch::Approx;

namespace
{
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
void renderChain (Chain& c, Setup& s, Stereo& y, std::function<void (size_t)> perBlock = {})
{
    Transport t;
    t.sampleRate = kFs;
    t.bpm = 120.0;
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

Setup everythingOn()
{
    Setup s;
    s.chain.on.fill (true);
    s.chain.echoReturn = 1.0f;
    s.chain.echoFeedback = 0.8f;
    s.body.decaySec = 6.0f;
    s.body.mix = 0.8f;
    s.body.slosh = 1.0f;
    s.fracture.crack = s.fracture.grit = s.fracture.fold = s.fracture.shard = 0.8f;
    s.liquid.ripple = 1.0f;
    s.echo.feedback = 0.8f;
    s.echo.mix = 0.6f;
    s.echo.timeMs = 120.0f;
    s.space.shimmer = 1.0f;
    s.space.mix = 0.5f;
    s.space.decaySec = 8.0f;
    return s;
}
} // namespace

TEST_CASE ("Chain: every stage off is bit-exact")
{
    Chain c;
    c.prepare (kFs, 32);
    Setup s;
    s.chain.on.fill (false);
    auto in = music (1.0);
    auto y = in;
    renderChain (c, s, y);
    REQUIRE (y.l == in.l);
    REQUIRE (y.r == in.r);
}

TEST_CASE ("Chain: switching the Body on and off is click-free")
{
    Chain c;
    c.prepare (kFs, 32);
    Setup s;
    s.chain.on = { false, true, false, false, false, false };
    s.body.mix = 1.0f;
    auto y = sine (330.0, 2.0, 0.4f);
    const float smooth = maxStep (sine (330.0, 0.1, 0.4f).l);
    renderChain (c, s, y, [&] (size_t i) { s.chain.on[body] = (i / (size_t) (0.25 * kFs)) % 2 == 0; });
    REQUIRE (allFinite (y));
    const float worst = maxStep (y.l, (size_t) (0.1 * kFs));
    std::printf ("  body toggles: largest step %.4f (sine alone %.4f)\n", worst, smooth);
    REQUIRE (worst < smooth * 6.0f);
}

TEST_CASE ("Chain: Echo Return at maximum stays bounded and its tail decays")
{
    Chain c;
    c.prepare (kFs, 32);
    auto s = everythingOn();
    s.space.shimmer = 0.0f;
    auto y = withSilence (music (2.0, 0.3f), 14.0);
    renderChain (c, s, y);
    REQUIRE (allFinite (y));
    REQUIRE (peak (y) < 3.0f);
    const float early = rms (y.l, (size_t) (2.0 * kFs), (size_t) (3.0 * kFs));
    const float late = rms (y.l, (size_t) (15.0 * kFs), (size_t) (16.0 * kFs));
    std::printf ("  echo return loop: tail fell %.1f dB in 12 s\n", db (early) - db (late));
    REQUIRE (db (late) < db (early) - 20.0f);
}

TEST_CASE ("Output: the limiter holds -0.3 dBFS")
{
    OutputStage o;
    o.prepare (kFs);
    OutputParams p;
    p.outGain = 8.0f;
    o.setParams (p);
    auto y = music (1.0, 0.4f);
    auto d = y;
    o.process (d.l.data(), d.r.data(), y.l.data(), y.r.data(), (int) y.size());
    REQUIRE (peak (y) <= OutputStage::kCeiling + 1e-6f);
}

TEST_CASE ("Chain: CPU, every stage on, worst case")
{
    Chain c;
    c.prepare (kFs, 32);
    auto s = everythingOn();
    const size_t seconds = 10;
    auto y = music ((double) seconds);
    const auto t0 = std::chrono::steady_clock::now();
    renderChain (c, s, y);
    const double secs = std::chrono::duration<double> (std::chrono::steady_clock::now() - t0).count();
    const double cpu = 100.0 * secs / (double) seconds;
    std::printf ("  every stage on, worst case: %.1f%% of one core\n", cpu);
    REQUIRE (allFinite (y));
    REQUIRE (cpu < 25.0);
}
