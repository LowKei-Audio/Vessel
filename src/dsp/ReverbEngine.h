#pragma once
// SPACE: the room around the vessel. 8-line feedback delay network with
// modulated lines.
//
//   pre-delay -> 4 input diffusers -> FDN (Hadamard mix, per-line damping)
//   Shimmer  - an octave-up pitch shifter inside the feedback (rising halo)
//   Modes:   Hall     - smooth, dense, the default
//            Plate    - brighter, denser early build
//            Reverse  - the tail is played back in reversed, rising swells
//            Gated    - the tail is cut by a gate that follows the input
//   Freeze   - infinite tail: no decay, no new input (a held chord of space)

#include "DelayLine.h"
#include "Module.h"
#include "PitchShifter.h"

#include <array>
#include <vector>

namespace vessel::dsp
{

enum class ReverbMode { hall = 0, plate, reverse, gated };

struct ReverbParams
{
    ReverbMode mode = ReverbMode::hall;
    float size = 0.6f;         // 0..1
    float decaySec = 3.0f;     // 0.2..20
    float predelayMs = 20.0f;  // 0..250
    float dampHz = 7000.0f;    // 1000..20000
    float lowCutHz = 100.0f;   // 20..1000
    float shimmer = 0.0f;      // 0..1
    float modulation = 0.3f;   // 0..1
    bool freeze = false;
    float mix = 0.3f;
};

class ReverbEngine : public Module
{
public:
    static constexpr int kLines = 8;

    void prepare (double sampleRate, int) override
    {
        fs = (float) sampleRate;
        const float scale = fs / 48000.0f;
        for (int k = 0; k < kLines; ++k)
        {
            lines[(size_t) k].prepare ((std::size_t) (kBaseLen[k] * 2.2f * scale) + 64);
            damp[(size_t) k].prepare (sampleRate);
            lfo[(size_t) k] = (float) k / kLines;
        }
        for (auto& d : diffusers) d.prepare ((std::size_t) (0.03f * fs) + 8);
        for (auto& p : pre) p.prepare ((std::size_t) (0.26f * fs) + 8);
        for (auto& h : lowcut) h.prepare (sampleRate);
        for (auto& s : shimmerPs) { s.prepare (sampleRate); s.setRatio (2.0f); s.setWindowMs (80.0f); }
        revLen = (int) (0.5f * fs);
        for (auto& r : revBuf) r.assign ((size_t) (2.0f * fs) + 16, 0.0f);
        gateS.prepare (sampleRate, 8.0f);
        for (auto* s : { &sizeS, &fbS, &shimS, &modS, &preS, &freezeS, &decayS })
            s->prepare (sampleRate, 60.0f);
        mixS.prepare (sampleRate);
        inRamp = 1.0f;
        inRampStep = 1.0f / (0.010f * fs);
        jumpPending = false;
        primed = false;
        reset();
    }

    void reset() override
    {
        for (auto& l : lines) l.reset();
        for (auto& d : damp) d.reset();
        for (auto& d : diffusers) d.reset();
        for (auto& p : pre) p.reset();
        for (auto& h : lowcut) h.reset();
        for (auto& s : shimmerPs) s.reset();
        for (auto& r : revBuf) std::fill (r.begin(), r.end(), 0.0f);
        revPos = 0;
        env = 0.0f;
        gateHold = 0;
        gateS.snapTo (0.0f);
    }

    void setParams (const ReverbParams& p) noexcept
    {
        const bool newRoom = jumpPending && primed
                             && (p.mode != params.mode
                                 || std::abs (0.25f + 0.75f * clamp01 (p.size) - sizeS.getCurrent()) > 0.01f
                                 || std::abs (std::clamp (p.predelayMs, 0.0f, 250.0f) * 0.001f * fs - preS.getCurrent()) > 1.0f);
        jumpPending = false;
        params = p;
        sizeS.setTarget (0.25f + 0.75f * clamp01 (p.size));
        // Smoothed: a step in the feedback gains of a ringing tail clicks (a preset that
        // changes Decay did, Nov 2026). Log domain, so 0.2 s -> 20 s moves evenly.
        decayS.setTarget (std::log (std::clamp (p.decaySec, 0.2f, 20.0f)));
        shimS.setTarget (clamp01 (p.shimmer));
        modS.setTarget (clamp01 (p.modulation));
        preS.setTarget (std::clamp (p.predelayMs, 0.0f, 250.0f) * 0.001f * fs);
        freezeS.setTarget (p.freeze ? 1.0f : 0.0f);
        const float dampHz = std::clamp (p.dampHz * (p.mode == ReverbMode::plate ? 1.5f : 1.0f), 1000.0f, 20000.0f);
        for (auto& d : damp) d.setCutoff (dampHz);
        for (auto& h : lowcut) h.setCutoff (std::clamp (p.lowCutHz, 20.0f, 1000.0f));
        revLen = std::clamp ((int) ((0.25f + 0.9f * clamp01 (p.size)) * fs), 256, (int) revBuf[0].size() / 2 - 8);
        mixS.set (p.mix, ! primed);
        if (newRoom)
        {
            // Size and pre-delay normally glide, which sweeps the old tail through the
            // moving delay lines (a rough, pitch-bent burst after a preset change, Nov
            // 2026). While the output is fully dry, start the new room empty instead and
            // fade its input in over 10 ms, so its lines fill without a step.
            for (auto* s : { &sizeS, &preS, &decayS }) s->snapTo (s->getTarget());
            reset();
            inRamp = 0.0f;
        }
        if (! primed)
        {
            for (auto* s : { &sizeS, &shimS, &modS, &preS, &freezeS, &decayS }) s->snapTo (s->getTarget());
            primed = true;
        }
    }

    // A new sound is being swapped in while the output is fully dry: if the room
    // changes (mode, size or pre-delay), the next setParams starts it fresh.
    void jumpOnNextParams() noexcept { jumpPending = true; }

    void process (float* L, float* R, int n, const Transport&) noexcept override
    {
        float* io[2] = { L, R };
        const float gateRel = std::exp (-1.0f / (0.05f * fs));

        for (int i = 0; i < n; ++i)
        {
            const float size = sizeS.next(), shim = shimS.next(), mod = modS.next(), preD = preS.next();
            const float frz = freezeS.next();
            const float logDecay = decayS.next();
            const float mix = mixS.next();
            const float xl = io[0][i], xr = io[1][i];

            // Pre-delay and input diffusion (on a mid/side-ish pair).
            float il = xl, ir = xr;
            if (inRamp < 1.0f)
            {
                inRamp = std::min (1.0f, inRamp + inRampStep);
                const float g = inRamp * inRamp * (3.0f - 2.0f * inRamp);
                il *= g; ir *= g;
            }
            pre[0].push (il); pre[1].push (ir);
            float a = pre[0].read (std::max (2.0f, preD)) * (1.0f - frz);
            float b = pre[1].read (std::max (2.0f, preD)) * (1.0f - frz);
            static constexpr float dl[4] = { 3.1f, 4.7f, 7.9f, 11.3f };
            for (int k = 0; k < 4; ++k)
            {
                auto& d = diffusers[(size_t) k];
                const float in = (k & 1) ? b : a;
                const float delayed = d.read (dl[k] * 0.001f * fs);
                const float v = in + 0.6f * delayed;
                d.push (v);
                const float o = delayed - 0.6f * v;
                if (k & 1) b = o; else a = o;
            }

            // Read the lines (with slow, de-correlated modulation).
            std::array<float, kLines> y {};
            const float scale = fs / 48000.0f;
            for (int k = 0; k < kLines; ++k)
            {
                lfo[(size_t) k] += (0.13f + 0.07f * (float) k) / fs;
                lfo[(size_t) k] -= std::floor (lfo[(size_t) k]);
                const float len = kBaseLen[k] * size * scale + mod * 12.0f * scale * std::sin (kTwoPi * lfo[(size_t) k]);
                y[(size_t) k] = lines[(size_t) k].read (std::max (8.0f, len));
            }

            // Per-line decay gain for the requested RT60 (frozen = lossless).
            // pow() is costly, so the gains are refreshed every 16 samples.
            if ((counter++ & 15) == 0)
            {
                decay = std::exp (logDecay);
                for (int k = 0; k < kLines; ++k)
                    lineGain[(size_t) k] = std::pow (10.0f, -3.0f * (kBaseLen[k] * size / 48000.0f) / decay);
            }
            float fbMix[kLines];
            for (int k = 0; k < kLines; ++k)
            {
                const float g = lineGain[(size_t) k];
                const float damped = lerp (damp[(size_t) k].lowpass (y[(size_t) k]), y[(size_t) k], frz);
                fbMix[k] = damped * lerp (g, 1.0f, frz);
            }
            hadamard (fbMix);

            // Shimmer: octave-up copy of the tail fed back in.
            const float sh0 = shimmerPs[0].process (y[0] + y[2]);
            const float sh1 = shimmerPs[1].process (y[1] + y[3]);

            for (int k = 0; k < kLines; ++k)
            {
                const float in = (k & 1) ? b : a;
                const float s = (k & 1) ? sh1 : sh0;
                const float v = fbMix[k] + 0.35f * in + shim * 0.25f * s * (1.0f - frz);
                lines[(size_t) k].push (softLimit (v));
            }

            float wl = 0.5f * (y[0] - y[2] + y[4] - y[6]) ;
            float wr = 0.5f * (y[1] - y[3] + y[5] - y[7]);
            wl = lowcut[0].highpass (wl);
            wr = lowcut[1].highpass (wr);

            if (params.mode == ReverbMode::reverse)
                reverseSwell (wl, wr);
            else if (params.mode == ReverbMode::gated)
            {
                const float lvl = std::max (std::abs (xl), std::abs (xr));
                env = std::max (lvl, env * gateRel);
                if (env > 0.05f) gateHold = (int) (std::min (decay, 2.0f) * 0.25f * fs);
                else if (gateHold > 0) --gateHold;
                gateS.setTarget (gateHold > 0 || frz > 0.5f ? 1.0f : 0.0f);
                const float g = gateS.next();
                wl *= g; wr *= g;
            }

            // Equal-power blend: the room is uncorrelated with the dry sound.
            const float dryG = std::cos (0.5f * kPi * mix), wetG = std::sin (0.5f * kPi * mix);
            io[0][i] = xl * dryG + wl * wetG;
            io[1][i] = xr * dryG + wr * wetG;
        }
    }

private:
    // Line lengths at 48 kHz for size = 1 (mutually prime-ish).
    static constexpr float kBaseLen[kLines] = { 2203.0f, 2531.0f, 2903.0f, 3313.0f, 3739.0f, 4243.0f, 4729.0f, 5303.0f };

    static void hadamard (float* x) noexcept
    {
        for (int h = 1; h < kLines; h <<= 1)
            for (int i = 0; i < kLines; i += h * 2)
                for (int j = i; j < i + h; ++j)
                {
                    const float a = x[j], b = x[j + h];
                    x[j] = a + b;
                    x[j + h] = a - b;
                }
        const float s = 0.35355339f;   // 1/sqrt(8): orthonormal, energy-preserving
        for (int k = 0; k < kLines; ++k) x[k] *= s;
    }

    // Reverse: record the tail in chunks, play each finished chunk backwards
    // with a rising envelope -> swelling, sucked-in reverb.
    void reverseSwell (float& l, float& r) noexcept
    {
        const int len = revLen;
        if (revPos >= len) { revPos = 0; revBank = ! revBank; }   // length may have shrunk
        const int half = (int) revBuf[0].size() / 2;
        const int bank = (revBank ? half : 0);
        revBuf[0][(size_t) (bank + revPos)] = l;
        revBuf[1][(size_t) (bank + revPos)] = r;
        const int other = revBank ? 0 : half;
        const int readIdx = other + (len - 1 - revPos);
        const float t = (float) revPos / (float) len;
        const float env2 = t * t * std::min (1.0f, (1.0f - t) * 20.0f);   // swell up, quick release
        l = 1.6f * env2 * revBuf[0][(size_t) readIdx];
        r = 1.6f * env2 * revBuf[1][(size_t) readIdx];
        if (++revPos >= len) { revPos = 0; revBank = ! revBank; }
    }

    float fs = 48000.0f, decay = 3.0f, env = 0.0f;
    unsigned counter = 0;
    std::array<float, kLines> lineGain {};
    int gateHold = 0, revLen = 24000, revPos = 0;
    bool revBank = false, jumpPending = false;
    float inRamp = 1.0f, inRampStep = 1.0f / 480.0f;
    ReverbParams params;
    std::array<DelayLine, kLines> lines;
    std::array<OnePole, kLines> damp;
    std::array<float, kLines> lfo {};
    std::array<DelayLine, 4> diffusers;
    std::array<DelayLine, 2> pre;
    std::array<OnePole, 2> lowcut;
    std::array<PitchShifter, 2> shimmerPs;
    std::vector<float> revBuf[2];
    Smoother sizeS, fbS, shimS, modS, preS, freezeS, gateS, decayS;
    MixSmoother mixS;
    bool primed = false;
};

} // namespace vessel::dsp
