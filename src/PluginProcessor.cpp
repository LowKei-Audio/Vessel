#include "PluginProcessor.h"
#include "PluginEditor.h"

namespace vessel
{
using namespace juce;

namespace
{
const Identifier kSlotType ("SLOT");
const Identifier kSrcProp ("src"), kDstProp ("dst"), kAmtProp ("amt");

// What Shatter does, in normalised units at full depth. Fracture (and Echo,
// if it is off) are brought in by Shatter itself, from a mix of zero.
struct ShatterMove { P p; float amount; };
constexpr ShatterMove kShatter[] = {
    { P::frCrack, 0.75f }, { P::frGrit, 0.45f }, { P::frFold, 0.4f }, { P::frShard, 0.3f },
    { P::bdInharm, 0.35f }, { P::fill, 0.3f }, { P::bdSpread, 0.4f },
    { P::ecMix, 0.3f }, { P::ecFeedback, 0.2f }, { P::ecReturn, 0.35f }, { P::spMix, 0.15f },
};
} // namespace

// ---------------------------------------------------------------------------
VesselProcessor::VesselProcessor()
    : AudioProcessor (BusesProperties()
                          .withInput  ("Input",  AudioChannelSet::stereo(), true)
                          .withOutput ("Output", AudioChannelSet::stereo(), true)),
      apvts (*this, &undo, "VESSEL", createParameterLayout())
{
    for (int i = 0; i < kNumParams; ++i)
    {
        raw[(size_t) i] = apvts.getRawParameterValue (def (i).id);
        jassert (raw[(size_t) i] != nullptr);
        ranges[(size_t) i] = apvts.getParameterRange (def (i).id);
    }
    lfos[0].setIndex (0);
    lfos[1].setIndex (1);

    ensureStateStructure();
    apvts.state.addListener (this);
    refreshFromState();

    presets.load (0);            // start on the Init preset
    undo.clearUndoHistory();     // loading Init is not an undoable edit
}

VesselProcessor::~VesselProcessor()
{
    apvts.state.removeListener (this);
}

bool VesselProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto& out = layouts.getMainOutputChannelSet();
    if (out != AudioChannelSet::mono() && out != AudioChannelSet::stereo())
        return false;
    return layouts.getMainInputChannelSet() == out;
}

void VesselProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    sampleRateHz = sampleRate;
    chain.prepare (sampleRate, kControlBlock);
    output.prepare (sampleRate);
    inGainS.prepare (sampleRate, 20.0f);
    inGainS.snapTo (Decibels::decibelsToGain (raw[(size_t) idx (P::inGain)]->load()));
    dry.setSize (2, samplesPerBlock);
    for (auto& l : lfos) l.reset();
    env.reset();
    drift.reset();
    shatterRamp = 0.0f;

    // Prime every engine with the current settings so nothing glides from zero.
    dsp::Transport t;
    t.sampleRate = sampleRate;
    t.bpm = raw[(size_t) idx (P::tempo)]->load();
    runControl (kControlBlock, 0.0f, t);
}

void VesselProcessor::reset()
{
    chain.reset();   // host stop / seek: clear rings, echoes and tails
    output.reset();
}

// ---------------------------------------------------------------------------
// Patch data
void VesselProcessor::ensureStateStructure()
{
    auto m = apvts.state.getChildWithName (kMatrixType);
    if (! m.isValid()) { m = ValueTree (kMatrixType); apvts.state.appendChild (m, nullptr); }
    while (m.getNumChildren() < kMatrixSlots)
    {
        ValueTree s (kSlotType);
        s.setProperty (kSrcProp, 0, nullptr);
        s.setProperty (kDstProp, "", nullptr);
        s.setProperty (kAmtProp, 0.0f, nullptr);
        m.appendChild (s, nullptr);
    }
}

ValueTree VesselProcessor::matrixTree() const { return apvts.state.getChildWithName (kMatrixType); }

void VesselProcessor::refreshFromState()
{
    const auto m = matrixTree();
    for (int i = 0; i < kMatrixSlots; ++i)
    {
        const auto s = m.getChild (i);
        auto& a = slotsA[(size_t) i];
        const int dst = s.isValid() ? findParam (s.getProperty (kDstProp).toString()) : -1;
        a.src.store (s.isValid() ? jlimit (0, kNumSources - 1, (int) s.getProperty (kSrcProp)) : 0);
        a.dst.store (dst >= 0 && def (dst).modulatable ? dst : -1);
        a.amount.store (s.isValid() ? jlimit (-1.0f, 1.0f, (float) s.getProperty (kAmtProp)) : 0.0f);
    }
}

MatrixSlot VesselProcessor::getSlot (int i) const
{
    MatrixSlot s;
    const auto t = matrixTree().getChild (i);
    if (! t.isValid()) return s;
    s.src = (Src) jlimit (0, kNumSources - 1, (int) t.getProperty (kSrcProp));
    s.dst = findParam (t.getProperty (kDstProp).toString());
    s.amount = (float) t.getProperty (kAmtProp);
    return s;
}

void VesselProcessor::setSlot (int i, const MatrixSlot& s)
{
    auto t = matrixTree().getChild (i);
    if (! t.isValid()) return;
    t.setProperty (kSrcProp, (int) s.src, &undo);
    t.setProperty (kDstProp, s.dst >= 0 ? String (def (s.dst).id) : String(), &undo);
    t.setProperty (kAmtProp, jlimit (-1.0f, 1.0f, s.amount), &undo);
}

void VesselProcessor::clearMatrix()
{
    for (int i = 0; i < kMatrixSlots; ++i) setSlot (i, {});
}

int VesselProcessor::addRoute (Src src, int dst, float amount)
{
    if (dst < 0 || ! def (dst).modulatable) return -1;
    for (int i = 0; i < kMatrixSlots; ++i)   // reuse a route with the same source and destination
    {
        const auto s = getSlot (i);
        if (s.src == src && s.dst == dst) { setSlot (i, { src, dst, amount }); return i; }
    }
    for (int i = 0; i < kMatrixSlots; ++i)
        if (! getSlot (i).isActive()) { setSlot (i, { src, dst, amount }); return i; }
    return -1;
}

void VesselProcessor::removeRoutesTo (int dst)
{
    for (int i = 0; i < kMatrixSlots; ++i)
        if (getSlot (i).dst == dst) setSlot (i, {});
}

Array<int> VesselProcessor::routesTo (int dst) const
{
    Array<int> r;
    for (int i = 0; i < kMatrixSlots; ++i)
    {
        const auto& a = slotsA[(size_t) i];
        if (a.dst.load() == dst && a.src.load() != 0 && a.amount.load() != 0.0f) r.add (i);
    }
    return r;
}

void VesselProcessor::setParamReal (int i, float real)
{
    if (auto* p = apvts.getParameter (def (i).id))
    {
        const float n = p->convertTo0to1 (real);
        if (std::abs (p->getValue() - n) < 1.0e-7f) return;
        p->beginChangeGesture();
        p->setValueNotifyingHost (n);
        p->endChangeGesture();
    }
}

ValueTree VesselProcessor::getPatchState() const
{
    return const_cast<AudioProcessorValueTreeState&> (apvts).copyState();   // copyState flushes params; logically const
}

void VesselProcessor::applyPatchState (const ValueTree& src, bool notifyHost)
{
    if (! src.hasType (apvts.state.getType())) return;

    if (! notifyHost)
    {
        apvts.replaceState (src.createCopy());
        ensureStateStructure();
        refreshFromState();
        return;
    }

    // Through the host-notifying path, so automation and undo stay in sync.
    for (int i = 0; i < kNumParams; ++i)
    {
        const auto child = src.getChildWithProperty ("id", def (i).id);
        setParamReal (i, child.isValid() ? (float) child.getProperty ("value") : def (i).def);
    }
    auto& st = apvts.state;
    if (src.hasProperty (kPresetNameProp)) st.setProperty (kPresetNameProp, src.getProperty (kPresetNameProp), &undo);

    auto from = src.getChildWithName (kMatrixType);
    auto to = st.getChildWithName (kMatrixType);
    if (from.isValid() && to.isValid()) to.copyPropertiesAndChildrenFrom (from, &undo);
    ensureStateStructure();
    refreshFromState();
}

namespace
{
// A saved state must survive save -> load -> save byte for byte (hosts and
// clap-validator compare them). Loading turns each value into the parameter's
// 0..1 position and back, which on skewed ranges can move a value by a few
// millionths, so each value is saved as the point that conversion maps to itself.
void makeParamValuesReloadStable (ValueTree& state, AudioProcessorValueTreeState& apvts)
{
    for (auto child : state)
    {
        if (! child.hasType ("PARAM")) continue;
        auto* p = apvts.getParameter (child.getProperty ("id").toString());
        if (p == nullptr) continue;
        auto v = (float) (double) child.getProperty ("value");
        for (int k = 0; k < 8; ++k)   // converges in one or two steps
        {
            const float next = p->convertFrom0to1 (p->convertTo0to1 (v));
            if (next == v) break;
            v = next;
        }
        child.setProperty ("value", v, nullptr);
    }
}
} // namespace

void VesselProcessor::getStateInformation (MemoryBlock& destData)
{
    auto state = getPatchState();
    makeParamValuesReloadStable (state, apvts);
    if (auto xml = state.createXml())
        copyXmlToBinary (*xml, destData);
}

void VesselProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary (data, sizeInBytes))
        if (xml->hasTagName (apvts.state.getType()))
        {
            applyPatchState (ValueTree::fromXml (*xml), false);
            updateHostDisplay (ChangeDetails().withProgramChanged (true));
            MessageManager::callAsync ([safe = WeakReference<VesselProcessor> (this)]
            {
                if (safe != nullptr) safe->presets.restoreSelectionFromState();
            });
        }
}

AudioProcessorEditor* VesselProcessor::createEditor() { return new VesselEditor (*this); }

// ---------------------------------------------------------------------------
// Control block: sources -> matrix -> Shatter -> effective values -> engines.
void VesselProcessor::runControl (int len, float inPeak, const dsp::Transport& t)
{
    const auto seed = (std::uint32_t) raw[(size_t) idx (P::seed)]->load();

    for (int i = 0; i < kNumParams; ++i)
        norm[(size_t) i] = ranges[(size_t) i].convertTo0to1 (raw[(size_t) i]->load());

    // Sources (LFO / Drift rates use last block's modulated values).
    for (int l = 0; l < 2; ++l)
    {
        const int base = idx (P::lfo1Shape) + l * 4;
        dsp::LfoParams lp;
        lp.shape = (dsp::LfoShape) (int) raw[(size_t) base]->load();
        lp.rateHz = eff[(size_t) base + 1] > 0.0f ? eff[(size_t) base + 1] : raw[(size_t) base + 1]->load();
        lp.sync = raw[(size_t) base + 2]->load() > 0.5f;
        lp.division = (int) raw[(size_t) base + 3]->load();
        srcVal[(int) Src::lfo1 + l] = lfos[l].advance (lp, t, len, seed);
    }
    {
        dsp::EnvParams ep { raw[(size_t) idx (P::envAttack)]->load(), raw[(size_t) idx (P::envRelease)]->load(),
                            raw[(size_t) idx (P::envGain)]->load() };
        srcVal[(int) Src::env] = env.advance (ep, inPeak, len, sampleRateHz);
    }
    {
        const float rate = eff[(size_t) idx (P::driftRate)] > 0.0f ? eff[(size_t) idx (P::driftRate)] : raw[(size_t) idx (P::driftRate)]->load();
        dsp::ChaosParams cp { rate, raw[(size_t) idx (P::driftSmooth)]->load() };
        srcVal[(int) Src::drift] = drift.advance (cp, len, sampleRateHz, seed);
    }
    for (int k = 0; k < kNumMacros; ++k)
        srcVal[(int) Src::macro1 + k] = norm[(size_t) idx (P::macro1) + (size_t) k];

    // Matrix.
    for (auto& s : slotsA)
    {
        const int dst = s.dst.load (std::memory_order_relaxed), src = s.src.load (std::memory_order_relaxed);
        if (dst < 0 || src == 0) continue;
        norm[(size_t) dst] += s.amount.load (std::memory_order_relaxed) * srcVal[src];
    }

    // Shatter: a smooth ramp in (Shatter Time), a quick fall back out.
    const bool shatterOn = raw[(size_t) idx (P::shatter)]->load() > 0.5f;
    const float sTime = std::max (0.02f, raw[(size_t) idx (P::shatterTime)]->load());
    const float step = (float) len / (float) sampleRateHz;
    shatterRamp = shatterOn ? std::min (1.0f, shatterRamp + step / sTime)
                            : std::max (0.0f, shatterRamp - step / std::min (sTime, 0.3f));
    const float sr = shatterRamp * shatterRamp * (3.0f - 2.0f * shatterRamp);   // smoothstep
    const float sh = sr * raw[(size_t) idx (P::shatterDepth)]->load();
    if (sh > 0.0f)
        for (const auto& m : kShatter) norm[(size_t) idx (m.p)] += sh * m.amount;
    uiShatter.store (sr, std::memory_order_relaxed);

    // Real values.
    for (int i = 0; i < kNumParams; ++i)
    {
        const auto& d = def (i);
        if (d.kind == Kind::flt)
        {
            norm[(size_t) i] = dsp::clamp01 (norm[(size_t) i]);
            eff[(size_t) i] = ranges[(size_t) i].convertFrom0to1 (norm[(size_t) i]);
        }
        else
        {
            eff[(size_t) i] = raw[(size_t) i]->load();
            norm[(size_t) i] = ranges[(size_t) i].convertTo0to1 (eff[(size_t) i]);
        }
    }

    // Shatter brings Fracture (and Echo) in from silence when they are off.
    auto forceIn = [&] (P onP, P mixP, float mixWhenOff)
    {
        if (eff[(size_t) idx (onP)] > 0.5f || sh <= 1.0e-4f) return;
        eff[(size_t) idx (onP)] = 1.0f;
        eff[(size_t) idx (mixP)] = mixWhenOff;
        norm[(size_t) idx (mixP)] = mixWhenOff;
    };
    forceIn (P::frOn, P::frMix, sh);
    forceIn (P::ecOn, P::ecMix, 0.35f * sh);
    if (raw[(size_t) idx (P::frOn)]->load() > 0.5f)
    {
        auto& fm = eff[(size_t) idx (P::frMix)];
        fm = fm + sh * (1.0f - fm);
        norm[(size_t) idx (P::frMix)] = fm;
    }

    for (int i = 0; i < kNumParams; ++i) uiNorm[(size_t) i].store (norm[(size_t) i], std::memory_order_relaxed);
    for (int l = 0; l < 2; ++l) uiLfo[l].store (srcVal[(int) Src::lfo1 + l], std::memory_order_relaxed);
    uiEnv.store (srcVal[(int) Src::env], std::memory_order_relaxed);
    uiDrift.store (srcVal[(int) Src::drift], std::memory_order_relaxed);

    pushEngineParams (t, len);
}

void VesselProcessor::pushEngineParams (const dsp::Transport& t, int len)
{
    auto v = [this] (P p) { return eff[(size_t) idx (p)]; };
    auto i = [this] (P p) { return (int) std::lround (eff[(size_t) idx (p)]); };
    auto b = [this] (P p) { return eff[(size_t) idx (p)] > 0.5f; };
    const auto seed = (std::uint32_t) i (P::seed);

    dsp::ChainParams cp;
    for (int s = 0; s < kNumStages; ++s) cp.on[(size_t) s] = b (kStages[s].on);
    cp.fracturePreBody = i (P::frPos) == 1;
    cp.echoReturn = v (P::ecReturn);
    cp.echoFeedback = v (P::ecFeedback);
    cp.liquidRateHz = v (P::lqRate);
    cp.liquidSync = b (P::lqSync);
    cp.liquidDivision = i (P::lqDiv);
    chain.setParams (cp);
    for (int s = 0; s < kNumStages; ++s) uiStageOn[(size_t) s].store (cp.on[(size_t) s], std::memory_order_relaxed);

    float phase = 0.0f, inc = 0.0f;
    chain.clock (t, len, phase, inc);
    uiSloshPhase.store (phase, std::memory_order_relaxed);

    dsp::PourParams po;
    po.drive = v (P::poDrive); po.strike = v (P::poStrike); po.tilt = v (P::poTilt); po.lowCutHz = v (P::poLowCut);
    chain.pourE.setParams (po);

    const bool liquidOn = cp.on[dsp::liquid];
    dsp::BodyParams bd;
    bd.material = i (P::bdMaterial);
    bd.tune = (dsp::TuneMode) i (P::bdTune);
    bd.pitchHz = v (P::bdPitch); bd.root = i (P::bdRoot); bd.octave = i (P::bdOctave); bd.chord = i (P::bdChord);
    bd.fineCents = v (P::bdFine); bd.decaySec = v (P::bdDecay); bd.damping = v (P::bdDamping);
    bd.brightness = v (P::bdBright); bd.inharm = v (P::bdInharm); bd.spread = v (P::bdSpread);
    bd.modes = i (P::bdModes); bd.excite = (dsp::ExciteMode) i (P::bdExcite); bd.glideMs = v (P::bdGlide);
    bd.fill = v (P::fill);
    bd.slosh = liquidOn ? v (P::lqSlosh) : 0.0f;
    bd.turbulence = v (P::lqTurb);
    bd.sloshPhase = phase;
    bd.seal = b (P::seal) ? 1.0f : 0.0f;
    bd.mix = v (P::bdMix);
    bd.seed = seed;
    chain.bodyE.setParams (bd, len);

    dsp::FractureParams fr;
    fr.crack = v (P::frCrack); fr.grit = v (P::frGrit); fr.fold = v (P::frFold); fr.shard = v (P::frShard);
    fr.tone = v (P::frTone); fr.mix = v (P::frMix); fr.seed = seed;
    fr.shardHz = chain.bodyE.fundamentalHz();
    chain.fractureE.setParams (fr);

    dsp::LiquidParams lq;
    lq.ripple = v (P::lqRipple); lq.turbulence = v (P::lqTurb); lq.mix = v (P::lqMix);
    lq.phase = phase; lq.phaseInc = inc; lq.seed = seed;
    chain.liquidE.setParams (lq);

    dsp::EchoParams ec;
    ec.timeMs = b (P::ecSync) ? (float) std::min (2000.0, 60000.0 / t.bpm * dsp::divisionBeats (i (P::ecDiv))) : v (P::ecTime);
    ec.feedback = v (P::ecFeedback); ec.pingPong = b (P::ecPingPong); ec.toneHz = v (P::ecTone);
    ec.wobble = v (P::ecWobble); ec.mix = v (P::ecMix);
    chain.echoE.setParams (ec);

    dsp::ReverbParams sp;
    sp.mode = (dsp::ReverbMode) i (P::spMode);
    sp.size = v (P::spSize); sp.decaySec = v (P::spDecay); sp.predelayMs = v (P::spPredelay); sp.dampHz = v (P::spDamp);
    sp.lowCutHz = v (P::spLowCut); sp.shimmer = v (P::spShimmer); sp.modulation = v (P::spMod);
    sp.freeze = b (P::seal) && b (P::sealSpace);
    sp.mix = v (P::spMix);
    chain.spaceE.setParams (sp);

    dsp::OutputParams op;
    op.width = v (P::width); op.bassMonoHz = v (P::bassMono); op.mix = v (P::mix);
    op.delta = b (P::delta); op.outGain = Decibels::decibelsToGain (v (P::outGain)); op.limiter = b (P::limiter);
    output.setParams (op);
}

// ---------------------------------------------------------------------------
void VesselProcessor::processBlock (AudioBuffer<float>& buffer, MidiBuffer&)
{
    ScopedNoDenormals noDenormals;
    const int numIn = getTotalNumInputChannels(), numOut = getTotalNumOutputChannels();
    const int n = buffer.getNumSamples();
    for (int ch = numIn; ch < numOut; ++ch) buffer.clear (ch, 0, n);
    if (n == 0 || numOut == 0) return;
    if (killRequested.exchange (false)) { chain.reset(); output.reset(); }

    // Tempo and position. When the host is stopped (or sends nothing, as in
    // the standalone app) a free-running clock keeps synced effects moving.
    double bpm = raw[(size_t) idx (P::tempo)]->load();
    bool playing = false;
    double hostPpq = 0.0;
    bool hasPpq = false;
    if (auto* ph = getPlayHead())
        if (auto pos = ph->getPosition())
        {
            if (auto bp = pos->getBpm()) bpm = *bp;
            playing = pos->getIsPlaying();
            if (auto p = pos->getPpqPosition()) { hostPpq = *p; hasPpq = true; }
        }
    bpm = jlimit (20.0, 999.0, bpm);
    uiBpm.store ((float) bpm, std::memory_order_relaxed);

    dsp::Transport t;
    t.bpm = bpm;
    t.sampleRate = sampleRateHz;
    t.playing = playing && hasPpq;
    t.ppq = t.playing ? hostPpq : freePpq;

    // Mono runs as dual-mono through the second scratch channel.
    if (dry.getNumSamples() < n) dry.setSize (2, n, false, false, true);   // host exceeded its block size
    float* L = buffer.getWritePointer (0);
    float* R = numOut >= 2 ? buffer.getWritePointer (1) : nullptr;

    inGainS.setTarget (Decibels::decibelsToGain (raw[(size_t) idx (P::inGain)]->load()));
    float inPeakBlock = 0.0f, outPeak = 0.0f, grMin = 1.0f;

    for (int start = 0; start < n; start += kControlBlock)
    {
        const int len = std::min (kControlBlock, n - start);
        float monoR[kControlBlock];
        float* l = L + start;
        float* r = R != nullptr ? R + start : monoR;
        if (R == nullptr) std::copy (l, l + len, monoR);

        // Input gain, then keep the dry reference for Mix / Delta.
        float pk = 0.0f;
        float* dl = dry.getWritePointer (0, start);
        float* dr = dry.getWritePointer (1, start);
        for (int k = 0; k < len; ++k)
        {
            const float g = inGainS.next();
            l[k] *= g; r[k] *= g;
            dl[k] = l[k]; dr[k] = r[k];
            pk = std::max ({ pk, std::abs (l[k]), std::abs (r[k]) });
        }
        inPeakBlock = std::max (inPeakBlock, pk);

        runControl (len, pk, t);
        chain.process (l, r, len, t);
        output.process (dl, dr, l, r, len);
        grMin = std::min (grMin, output.gainReduction());
        t.ppq += t.beatsPerSample() * len;

        for (int k = 0; k < len; ++k) outPeak = std::max ({ outPeak, std::abs (l[k]), std::abs (r[k]) });
        if (R == nullptr)
            for (int k = 0; k < len; ++k) l[k] = 0.5f * (l[k] + r[k]);
    }
    freePpq = t.ppq;

    uiInPeak.store (inPeakBlock, std::memory_order_relaxed);
    uiOutPeak.store (outPeak, std::memory_order_relaxed);
    uiGainReduction.store (grMin, std::memory_order_relaxed);
    uiSeal.store (eff[(size_t) idx (P::seal)], std::memory_order_relaxed);
    uiBodyLevel.store (chain.bodyE.level(), std::memory_order_relaxed);
    uiFundamental.store (chain.bodyE.fundamentalHz(), std::memory_order_relaxed);
    for (int k = 0; k < dsp::kMaxModes; ++k)
    {
        uiModeHz[(size_t) k].store (chain.bodyE.modeHz (k), std::memory_order_relaxed);
        uiModeLevel[(size_t) k].store (chain.bodyE.modeLevel (k), std::memory_order_relaxed);
    }
}

} // namespace vessel

// Plugin entry point.
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new vessel::VesselProcessor();
}
