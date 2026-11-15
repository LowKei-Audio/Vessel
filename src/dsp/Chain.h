#pragma once
// The Vessel signal chain (fixed order, each stage switchable click-free):
//
//   POUR -> [FRACTURE pre] -> BODY -> [FRACTURE post] -> LIQUID -> ECHO -> SPACE
//                              ^                                     |
//                              +---------- Echo Return --------------+
//
// Echo Return feeds each block's echoes into the Body's modes on the next
// block (a 32-sample loop at most), so every repeat rings the vessel again.
// The Liquid clock lives here: it drives both Body slosh and Liquid ripple.

#include "BodyEngine.h"
#include "EchoEngine.h"
#include "FractureEngine.h"
#include "LiquidEngine.h"
#include "PourEngine.h"
#include "ReverbEngine.h"

#include <array>

namespace vessel::dsp
{

enum Stage : int { pour = 0, body, fracture, liquid, echo, space, kStageCount };

struct ChainParams
{
    std::array<bool, kStageCount> on { true, true, false, false, false, false };
    bool fracturePreBody = false;
    float echoReturn = 0.0f;           // 0..1
    float echoFeedback = 0.0f;         // the Echo's own feedback (caps the return loop)
    float liquidRateHz = 0.3f;
    bool liquidSync = false;
    int liquidDivision = Div::bar1;
    float slosh = 0.0f, turbulence = 0.0f;
};

class Chain
{
public:
    static constexpr int kMaxBlock = 32;

    void prepare (double sampleRate, int)
    {
        fs = sampleRate;
        for (auto* m : modules()) m->prepare (sampleRate, kMaxBlock);
        for (auto& s : onS) s.prepare (sampleRate, 12.0f);
        primed = false;
        sloshGlide.reset();
        clockPrimed = false;
        reset();
    }

    // A sound change is fading the input out and back in: onset detectors hold
    // (1, fading out) or hold and follow upward (2, from the swap through the fade-in).
    void setDetectorMode (int mode) noexcept
    {
        pourE.setDetectorMode (mode);
        bodyE.setDetectorMode (mode);
        fractureE.setHoldCracks (mode != 0);
    }

    // The host playhead jumped over sound: ease the synced slosh onto the new position.
    void armGlide() noexcept { sloshGlide.arm(); }

    void reset()
    {
        for (auto* m : modules()) m->reset();
        retBuf[0].fill (0.0f);
        retBuf[1].fill (0.0f);
    }

    void setParams (const ChainParams& p) noexcept
    {
        params = p;
        for (int s = 0; s < kStageCount; ++s)
        {
            onS[(size_t) s].setTarget (p.on[(size_t) s] ? 1.0f : 0.0f);
            if (! primed) onS[(size_t) s].snapTo (onS[(size_t) s].getTarget());
        }
        primed = true;
    }

    // Liquid clock at the start of the next block, and its per-sample step.
    void clock (const Transport& t, int n, float& phase, float& inc) noexcept
    {
        // Sync switched on or off, or a new division: carry on from where the slosh was
        // heading instead of jumping to the new clock's phase (a jump retunes the ringing
        // body and the ripple chorus in one sample, heard as a click, Nov 2026).
        const bool changed = clockPrimed && (params.liquidSync != lastSync
                                             || (params.liquidSync && params.liquidDivision != lastDivision));
        if (params.liquidSync)
        {
            if (changed) sloshGlide.continueFrom (lastEnd);
            const double beats = divisionBeats (params.liquidDivision);
            double incD = 0.0;
            const double pos = sloshGlide.process (t.ppq / beats, t.beatsPerSample() / beats, n, fs, incD);
            phase = (float) (pos - std::floor (pos));
            inc = (float) incD;
        }
        else
        {
            sloshGlide.reset();
            if (changed) freePhase = lastEnd - std::floor (lastEnd);
            inc = std::clamp (params.liquidRateHz, 0.01f, 20.0f) / (float) fs;
            phase = (float) freePhase;
            freePhase += (double) inc * n;
            freePhase -= std::floor (freePhase);
        }
        lastSync = params.liquidSync;
        lastDivision = params.liquidDivision;
        lastEnd = (double) phase + (double) inc * n;
        clockPrimed = true;
    }

    // n <= kMaxBlock. Engines' params must be set before this call.
    void process (float* L, float* R, int n, const Transport& t) noexcept
    {
        run (pour, pourE, L, R, n, t);
        if (params.fracturePreBody) run (fracture, fractureE, L, R, n, t);

        // Return gain is capped by (1 - feedback), so echo feedback plus the
        // trip through the (auto-levelled) body can never exceed unity.
        const float retGain = params.on[echo] ? clamp01 (params.echoReturn) * 0.9f * (1.0f - clamp01 (params.echoFeedback)) : 0.0f;
        bodyE.setExtraExcitation (retBuf[0].data(), retBuf[1].data(), retGain);
        run (body, bodyE, L, R, n, t);
        bodyE.setExtraExcitation (nullptr, nullptr, 0.0f);

        if (! params.fracturePreBody) run (fracture, fractureE, L, R, n, t);
        run (liquid, liquidE, L, R, n, t);

        const bool echoActive = run (echo, echoE, L, R, n, t);
        for (int c = 0; c < 2; ++c)
            for (int i = 0; i < kMaxBlock; ++i)
                retBuf[(size_t) c][(size_t) i] = echoActive && i < n ? echoE.returns (c)[i] : 0.0f;

        run (space, spaceE, L, R, n, t);
    }

    float stageLevel (int s) const noexcept { return onS[(size_t) s].getCurrent(); }

    PourEngine pourE;
    BodyEngine bodyE;
    FractureEngine fractureE;
    LiquidEngine liquidE;
    EchoEngine echoE;
    ReverbEngine spaceE;

private:
    std::array<Module*, kStageCount> modules() noexcept { return { &pourE, &bodyE, &fractureE, &liquidE, &echoE, &spaceE }; }

    // Runs one stage with a click-free on/off crossfade. Returns true if it ran.
    bool run (int s, Module& m, float* L, float* R, int n, const Transport& t) noexcept
    {
        auto& fade = onS[(size_t) s];
        if (fade.getTarget() < 0.5f && fade.getCurrent() < 1.0e-4f)
        {
            fade.snapTo (0.0f);
            dormant[(size_t) s] = true;
            return false;
        }
        if (dormant[(size_t) s])
        {
            m.reset();   // no stale echoes or tails when a stage comes back
            dormant[(size_t) s] = false;
        }
        std::copy (L, L + n, dryL.begin());
        std::copy (R, R + n, dryR.begin());

        // The stage's input fades with its output. A stage that wakes up starts with
        // empty delay lines: fed the full signal at once, a chorus or echo tap reads
        // silence and then the signal arrives in one step, heard as a click a few ms
        // after Liquid or Echo is switched on (or a preset turns it on). Faded in, the
        // lines fill smoothly. Fully on, the gain is exactly 1: nothing changes.
        std::array<float, kMaxBlock> g {};
        bool partial = false;
        for (int i = 0; i < n; ++i)
        {
            g[(size_t) i] = fade.next();
            partial = partial || g[(size_t) i] != 1.0f;
        }
        if (partial)
            for (int i = 0; i < n; ++i) { L[i] *= g[(size_t) i]; R[i] *= g[(size_t) i]; }

        m.process (L, R, n, t);
        if (partial)
            for (int i = 0; i < n; ++i)
            {
                L[i] = dryL[(size_t) i] + g[(size_t) i] * (L[i] - dryL[(size_t) i]);
                R[i] = dryR[(size_t) i] + g[(size_t) i] * (R[i] - dryR[(size_t) i]);
            }
        return true;
    }

    double fs = 48000.0, freePhase = 0.0;
    PhaseGlide sloshGlide;
    double lastEnd = 0.0;          // slosh phase the last block was heading to
    int lastDivision = 0;
    bool lastSync = false, clockPrimed = false;
    ChainParams params;
    std::array<Smoother, kStageCount> onS;
    std::array<float, kMaxBlock> dryL {}, dryR {};
    std::array<std::array<float, kMaxBlock>, 2> retBuf {};
    std::array<bool, kStageCount> dormant {};
    bool primed = false;
};

} // namespace vessel::dsp
