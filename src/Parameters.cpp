#include "Parameters.h"
#include "dsp/Materials.h"
#include "dsp/Module.h"
#include "presets/FactoryPresets.h"

#include <unordered_map>

namespace vessel
{

namespace Choices
{
    const juce::StringArray materials = []
    {
        juce::StringArray s;
        for (const auto& m : dsp::materials()) s.add (m.name);
        return s;
    }();
    const juce::StringArray tuneModes   { "Free", "Note", "Chord" };
    const juce::StringArray notes       { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" };
    const juce::StringArray chords = []
    {
        juce::StringArray s;
        for (const auto& c : dsp::chords()) s.add (c.name);
        return s;
    }();
    const juce::StringArray excite      { "Input", "Transients", "Noise" };
    const juce::StringArray fracturePos { "Post-Body", "Pre-Body" };
    const juce::StringArray divisions = []
    {
        juce::StringArray s;
        for (const auto& d : dsp::kDivisions) s.add (d.name);
        return s;
    }();
    const juce::StringArray spaceModes  { "Hall", "Plate", "Reverse", "Gated" };
    const juce::StringArray lfoShapes   { "Sine", "Triangle", "Saw Up", "Saw Down", "Square", "S&H", "Smooth Rnd" };
}

namespace
{
using U = Unit;

ParamDef F (P p, const char* id, const char* name, const char* label, float lo, float hi, float d, float centre,
            Unit u, bool mod, const char* help)
{
    return { p, id, name, label, Kind::flt, lo, hi, d, centre, u, mod, nullptr, help };
}
ParamDef B (P p, const char* id, const char* name, const char* label, bool d, const char* help)
{
    return { p, id, name, label, Kind::boolean, 0, 1, d ? 1.0f : 0.0f, 0, U::plain, false, nullptr, help };
}
ParamDef C (P p, const char* id, const char* name, const char* label, const juce::StringArray& c, int d, const char* help)
{
    return { p, id, name, label, Kind::choice, 0, (float) (c.size() - 1), (float) d, 0, U::plain, false, &c, help };
}
ParamDef I (P p, const char* id, const char* name, const char* label, int lo, int hi, int d, const char* help)
{
    return { p, id, name, label, Kind::integer, (float) lo, (float) hi, (float) d, 0, U::plain, false, nullptr, help };
}

const std::vector<ParamDef>& table()
{
    using namespace Choices;
    static const std::vector<ParamDef> t = {
        // Global ---------------------------------------------------------------------------------------
        F (P::inGain,  "in_gain",  "Input",  "Input",  -24, 24, 0, 0, U::db, false, "Level going into Vessel"),
        F (P::outGain, "out_gain", "Output", "Output", -24, 24, 0, 0, U::db, false, "Final output level"),
        F (P::mix,     "mix",      "Mix",    "Mix",    0, 1, 1, 0, U::pct, true, "Blend of the original sound and the vessel"),
        B (P::delta,   "delta",    "Delta Listen", "Delta", false, "Hear only what Vessel adds (the difference)"),
        B (P::limiter, "limiter",  "Safety Limiter", "Limiter", true, "Catches peaks at -0.3 dBFS so nothing can blow up"),
        F (P::fill,    "fill",     "Fill", "Fill", 0, 1, 0, 0, U::pct, true, "Water in the vessel: lowers every resonance like a filled glass and softens the highs"),
        B (P::seal,    "seal",     "Seal", "Seal", false, "Seals the vessel: what is ringing keeps ringing, nothing new gets in"),
        B (P::sealSpace, "seal_space", "Seal Space Too", "Seal Space", false, "Seal also freezes the Space tail"),
        B (P::shatter, "shatter",  "Shatter", "Shatter", false, "Ramps the vessel into pieces: cracks, grit, warped modes and thrown echoes"),
        F (P::shatterTime,  "shatter_time",  "Shatter Time", "Ramp", 0.02f, 8.0f, 0.5f, 1.0f, U::sec, false, "How long Shatter takes to break the vessel"),
        F (P::shatterDepth, "shatter_depth", "Shatter Depth", "Depth", 0, 1, 0.8f, 0, U::pct, false, "How badly Shatter breaks it"),
        F (P::macro1, "macro1", "Macro 1 Resonance", "Resonance", 0, 1, 0, 0, U::pct, false, "How much the vessel sings: body level and ring time"),
        F (P::macro2, "macro2", "Macro 2 Motion",    "Motion",    0, 1, 0, 0, U::pct, false, "Liquid movement: slosh, ripple and modulation"),
        F (P::macro3, "macro3", "Macro 3 Fracture",  "Fracture",  0, 1, 0, 0, U::pct, false, "Cracks, grit and folding"),
        F (P::macro4, "macro4", "Macro 4 Space",     "Space",     0, 1, 0, 0, U::pct, false, "Echoes, room and width"),
        F (P::tempo,   "tempo", "Free Tempo", "Tempo", 40, 240, 120, 0, U::bpm, false, "Tempo used when the host does not send one (standalone)"),
        I (P::seed,    "seed", "Random Seed", "Seed", 1, 9999, 1, "Same seed = same cracks, currents and random patterns every time"),
        F (P::width,   "width", "Width", "Width", 0, 2, 1, 1, U::pct, true, "Stereo width of the processed sound (100% = unchanged)"),
        F (P::bassMono, "bass_mono", "Bass Mono", "Bass Mono", 20, 500, 20, 120, U::hz, false, "Keeps everything below this frequency mono (20 Hz = off)"),

        // Pour -----------------------------------------------------------------------------------------
        B (P::poOn,     "po_on",     "Pour On", "Pour", true, "Conditions the sound going into the vessel"),
        F (P::poDrive,  "po_drive",  "Pour Drive", "Drive", 0, 1, 0, 0, U::pct, true, "Warm saturation before the body (auto-levelled)"),
        F (P::poStrike, "po_strike", "Pour Strike", "Strike", -1, 1, 0, 0, U::pctBipolar, true, "Transient shaper: + sharper attacks ring the body harder, - softer"),
        F (P::poTilt,   "po_tilt",   "Pour Tilt", "Tilt", -1, 1, 0, 0, U::pctBipolar, true, "Dark to bright, before the body"),
        F (P::poLowCut, "po_lowcut", "Pour Low Cut", "Low Cut", 20, 1000, 20, 150, U::hz, true, "Removes lows before the body (20 Hz = off)"),

        // Body -----------------------------------------------------------------------------------------
        B (P::bdOn,     "bd_on",     "Body On", "Body", true, "The vessel: up to 16 tuned resonant modes"),
        F (P::bdMix,    "bd_mix",    "Body Mix", "Body Mix", 0, 1, 0.6f, 0, U::pct, true, "Blend of the poured sound and the singing body"),
        C (P::bdMaterial, "bd_material", "Material", "Material", materials, 0, "What the vessel is made of: where its modes sit and how it rings"),
        C (P::bdTune,   "bd_tune",   "Tuning", "Tuning", tuneModes, 1, "Free: any pitch. Note: tuned to a note. Chord: modes re-tuned to a chord"),
        F (P::bdPitch,  "bd_pitch",  "Free Pitch", "Pitch", 30, 2000, 220, 220, U::hz, true, "Fundamental in Free tuning"),
        C (P::bdRoot,   "bd_root",   "Root Note", "Root", notes, 9, "Root note in Note and Chord tuning (match your track's key)"),
        I (P::bdOctave, "bd_octave", "Octave", "Octave", 1, 6, 3, "Octave of the root note"),
        C (P::bdChord,  "bd_chord",  "Chord", "Chord", chords, 2, "Chord the modes are tuned to in Chord tuning"),
        F (P::bdFine,   "bd_fine",   "Fine Tune", "Fine", -100, 100, 0, 0, U::cents, true, "Fine tuning (for tracks not at A = 440 Hz)"),
        F (P::bdDecay,  "bd_decay",  "Body Decay", "Decay", 0.05f, 20, 2.0f, 2.0f, U::sec, true, "How long the vessel rings"),
        F (P::bdDamping, "bd_damping", "Body Damping", "Damping", 0, 1, 0.3f, 0, U::pct, true, "How much faster the high modes die than the low ones"),
        F (P::bdBright, "bd_bright", "Body Brightness", "Bright", -1, 1, 0, 0, U::pctBipolar, true, "Balance of low and high modes"),
        F (P::bdInharm, "bd_inharm", "Body Warp", "Warp", -1, 1, 0, 0, U::pctBipolar, true, "Squeezes (-) or stretches (+) the mode spacing: from hollow to clangorous"),
        F (P::bdSpread, "bd_spread", "Body Spread", "Spread", 0, 1, 0.35f, 0, U::pct, true, "Left and right sides tuned slightly apart: width and slow beating"),
        I (P::bdModes,  "bd_modes",  "Modes", "Modes", 4, 16, 16, "How many modes ring: fewer is purer, more is richer"),
        C (P::bdExcite, "bd_excite", "Excite", "Excite", excite, 0, "What rings the body: the whole Input, only Transients (hits ping it), or Noise shaped by the input"),
        F (P::bdGlide,  "bd_glide",  "Glide", "Glide", 0, 2000, 40, 200, U::ms, false, "Pitch glide when the tuning changes"),

        // Fracture -------------------------------------------------------------------------------------
        B (P::frOn,    "fr_on",    "Fracture On", "Fracture", false, "Cracks, grit, folding and shards"),
        F (P::frMix,   "fr_mix",   "Fracture Mix", "Mix", 0, 1, 1, 0, U::pct, true, "Blend of whole and fractured"),
        C (P::frPos,   "fr_pos",   "Fracture Position", "Position", fracturePos, 0, "Post-Body cracks the ringing vessel; Pre-Body cracks what rings it"),
        F (P::frCrack, "fr_crack", "Crack", "Crack", 0, 1, 0.3f, 0, U::pct, true, "Glassy cracks triggered by the sound; above 50% pieces break away"),
        F (P::frGrit,  "fr_grit",  "Grit", "Grit", 0, 1, 0, 0, U::pct, true, "Bit and sample-rate reduction"),
        F (P::frFold,  "fr_fold",  "Fold", "Fold", 0, 1, 0.2f, 0, U::pct, true, "Wavefolding: glassy, then harsh"),
        F (P::frShard, "fr_shard", "Shard", "Shard", 0, 1, 0, 0, U::pct, true, "Metallic ring modulation tuned to the vessel"),
        F (P::frTone,  "fr_tone",  "Fracture Tone", "Tone", -1, 1, 0, 0, U::pctBipolar, true, "Dark to bright, after the damage"),

        // Liquid ---------------------------------------------------------------------------------------
        B (P::lqOn,    "lq_on",    "Liquid On", "Liquid", false, "The water moving inside: slosh and ripple"),
        F (P::lqMix,   "lq_mix",   "Ripple Mix", "Mix", 0, 1, 0.5f, 0, U::pct, true, "Blend of still and rippling sound"),
        F (P::lqRate,  "lq_rate",  "Liquid Rate", "Rate", 0.02f, 12, 0.35f, 1.0f, U::hz, true, "Speed of the water"),
        B (P::lqSync,  "lq_sync",  "Liquid Sync", "Sync", false, "Lock the water's movement to the host tempo"),
        C (P::lqDiv,   "lq_div",   "Liquid Division", "Rate", divisions, dsp::Div::bar1, "Synced speed"),
        F (P::lqSlosh, "lq_slosh", "Slosh", "Slosh", 0, 1, 0.4f, 0, U::pct, true, "Every mode of the body wobbles in pitch on its own phase"),
        F (P::lqRipple, "lq_ripple", "Ripple", "Ripple", 0, 1, 0.4f, 0, U::pct, true, "Liquid chorus depth"),
        F (P::lqTurb,  "lq_turb",  "Turbulence", "Turbulence", 0, 1, 0, 0, U::pct, true, "Smooth waves become random currents"),

        // Echo -----------------------------------------------------------------------------------------
        B (P::ecOn,     "ec_on",     "Echo On", "Echo", false, "Tape-flavoured echoes that can ring the vessel again"),
        F (P::ecMix,    "ec_mix",    "Echo Mix", "Mix", 0, 1, 0.3f, 0, U::pct, true, "Level of the echoes"),
        F (P::ecTime,   "ec_time",   "Echo Time", "Time", 10, 2000, 375, 300, U::ms, true, "Time between repeats"),
        B (P::ecSync,   "ec_sync",   "Echo Sync", "Sync", true, "Lock the echo time to the host tempo"),
        C (P::ecDiv,    "ec_div",    "Echo Division", "Time", divisions, dsp::Div::d8D, "Synced echo time"),
        F (P::ecFeedback, "ec_feedback", "Echo Feedback", "Feedback", 0, 0.95f, 0.35f, 0, U::pct, true, "How many repeats"),
        B (P::ecPingPong, "ec_pingpong", "Ping-Pong", "Ping-Pong", false, "Repeats bounce left and right"),
        F (P::ecTone,   "ec_tone",   "Echo Tone", "Tone", 500, 20000, 6000, 4000, U::hz, true, "Each repeat gets darker"),
        F (P::ecWobble, "ec_wobble", "Echo Wobble", "Wobble", 0, 1, 0.1f, 0, U::pct, true, "Tape wow on the repeats"),
        F (P::ecReturn, "ec_return", "Echo Return", "Return", 0, 1, 0, 0, U::pct, true, "Sends the echoes back into the vessel: every repeat rings it again"),

        // Space ----------------------------------------------------------------------------------------
        B (P::spOn,     "sp_on",     "Space On", "Space", false, "The room around the vessel"),
        F (P::spMix,    "sp_mix",    "Space Mix", "Mix", 0, 1, 0.25f, 0, U::pct, true, "Blend of dry and room"),
        C (P::spMode,   "sp_mode",   "Space Mode", "Mode", spaceModes, 0, "Hall, Plate, Reverse (sucked-in swells) or Gated"),
        F (P::spSize,   "sp_size",   "Space Size", "Size", 0, 1, 0.6f, 0, U::pct, true, "Size of the room"),
        F (P::spDecay,  "sp_decay",  "Space Decay", "Decay", 0.2f, 20, 3, 3, U::sec, true, "Tail length"),
        F (P::spPredelay, "sp_predelay", "Space Pre-Delay", "Pre-Delay", 0, 250, 20, 50, U::ms, true, "Gap before the room answers"),
        F (P::spDamp,   "sp_damp",   "Space Damping", "Damping", 1000, 20000, 7000, 5000, U::hz, true, "Darkens the tail"),
        F (P::spLowCut, "sp_lowcut", "Space Low Cut", "Low Cut", 20, 1000, 120, 150, U::hz, true, "Keeps the room out of the bass"),
        F (P::spShimmer, "sp_shimmer", "Space Shimmer", "Shimmer", 0, 1, 0, 0, U::pct, true, "An octave-up halo rising out of the tail"),
        F (P::spMod,    "sp_mod",    "Space Movement", "Movement", 0, 1, 0.3f, 0, U::pct, true, "Slow movement inside the tail"),

        // Modulation sources ---------------------------------------------------------------------------
        C (P::lfo1Shape, "lfo1_shape", "LFO 1 Shape", "Shape", lfoShapes, 0, "LFO 1 waveform"),
        F (P::lfo1Rate,  "lfo1_rate",  "LFO 1 Rate", "Rate", 0.01f, 20, 0.5f, 1.0f, U::hz, true, "LFO 1 speed"),
        B (P::lfo1Sync,  "lfo1_sync",  "LFO 1 Sync", "Sync", false, "Lock LFO 1 to the host tempo"),
        C (P::lfo1Div,   "lfo1_div",   "LFO 1 Division", "Rate", divisions, dsp::Div::bar1, "LFO 1 synced speed"),
        C (P::lfo2Shape, "lfo2_shape", "LFO 2 Shape", "Shape", lfoShapes, 6, "LFO 2 waveform"),
        F (P::lfo2Rate,  "lfo2_rate",  "LFO 2 Rate", "Rate", 0.01f, 20, 2.0f, 1.0f, U::hz, true, "LFO 2 speed"),
        B (P::lfo2Sync,  "lfo2_sync",  "LFO 2 Sync", "Sync", true, "Lock LFO 2 to the host tempo"),
        C (P::lfo2Div,   "lfo2_div",   "LFO 2 Division", "Rate", divisions, dsp::Div::d8, "LFO 2 synced speed"),
        F (P::envAttack,  "env_attack",  "Envelope Attack", "Attack", 0.5f, 500, 10, 30, U::ms, false, "How fast the envelope follows rising level"),
        F (P::envRelease, "env_release", "Envelope Release", "Release", 10, 3000, 250, 300, U::ms, false, "How fast it falls back"),
        F (P::envGain,    "env_gain",    "Envelope Sensitivity", "Sensitivity", -12, 24, 6, 0, U::db, false, "Boosts the envelope for quiet sources"),
        F (P::driftRate,  "drift_rate",  "Drift Rate", "Rate", 0.02f, 20, 0.5f, 1.0f, U::hz, true, "How often Drift picks a new random value"),
        F (P::driftSmooth, "drift_smooth", "Drift Smooth", "Smooth", 0, 1, 0.7f, 0, U::pct, false, "Stepped (0%) to gliding (100%)"),
    };
    return t;
}

const struct TableCheck
{
    TableCheck()
    {
        const auto& t = table();
        jassert ((int) t.size() == kNumParams);
        for (int i = 0; i < (int) t.size(); ++i)
            jassert ((int) t[(size_t) i].p == i);   // table order must match enum P
    }
} tableCheck;
} // namespace

const ParamDef& def (P p) { return table()[(size_t) p]; }

int findParam (const juce::String& id)
{
    static const auto map = []
    {
        std::unordered_map<std::string, int> m;
        for (int i = 0; i < kNumParams; ++i) m[def (i).id] = i;
        return m;
    }();
    const auto it = map.find (id.toStdString());
    return it == map.end() ? -1 : it->second;
}

namespace
{
// Formats v with fine decimals below `limit` and coarse decimals above it.
// The choice is made on the value as it will be printed (rounded), so that
// value -> text -> value -> text is stable (hosts and clap-validator check).
juce::String fmtSplit (float v, float limit, int fineDecimals, int coarseDecimals)
{
    const float scale = std::pow (10.0f, (float) fineDecimals);
    const float shown = std::round (v * scale) / scale;
    return std::abs (shown) < limit ? juce::String (v, fineDecimals) : juce::String (v, coarseDecimals);
}
} // namespace

juce::String formatValue (const ParamDef& d, float v)
{
    using juce::String;
    switch (d.kind)
    {
        case Kind::boolean: return v > 0.5f ? "On" : "Off";
        case Kind::choice:  return (*d.choices)[juce::jlimit (0, d.choices->size() - 1, juce::roundToInt (v))];
        case Kind::integer: return String (juce::roundToInt (v));
        case Kind::flt:     break;
    }
    switch (d.unit)
    {
        case Unit::pct:        return String (juce::roundToInt (v * 100.0f)) + "%";
        case Unit::pctBipolar: { const int k = juce::roundToInt (v * 100.0f); return (k > 0 ? "+" : "") + String (k) + "%"; }
        case Unit::db:         return (v > 0.05f ? "+" : "") + String (v, 1) + " dB";
        case Unit::hz:
            if (std::abs (std::round (v)) >= 1000.0f) return String (v / 1000.0f, 2) + " kHz";
            return fmtSplit (v, 10.0f, 2, 0) + " Hz";
        case Unit::ms:         return fmtSplit (v, 10.0f, 1, 0) + " ms";
        case Unit::sec:        return fmtSplit (v, 10.0f, 2, 1) + " s";
        case Unit::semis:      { const float r = std::round (v * 10.0f) / 10.0f; return (r > 0 ? "+" : "") + String (r, 1) + " st"; }
        case Unit::cents:      return (v > 0.5f ? "+" : "") + String (juce::roundToInt (v)) + " ct";
        case Unit::bpm:        return String (v, 1) + " bpm";
        case Unit::plain:      return fmtSplit (v, 10.0f, 1, 0);
    }
    return String (v);
}

// The default each parameter reports to the host: its value in the Init preset,
// which is what the plugin loads on startup. Hosts (and clap-validator) expect a
// fresh instance to sit at its reported defaults.
float hostDefault (int i)
{
    for (const auto& [p, val] : factoryPresets().front().values)
        if (idx (p) == i) return val;
    return def (i).def;
}

juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout()
{
    using namespace juce;
    AudioProcessorValueTreeState::ParameterLayout layout;

    for (int i = 0; i < kNumParams; ++i)
    {
        const auto& d = def (i);
        const float dflt = hostDefault (i);
        const ParameterID pid { d.id, 1 };
        switch (d.kind)
        {
            case Kind::flt:
            {
                NormalisableRange<float> r (d.min, d.max);
                if (d.centre != 0.0f && d.centre > d.min && d.centre < d.max) r.setSkewForCentre (d.centre);
                auto attrs = AudioParameterFloatAttributes()
                                 .withStringFromValueFunction ([&d] (float v, int) { return formatValue (d, v); })
                                 .withValueFromStringFunction ([&d] (const String& s)
                                 {
                                     const float x = s.getFloatValue();
                                     if (d.unit == Unit::pct || d.unit == Unit::pctBipolar) return x / 100.0f;
                                     if (d.unit == Unit::hz && s.containsIgnoreCase ("k")) return x * 1000.0f;
                                     return x;
                                 });
                layout.add (std::make_unique<AudioParameterFloat> (pid, d.name, r, dflt, attrs));
                break;
            }
            case Kind::boolean:
                layout.add (std::make_unique<AudioParameterBool> (pid, d.name, dflt > 0.5f));
                break;
            case Kind::choice:
                layout.add (std::make_unique<AudioParameterChoice> (pid, d.name, *d.choices, (int) dflt));
                break;
            case Kind::integer:
                layout.add (std::make_unique<AudioParameterInt> (pid, d.name, (int) d.min, (int) d.max, (int) dflt));
                break;
        }
    }
    return layout;
}

// ---------------------------------------------------------------------------
const StageInfo kStages[kNumStages] = {
    { "Pour",     "Drive, strike and tilt into the vessel",  P::poOn, P::poOn,  P::poDrive,    P::poLowCut, false },
    { "Body",     "The tuned, singing resonant body",        P::bdOn, P::bdMix, P::bdMaterial, P::bdGlide,  true },
    { "Fracture", "Cracks, grit, folding and shards",        P::frOn, P::frMix, P::frPos,      P::frTone,   true },
    { "Liquid",   "Slosh and ripple: the water moving",      P::lqOn, P::lqMix, P::lqRate,     P::lqTurb,   true },
    { "Echo",     "Echoes that can ring the vessel again",   P::ecOn, P::ecMix, P::ecTime,     P::ecReturn, true },
    { "Space",    "The room around the vessel",              P::spOn, P::spMix, P::spMode,     P::spMod,    true },
};

const juce::StringArray& sourceNames()
{
    static const juce::StringArray s { "None", "LFO 1", "LFO 2", "Envelope", "Drift",
                                       "Macro 1", "Macro 2", "Macro 3", "Macro 4" };
    return s;
}

} // namespace vessel
