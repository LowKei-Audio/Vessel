#pragma once
// BODY: the vessel itself. A bank of up to 16 modal resonators per side.
//
// Each mode is a complex one-pole resonator (a decaying phasor):
//     s <- r * e^(i w) * s + g * x,   y = Im(s)
// It is unconditionally stable, its frequency can glide or wobble without
// changing its energy (only the rotation changes), and r = 1 holds the ring
// forever, which is exactly what SEAL needs. The input gain g = 2 (1 - r)
// (with r capped at a 1.5 s ring) keeps each mode near unity peak gain.
//
//   excitation (Input | Transients | Noise) -> modes -> DC block
//   -> auto level (the body sits at the input's level) -> soft ceiling
//   out = poured input crossfaded with the body by Body Mix
//
// FILL lowers every mode like water in a glass (to ~0.6x at 100%) and damps
// the high modes; SLOSH (from LIQUID) wobbles each mode on its own phase.

#include "Materials.h"
#include "Module.h"

#include <array>
#include <utility>

namespace vessel::dsp
{

enum class ExciteMode { input = 0, transients, noise };

struct BodyParams
{
    int material = 0;
    TuneMode tune = TuneMode::note;
    float pitchHz = 220.0f;      // Free mode
    int root = 9;                // 0 = C .. 11 = B
    int octave = 3;              // 1..6
    int chord = 2;
    float fineCents = 0.0f;
    float decaySec = 2.0f;       // 0.05..20
    float damping = 0.3f;        // 0..1
    float brightness = 0.0f;     // -1..1
    float inharm = 0.0f;         // -1..1 compress / stretch the mode spacing
    float spread = 0.3f;         // 0..1 stereo detune
    int modes = 16;              // 4..16
    ExciteMode excite = ExciteMode::input;
    float glideMs = 40.0f;
    float fill = 0.0f;           // 0..1
    float slosh = 0.0f;          // 0..1 mode wobble depth
    float sloshPhase = 0.0f;     // 0..1 (from the Liquid clock)
    float turbulence = 0.0f;     // 0..1 random part of the wobble
    float seal = 0.0f;           // 0..1 (ramped): hold the ring, stop new input
    float mix = 0.7f;
    std::uint32_t seed = 1;
};

class BodyEngine : public Module
{
public:
    void prepare (double sampleRate, int) override
    {
        fs = (float) sampleRate;
        for (auto& d : dc) d.prepare (sampleRate);
        mixS.prepare (sampleRate);
        sealS.prepare (sampleRate, 30.0f);
        for (auto& e : envFast) e = 0.0f;
        for (auto& e : envSlow) e = 0.0f;
        aFast = std::exp (-1.0f / (0.002f * fs));
        rFast = std::exp (-1.0f / (0.020f * fs));
        aSlow = std::exp (-1.0f / (0.040f * fs));
        rSlow = std::exp (-1.0f / (0.200f * fs));
        powCoeff = std::exp (-1.0f / (0.6f * fs));
        guardRel = std::exp (-1.0f / (0.1f * fs));
        guardDown = 1.0f - std::exp (-1.0f / (0.015f * fs));
        guardSamples = (int) (0.5f * fs);
        guardLeft = 0;
        guardIn = guardOut = 0.0f;
        agcDown = 1.0f - std::exp (-1.0f / (0.12f * fs));
        agcUp = 1.0f - std::exp (-1.0f / (0.5f * fs));
        for (int k = 0; k < kMaxModes; ++k) drift[(size_t) k].prepare (sampleRate / 32.0);
        primed = false;
        reset();
    }

    void reset() override
    {
        for (auto& ch : state) for (auto& m : ch) m = {};
        for (auto& d : dc) d.reset();
        for (auto& e : envFast) e = 0.0f;
        for (auto& e : envSlow) e = 0.0f;
        inPow = outPow = 0.0f;
        agc = 1.0f;
        energy = 0.0f;
        for (auto& l : uiLevel) l = 0.0f;
    }

    // Called once per control block. Computes every mode's target.
    void setParams (const BodyParams& p, int blockLen) noexcept
    {
        params = p;
        mixS.set (p.mix, ! primed);
        sealS.setTarget (clamp01 (p.seal));
        if (! primed) sealS.snapTo (sealS.getTarget());
        if (p.seed != lastSeed)
        {
            lastSeed = p.seed;
            noise.reseed (p.seed * 2654435761u + 7u);
            for (int k = 0; k < kMaxModes; ++k) drift[(size_t) k].reseed (p.seed + 977u * (std::uint32_t) k);
        }

        const auto& mat = materials()[(size_t) std::clamp (p.material, 0, kNumMaterials - 1)];
        fundamental = baseFrequency (p);
        const float fillMul = 1.0f / std::sqrt (1.0f + 1.8f * p.fill * p.fill);
        const float warp = 1.0f + 0.22f * std::clamp (p.inharm, -1.0f, 1.0f);
        const float bright = std::clamp (p.brightness + mat.brightBias, -1.5f, 1.5f);
        // Right after a sound change (beginSoundChange) the modes jump to the new tuning:
        // the output is fully dry then, and each mode keeps its state, so the ring stays
        // continuous. Gliding there instead swept the modes through the held input and
        // rang them up to 3x louder for ~50 ms, into the ceiling (Nov 2026).
        const bool jumpTuning = std::exchange (tuningJumpPending, false);
        const float glideCoeff = p.glideMs <= 0.5f || ! primed || jumpTuning ? 1.0f
                                 : 1.0f - std::exp (-(float) blockLen / (0.001f * p.glideMs * fs));
        const int active = std::clamp (p.modes, 1, kMaxModes);

        for (int k = 0; k < kMaxModes; ++k)
        {
            const float ratio = std::pow (std::max (0.05f, modeRatio (p.tune, p.material, p.chord, k)), warp);

            // Slosh: each mode wobbles on its own phase; turbulence adds a random part.
            drift[(size_t) k].setRate (0.4f + 0.25f * (float) k);
            const float d = drift[(size_t) k].next();
            const float wob = p.slosh * (0.022f * std::sin (kTwoPi * p.sloshPhase + 1.7f * (float) k) * (1.0f - p.turbulence)
                                         + 0.03f * p.turbulence * d);
            const float target = fundamental * ratio * fillMul * (1.0f + wob);
            auto& f = freq[(size_t) k];
            f = f <= 0.0f ? target : f + glideCoeff * (target - f);

            // Level: material roll-off, brightness tilt, fade out near Nyquist.
            float amp = std::pow ((float) (k + 1), -mat.ampExp) * std::pow (ratio, 0.55f * bright);
            const float nyq = f / fs;
            amp *= nyq > 0.45f ? 0.0f : (nyq > 0.38f ? (0.45f - nyq) / 0.07f : 1.0f);
            if (k >= active) amp = 0.0f;
            if (f < 18.0f) amp *= std::max (0.0f, f / 18.0f);
            ampT[(size_t) k] = amp;

            // Decay: high modes ring shorter; water damps them further.
            float t60 = std::clamp (p.decaySec, 0.02f, 30.0f) * mat.decayMul
                        * std::pow (std::max (1.0f, ratio), -(mat.hfDamp + 1.2f * clamp01 (p.damping)))
                        / (1.0f + 1.5f * p.fill * (float) k / (float) kMaxModes);
            t60 = std::max (0.015f, t60);
            radius[(size_t) k] = std::exp (-6.9078f / (t60 * fs));
            // Input gain as if the ring were at most 1.5 s long: very long rings
            // still catch sound that lands between their modes (auto level
            // trims the result), instead of going almost silent.
            inGainK[(size_t) k] = 2.0f * (1.0f - std::exp (-6.9078f / (std::min (t60, 1.5f) * fs)));

            // Stereo: the two sides are tuned slightly apart (alternating per mode).
            const float sp = clamp01 (p.spread) * mat.spreadMul * 0.006f * ((k & 1) ? 1.0f : -1.0f) * (1.0f + 0.1f * (float) k);
            const float wl = kTwoPi * std::min (0.49f, f * (1.0f - sp) / fs);
            const float wr = kTwoPi * std::min (0.49f, f * (1.0f + sp) / fs);
            cosW[0][(size_t) k] = std::cos (wl); sinW[0][(size_t) k] = std::sin (wl);
            cosW[1][(size_t) k] = std::cos (wr); sinW[1][(size_t) k] = std::sin (wr);
            const float pan = 0.35f * clamp01 (p.spread) * ((k & 1) ? 1.0f : -1.0f);
            panG[0][(size_t) k] = 1.0f - pan;
            panG[1][(size_t) k] = 1.0f + pan;
        }
        primed = true;
    }

    // During a sound change the input fades out and back in. Mode 1 (fading out): the
    // slow envelope holds. Mode 2 (from the swap through the fade-in): it may also rise
    // with the fast one. Transients excitation then never hears the return as a new
    // hit, even when the new preset's Pour stage makes it louder (it struck the body on
    // preset changes, Nov 2026). Noise excitation always follows the input.
    void setDetectorMode (int mode) noexcept { detectorMode = mode; }

    // A new sound has just been swapped in. Auto level listens over ~0.6 s, so a new
    // body that rings louder than the old one plays at the old gain for 100+ ms: up to
    // twice its level, into the soft ceiling and the limiter, heard as crackle after a
    // preset change (Nov 2026). For the next 0.5 s a peak guard turns the body down
    // (~15 ms) whenever it runs more than 6 dB over the input. It only ever turns down,
    // and it is off the rest of the time, so a settled preset sounds exactly as before.
    void beginSoundChange() noexcept { guardLeft = guardSamples; guardIn = guardOut = 0.0f; tuningJumpPending = true; }

    void process (float* L, float* R, int n, const Transport&) noexcept override
    {
        float* io[2] = { L, R };
        static constexpr float kFreezeR = 0.9999995f;   // ~5 minutes to fade 60 dB
        float blockEnergy = 0.0f;

        for (int i = 0; i < n; ++i)
        {
            const float seal = sealS.next();
            const float mix = mixS.next();
            const float inGain = 1.0f - seal;
            const float lock = std::min (1.0f, seal * 4.0f);   // the ring locks well before the input has faded

            // Excitation.
            float ex[2];
            const float pin = 0.5f * (io[0][i] * io[0][i] + io[1][i] * io[1][i]) * inGain;
            for (int c = 0; c < 2; ++c)
            {
                const float x = io[c][i];
                const float a = std::abs (x);
                envFast[c] = a > envFast[c] ? a + aFast * (envFast[c] - a) : a + rFast * (envFast[c] - a);
                if (detectorMode == 0)
                    envSlow[c] = a > envSlow[c] ? a + aSlow * (envSlow[c] - a) : a + rSlow * (envSlow[c] - a);
                else if (detectorMode == 2)
                    envSlow[c] = std::max (envSlow[c], envFast[c]);   // after the swap: may only rise
                switch (params.excite)
                {
                    case ExciteMode::input: ex[c] = x; break;
                    case ExciteMode::transients:
                    {
                        const float t = std::max (0.0f, envFast[c] - 1.15f * envSlow[c]);
                        ex[c] = x * std::min (1.0f, 4.0f * t / (envFast[c] + 1.0e-6f)) * 2.5f;
                        break;
                    }
                    case ExciteMode::noise: ex[c] = noise.next() * envFast[c] * 3.0f; break;
                }
                if (extra[c] != nullptr) ex[c] += extraGain * extra[c][i];
                ex[c] *= inGain;
            }

            float y[2] = { 0.0f, 0.0f };
            for (int k = 0; k < kMaxModes; ++k)
            {
                const float amp = ampT[(size_t) k];
                const float r = lerp (radius[(size_t) k], kFreezeR, lock);
                const float g = inGainK[(size_t) k] * amp;
                for (int c = 0; c < 2; ++c)
                {
                    auto& s = state[(size_t) c][(size_t) k];
                    const float cr = r * cosW[c][(size_t) k], sr = r * sinW[c][(size_t) k];
                    const float re = cr * s.re - sr * s.im + g * panG[c][(size_t) k] * ex[c];
                    const float im = sr * s.re + cr * s.im;
                    s.re = flushDenormal (re);
                    s.im = flushDenormal (im);
                    y[c] += im;
                }
            }

            // Auto level: over a ~0.6 s window the body sits at the level of the
            // input (whatever the excitation mode), so a long ring after a hit
            // is not ducked, and a body tuned onto a loud note is not too loud.
            const float yl = dc[0].process (y[0]), yr = dc[1].process (y[1]);
            const float pout = 0.5f * (yl * yl + yr * yr);
            inPow = pin + powCoeff * (inPow - pin);
            outPow = pout + powCoeff * (outPow - pout);
            if (inPow > 1.0e-7f && seal < 0.5f)
            {
                const float target = std::clamp (std::sqrt (inPow / (outPow + 1.0e-12f)), 0.1f, 40.0f);
                agc += (target < agc ? agcDown : agcUp) * (target - agc);
            }

            // Overshoot guard, only for 0.5 s after a sound change (beginSoundChange):
            // the body's peak may not run more than 6 dB over the input's peak.
            if (guardLeft > 0)
            {
                --guardLeft;
                const float pkIn = std::sqrt (pin), pkOut = std::max (std::abs (yl), std::abs (yr)) * agc;
                guardIn = std::max (pkIn, guardIn * guardRel);
                guardOut = std::max (pkOut, guardOut * guardRel);
                if (guardOut > 2.0f * guardIn + 1.0e-6f)
                    agc += guardDown * (agc * (2.0f * guardIn) / guardOut - agc);
            }
            const float bl = bodyCeiling (yl * agc), br = bodyCeiling (yr * agc);
            blockEnergy = std::max (blockEnergy, std::max (std::abs (bl), std::abs (br)));
            // Near equal-power blend (the body is mostly uncorrelated with its
            // input) so the level holds steady across the Mix knob.
            if (mix != lastMix) { lastMix = mix; dryG = std::pow (1.0f - mix, 0.6f); wetG = std::pow (mix, 0.6f); }
            io[0][i] = io[0][i] * dryG + bl * wetG;
            io[1][i] = io[1][i] * dryG + br * wetG;
        }

        energy = std::max (blockEnergy, energy * 0.97f);
        for (int k = 0; k < kMaxModes; ++k)
        {
            const auto& s = state[0][(size_t) k];
            const float lvl = std::sqrt (s.re * s.re + s.im * s.im) * agc * 4.0f;
            uiLevel[(size_t) k] = std::max (lvl, uiLevel[(size_t) k] * 0.9f);
        }
    }

    // Extra signal that only rings the modes (Echo Return) for the next
    // process() call. Pass nullptr to clear.
    void setExtraExcitation (const float* l, const float* r, float gain) noexcept
    {
        extra[0] = l; extra[1] = r; extraGain = gain;
    }

    // Read by the UI (via the processor) and by Fracture's Shard.
    float fundamentalHz() const noexcept { return fundamental; }
    float modeHz (int k) const noexcept { return freq[(size_t) k]; }
    float modeLevel (int k) const noexcept { return ampT[(size_t) k] > 0.0f ? std::min (1.0f, uiLevel[(size_t) k]) : 0.0f; }
    float level() const noexcept { return energy; }

    static float baseFrequency (const BodyParams& p) noexcept
    {
        float hz = p.pitchHz;
        if (p.tune != TuneMode::free)
            hz = midiToHz ((float) (12 * (std::clamp (p.octave, 0, 8) + 1) + std::clamp (p.root, 0, 11)));
        return std::clamp (hz * std::exp2 (p.fineCents / 1200.0f), 20.0f, 4000.0f);
    }

private:
    struct Phasor { float re = 0.0f, im = 0.0f; };

    // Linear up to 0.8, then a smooth bend to a 1.2 ceiling.
    static float bodyCeiling (float x) noexcept
    {
        const float a = std::abs (x);
        if (a <= 0.8f) return x;
        const float y = 0.8f + 0.4f * std::tanh ((a - 0.8f) / 0.4f);
        return x < 0.0f ? -y : y;
    }

    float fs = 48000.0f;
    BodyParams params;
    std::array<std::array<Phasor, kMaxModes>, 2> state {};
    std::array<float, kMaxModes> freq {}, ampT {}, radius {}, inGainK {}, uiLevel {};
    std::array<float, kMaxModes> cosW[2] {}, sinW[2] {}, panG[2] {};
    std::array<Drift, kMaxModes> drift;
    DcBlocker dc[2];
    MixSmoother mixS;
    int detectorMode = 0;
    float guardRel = 0.999f, guardDown = 0.001f, guardIn = 0.0f, guardOut = 0.0f;
    int guardSamples = 24000, guardLeft = 0;
    bool tuningJumpPending = false;
    Smoother sealS;
    Noise noise;
    float envFast[2] {}, envSlow[2] {};
    const float* extra[2] { nullptr, nullptr };
    float extraGain = 0.0f;
    float aFast = 0.0f, rFast = 0.0f, aSlow = 0.0f, rSlow = 0.0f;
    float powCoeff = 0.999f, agcDown = 0.001f, agcUp = 0.0001f;   // auto-level smoothing
    float inPow = 0.0f, outPow = 0.0f, agc = 1.0f, energy = 0.0f, fundamental = 220.0f;
    std::uint32_t lastSeed = 0;
    float lastMix = -1.0f, dryG = 1.0f, wetG = 0.0f;
    bool primed = false;
};

} // namespace vessel::dsp
