#pragma once
// FRACTURE: what happens when the vessel cracks.
//   Fold   - wavefolding: the sound folds back on itself (glassy, then harsh)
//   Grit   - bit and sample-rate reduction on one knob
//   Shard  - ring modulation against an inharmonic partner of the body's
//            pitch, so the shards stay related to the vessel's tuning
//   Crack  - glassy cracks triggered by the input (tiny resonant bursts) and,
//            above halfway, short dropouts as pieces break away
//   Tone   - dark .. bright tilt after the damage
// Placed after the Body by default; Pre-Body cracks the sound that rings it.

#include "Module.h"

namespace vessel::dsp
{

struct FractureParams
{
    float crack = 0.0f, grit = 0.0f, fold = 0.0f, shard = 0.0f;   // 0..1
    float tone = 0.0f;                                            // -1..1
    float mix = 1.0f;
    float shardHz = 700.0f;
    std::uint32_t seed = 1;
};

class FractureEngine : public Module
{
public:
    void prepare (double sampleRate, int) override
    {
        fs = (float) sampleRate;
        for (auto* s : { &crackS, &gritS, &foldS, &shardS, &toneS, &shardHzS }) s->prepare (sampleRate, 25.0f);
        for (auto& f : tilt) { f.prepare (sampleRate); f.setCutoff (1500.0f); }
        for (auto& f : crackFilter) f.prepare (sampleRate);
        burstDecay = std::exp (-1.0f / (0.004f * fs));
        envRel = std::exp (-1.0f / (0.05f * fs));
        mixS.prepare (sampleRate);
        dropS.prepare (sampleRate, 2.0f);
        primed = false;
        reset();
    }

    void reset() override
    {
        for (auto& f : tilt) f.reset();
        for (auto& f : crackFilter) f.reset();
        for (auto& h : held) h = 0.0f;
        holdCount = 0.0f;
        burst[0] = burst[1] = 0.0f;
        env = 0.0f;
        dropLeft = 0;
        dropS.snapTo (1.0f);
        phase[0] = phase[1] = 0.0f;
    }

    void setParams (const FractureParams& p) noexcept
    {
        crackS.setTarget (clamp01 (p.crack));
        gritS.setTarget (clamp01 (p.grit));
        foldS.setTarget (clamp01 (p.fold));
        shardS.setTarget (clamp01 (p.shard));
        toneS.setTarget (std::clamp (p.tone, -1.0f, 1.0f));
        shardHzS.setTarget (std::clamp (p.shardHz * 3.17f, 40.0f, 6000.0f));
        mixS.set (p.mix, ! primed);
        if (p.seed != seed) { seed = p.seed; noise.reseed (p.seed * 747796405u + 1u); }
        if (! primed)
        {
            for (auto* s : { &crackS, &gritS, &foldS, &shardS, &toneS, &shardHzS }) s->snapTo (s->getTarget());
            primed = true;
        }
    }

    // During a sound change (the fade and the 150 ms the new sound settles) no new
    // crack starts; one already ringing finishes. Cracks are events the sound
    // triggers, and a body still ringing up after a switch set off cracks bigger than
    // the settled sound ever makes (Nov 2026).
    void setHoldCracks (bool hold) noexcept { holdCracks = hold; }

    void process (float* L, float* R, int n, const Transport&) noexcept override
    {
        float* io[2] = { L, R };
        for (int i = 0; i < n; ++i)
        {
            const float crack = crackS.next(), grit = gritS.next(), fold = foldS.next(), shard = shardS.next();
            const float tone = toneS.next(), shardHz = shardHzS.next(), mix = mixS.next();
            const float a = std::max (std::abs (io[0][i]), std::abs (io[1][i]));
            env = std::max (a, env * envRel);

            // Crack events: denser and louder the more signal is coming in, and
            // none at all in silence (Fracture never makes noise on its own).
            if (! holdCracks && crack > 1.0e-3f && env > 1.0e-4f && noise.next01() < crack * crack * 0.0025f * (0.15f + 6.0f * env))
            {
                const float f = 1500.0f + 7500.0f * noise.next01();
                for (int c = 0; c < 2; ++c)
                {
                    crackFilter[c].set (f * (c == 0 ? 1.0f : 1.07f), kCrackK);
                    burst[c] = (0.4f + 0.8f * noise.next01()) * env * 3.0f;
                }
                // Pieces break away: a short dropout now and then.
                if (crack > 0.5f && noise.next01() < (crack - 0.5f) * 0.6f)
                    dropLeft = (int) ((0.004f + 0.025f * noise.next01()) * fs);
            }
            dropS.setTarget (dropLeft > 0 ? 0.0f : 1.0f);
            if (dropLeft > 0) --dropLeft;
            const float drop = dropS.next();

            // Sample-and-hold for the downsampler (shared clock: no stereo smear).
            const float ds = 1.0f + 15.0f * grit * grit;
            bool take = true;
            if (grit > 1.0e-3f)
            {
                holdCount += 1.0f;
                take = holdCount >= ds;
                if (take) holdCount -= ds;
            }
            const float step = std::exp2 (1.0f - (16.0f - 12.0f * grit));

            for (int c = 0; c < 2; ++c)
            {
                const float x = io[c][i];
                float y = x;
                if (fold > 1.0e-3f)
                {
                    // Scaled so quiet signals keep their level: folding adds
                    // harmonics, not volume.
                    const float d = 1.0f + 7.0f * fold;
                    const float folded = std::sin (0.5f * kPi * d * y) * 1.3f / (0.5f * kPi * d);
                    y = lerp (y, folded, std::min (1.0f, fold * 2.0f));
                }
                if (grit > 1.0e-3f)
                {
                    if (take) held[c] = std::round (y / step) * step;
                    y = lerp (y, held[c], std::min (1.0f, grit * 3.0f));
                }
                if (shard > 1.0e-3f)
                {
                    phase[c] += shardHz * (c == 0 ? 1.0f : 1.013f) / fs;
                    phase[c] -= std::floor (phase[c]);
                    y = lerp (y, y * 1.4f * std::sin (kTwoPi * phase[c]), shard);
                }
                if (crack > 1.0e-3f)
                {
                    burst[c] *= burstDecay;
                    // k * bp has unity gain at the crack's pitch; tanh keeps a crack
                    // below the level of the sound that caused it.
                    const float bp = crackFilter[c].tick (noise.next() * burst[c]).bp * kCrackK;
                    y = y * lerp (1.0f, drop, crack) + crack * 0.8f * env * std::tanh (bp * 6.0f / (env + 1.0e-3f));
                }
                const float lo = tilt[c].lowpass (y);
                y = lo * dbToGain (-6.0f * tone) + (y - lo) * dbToGain (6.0f * tone);
                io[c][i] = x + mix * (y - x);
            }
        }
    }

private:
    static constexpr float kCrackK = 0.05f;   // crack resonance (Q = 20)
    float fs = 48000.0f, burstDecay = 0.99f, envRel = 0.999f, env = 0.0f, holdCount = 0.0f;
    float held[2] {}, burst[2] {}, phase[2] {};
    int dropLeft = 0;
    std::uint32_t seed = 0;
    Smoother crackS, gritS, foldS, shardS, toneS, shardHzS, dropS;
    MixSmoother mixS;
    OnePole tilt[2];
    Svf crackFilter[2];
    Noise noise;
    bool primed = false;
    bool holdCracks = false;
};

} // namespace vessel::dsp
