#pragma once
// Vessel's audio processor: reads parameters, runs the modulation system,
// Shatter and the macros every 32-sample control block, then drives the
// chain (Pour -> Body -> Fracture -> Liquid -> Echo -> Space) and the output.
//
// Threading: patch data that is not a plain parameter (the mod matrix) lives
// in the APVTS ValueTree (saved with the project, undoable) and is edited on
// the message thread only. Every change is copied into lock-free atomics,
// which are all the audio thread ever reads.

#include "Parameters.h"
#include "dsp/Chain.h"
#include "dsp/ModSources.h"
#include "dsp/OutputStage.h"
#include "presets/PresetManager.h"
#include "state/StateGuard.h"

#include <juce_audio_processors/juce_audio_processors.h>

namespace vessel
{

struct MatrixSlot
{
    Src src = Src::none;
    int dst = -1;          // parameter index, -1 = empty
    float amount = 0.0f;   // -1..1 of the destination's full range
    bool isActive() const noexcept { return src != Src::none && dst >= 0 && amount != 0.0f; }
};

class VesselProcessor : public juce::AudioProcessor, private juce::ValueTree::Listener
{
public:
    static constexpr int kControlBlock = 32;

    VesselProcessor();
    ~VesselProcessor() override;

    // Lifecycle -------------------------------------------------------------------------
    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    void reset() override;
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    using AudioProcessor::processBlock;

    // Editor / info ---------------------------------------------------------------------
    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }
    const juce::String getName() const override { return JucePlugin_Name; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 20.0; }   // long bodies, echoes, rooms

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return "Default"; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    // Patch data (message thread) -------------------------------------------------------
    juce::UndoManager undo;
    juce::AudioProcessorValueTreeState apvts;
    mutable lowkei::StateGuard stateGuard { apvts };   // host save/load off the message thread

    MatrixSlot getSlot (int i) const;
    void setSlot (int i, const MatrixSlot& s);
    void clearMatrix();
    int addRoute (Src src, int dst, float amount);   // -1 if the matrix is full
    void removeRoutesTo (int dst);
    juce::Array<int> routesTo (int dst) const;

    juce::ValueTree getPatchState() const;
    void applyPatchState (const juce::ValueTree& tree, bool notifyHost);

    PresetManager presets { *this };


    // Panic: clears every tail, buffer and the echo return on the next block.
    void requestKill() noexcept { killRequested.store (true); }

    // Live values for the UI (written by the audio thread, read at 30 fps) ---------------
    std::array<std::atomic<float>, kNumParams> uiNorm {};   // after modulation and Shatter, 0..1
    std::atomic<float> uiLfo[2] {}, uiEnv { 0.0f }, uiDrift { 0.0f }, uiShatter { 0.0f }, uiSeal { 0.0f };
    std::atomic<float> uiInPeak { 0.0f }, uiOutPeak { 0.0f }, uiGainReduction { 1.0f }, uiBpm { 120.0f };
    std::atomic<float> uiSloshPhase { 0.0f }, uiBodyLevel { 0.0f }, uiFundamental { 220.0f };
    std::array<std::atomic<float>, dsp::kMaxModes> uiModeHz {}, uiModeLevel {};
    std::array<std::atomic<bool>, kNumStages> uiStageOn {};

    // State property names.
    static constexpr const char* kPresetNameProp = "presetName";
    static constexpr const char* kMatrixType = "MATRIX";

    // For tests / tools: the effective value of a parameter after modulation and Shatter.
    float effectiveValue (P p) const noexcept { return eff[(size_t) idx (p)]; }
    const dsp::Chain& getChain() const noexcept { return chain; }

private:
    void valueTreePropertyChanged (juce::ValueTree& t, const juce::Identifier&) override
    {
        if (t.hasType ("SLOT")) refreshFromState();
    }
    void valueTreeChildAdded (juce::ValueTree&, juce::ValueTree&) override { refreshFromState(); }
    void valueTreeChildRemoved (juce::ValueTree&, juce::ValueTree&, int) override { refreshFromState(); }
    void valueTreeRedirected (juce::ValueTree&) override { refreshFromState(); }

    void ensureStateStructure();
    void refreshFromState() { refreshFrom (apvts.state); }
    void refreshFrom (const juce::ValueTree& st);   // the live tree, or a state being loaded
    juce::ValueTree matrixTree() const;
    void setParamReal (int i, float real);

    void runControl (int len, float inPeak, const dsp::Transport& t);
    void pushEngineParams (const dsp::Transport& t, int len);

    struct SlotAtomic
    {
        std::atomic<int> src { 0 }, dst { -1 };
        std::atomic<float> amount { 0.0f };
    };
    std::array<SlotAtomic, kMatrixSlots> slotsA;
    std::atomic<bool> killRequested { false };

    // Parameter access (looked up once).
    std::array<std::atomic<float>*, kNumParams> raw {};
    std::array<juce::NormalisableRange<float>, kNumParams> ranges;

    // Audio-thread working state.
    std::array<float, kNumParams> eff {};      // effective real values for this block
    std::array<float, kNumParams> norm {};     // effective normalised values
    dsp::Chain chain;
    dsp::OutputStage output;
    dsp::ModLfo lfos[2];
    dsp::EnvFollower env;
    dsp::Chaos drift;
    float srcVal[kNumSources] {};
    float shatterRamp = 0.0f;
    double freePpq = 0.0;
    double sampleRateHz = 48000.0;
    juce::AudioBuffer<float> dry;
    dsp::Smoother inGainS;

    JUCE_DECLARE_WEAK_REFERENCEABLE (VesselProcessor)
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (VesselProcessor)
};

} // namespace vessel
