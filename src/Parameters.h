#pragma once
// Every automatable parameter, defined once in a table (Parameters.cpp):
// ID, display name, range, default, unit and whether the mod matrix can
// reach it. IDs are permanent once shipped (DAW projects and presets store
// them), so rename the display name, never the ID.

#include <juce_audio_processors/juce_audio_processors.h>

namespace vessel
{

// Order matches the table in Parameters.cpp (checked at startup).
enum class P : int
{
    // Global
    inGain, outGain, mix, delta, limiter, fill, seal, sealSpace, shatter, shatterTime, shatterDepth,
    macro1, macro2, macro3, macro4, tempo, seed, width, bassMono,
    // Pour
    poOn, poDrive, poStrike, poTilt, poLowCut,
    // Body
    bdOn, bdMix, bdMaterial, bdTune, bdPitch, bdRoot, bdOctave, bdChord, bdFine, bdDecay, bdDamping,
    bdBright, bdInharm, bdSpread, bdModes, bdExcite, bdGlide,
    // Fracture
    frOn, frMix, frPos, frCrack, frGrit, frFold, frShard, frTone,
    // Liquid
    lqOn, lqMix, lqRate, lqSync, lqDiv, lqSlosh, lqRipple, lqTurb,
    // Echo
    ecOn, ecMix, ecTime, ecSync, ecDiv, ecFeedback, ecPingPong, ecTone, ecWobble, ecReturn,
    // Space
    spOn, spMix, spMode, spSize, spDecay, spPredelay, spDamp, spLowCut, spShimmer, spMod,
    // Modulation sources
    lfo1Shape, lfo1Rate, lfo1Sync, lfo1Div,
    lfo2Shape, lfo2Rate, lfo2Sync, lfo2Div,
    envAttack, envRelease, envGain,
    driftRate, driftSmooth,
    count
};

inline constexpr int kNumParams = (int) P::count;
inline constexpr int idx (P p) noexcept { return (int) p; }

enum class Kind { flt, boolean, choice, integer };
enum class Unit { plain, pct, pctBipolar, db, hz, ms, sec, semis, cents, bpm };

struct ParamDef
{
    P p;
    const char* id;
    const char* name;       // full name (DAW automation lane)
    const char* label;      // short knob legend
    Kind kind;
    float min, max, def;
    float centre;           // skew so this value sits mid-knob (0 = linear)
    Unit unit;
    bool modulatable;       // may be a mod-matrix destination
    const juce::StringArray* choices = nullptr;
    const char* help = "";  // one-line description for tooltips / hint bar
};

const ParamDef& def (P p);
inline const ParamDef& def (int i) { return def ((P) i); }
int findParam (const juce::String& id);   // -1 if unknown

namespace Choices
{
    extern const juce::StringArray materials, tuneModes, notes, chords, excite, fracturePos, divisions,
                                   spaceModes, lfoShapes;
}

// Text shown for a value of a parameter (used by the host and the UI).
juce::String formatValue (const ParamDef& d, float value);

juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();

// ---------------------------------------------------------------------------
// The six stages: their power switch, mix (none for Pour) and controls.
struct StageInfo
{
    const char* name;
    const char* blurb;
    P on, mix, first, last;   // first..last = the stage's own controls
    bool hasMix;
};
inline constexpr int kNumStages = 6;
extern const StageInfo kStages[kNumStages];

// ---------------------------------------------------------------------------
// Modulation sources for the matrix.
enum class Src : int { none = 0, lfo1, lfo2, env, drift, macro1, macro2, macro3, macro4, count };
inline constexpr int kNumSources = (int) Src::count;
const juce::StringArray& sourceNames();
inline bool isBipolar (Src s) noexcept { return s == Src::lfo1 || s == Src::lfo2 || s == Src::drift; }

inline constexpr int kMatrixSlots = 16;
inline constexpr int kNumMacros = 4;
inline constexpr const char* kMacroNames[kNumMacros] = { "Resonance", "Motion", "Fracture", "Space" };

} // namespace vessel
