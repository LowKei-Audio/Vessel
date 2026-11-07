#pragma once
// Vessel materials and tunings.
//
// A material is the physical character of the body: where its resonant modes
// sit (ratios to the fundamental), how loud each mode is, how long the body
// rings and how fast the high modes die. Ratios come from the physics of each
// shape (wine glass shell modes, a singing bowl's beating doublets, a church
// bell's hum/prime/tierce/quint/nominal, a free metal plate, a drum membrane's
// Bessel zeros, a wooden box, a clay jar, a closed tube's odd harmonics).
//
// Tunings decide the fundamental (Free in Hz, a Note, or a Chord). In Chord
// mode the modes are re-tuned to chord tones stacked up the octaves, and the
// material still shapes level, decay and colour, so a bronze chord still
// sounds like bronze.

#include <algorithm>
#include <array>
#include <cmath>

namespace vessel::dsp
{

inline constexpr int kMaxModes = 16;
inline constexpr int kNumMaterials = 8;

struct Material
{
    const char* name;
    std::array<float, kMaxModes> ratio;
    float ampExp;      // mode level falls as (k+1)^-ampExp
    float decayMul;    // multiplies the Decay knob
    float hfDamp;      // high modes ring shorter: T_k = T * ratio_k^-hfDamp
    float spreadMul;   // natural stereo detune between the two sides
    float brightBias;  // -1..1 added to the Brightness knob
};

inline const std::array<Material, kNumMaterials>& materials()
{
    static const std::array<Material, kNumMaterials> m = { {
        { "Glass",   { 1.0f, 2.32f, 2.83f, 4.25f, 5.42f, 6.63f, 8.77f, 9.38f, 12.87f, 14.1f, 17.7f, 19.9f, 23.3f, 26.0f, 29.6f, 33.1f },
          0.55f, 1.0f, 0.35f, 1.0f, 0.25f },
        { "Crystal", { 1.0f, 1.004f, 2.71f, 2.716f, 5.12f, 5.131f, 8.31f, 8.328f, 12.1f, 12.13f, 16.6f, 16.64f, 21.7f, 21.75f, 27.4f, 27.47f },
          0.45f, 1.6f, 0.25f, 0.6f, 0.35f },
        { "Bronze",  { 0.5f, 1.0f, 1.183f, 1.506f, 2.0f, 2.514f, 2.662f, 3.011f, 4.166f, 5.433f, 6.796f, 8.215f, 9.67f, 11.2f, 12.8f, 14.4f },
          0.35f, 1.3f, 0.45f, 0.8f, 0.1f },
        { "Iron",    { 1.0f, 1.73f, 2.33f, 3.91f, 4.11f, 6.30f, 6.99f, 8.95f, 9.86f, 11.9f, 13.3f, 15.1f, 16.7f, 18.6f, 20.5f, 22.7f },
          0.4f, 0.8f, 0.4f, 1.2f, 0.2f },
        { "Wood",    { 1.0f, 1.48f, 2.02f, 2.51f, 3.03f, 3.62f, 4.05f, 4.61f, 5.12f, 5.66f, 6.1f, 6.73f, 7.2f, 7.9f, 8.3f, 9.0f },
          0.8f, 0.22f, 0.7f, 1.0f, -0.35f },
        { "Clay",    { 1.0f, 2.01f, 3.03f, 4.06f, 5.1f, 6.15f, 7.2f, 8.27f, 9.35f, 10.4f, 11.5f, 12.6f, 13.7f, 14.8f, 15.9f, 17.0f },
          1.0f, 0.4f, 0.6f, 0.7f, -0.45f },
        { "Tube",    { 1.0f, 3.0f, 5.0f, 7.0f, 9.0f, 11.0f, 13.0f, 15.0f, 17.0f, 19.0f, 21.0f, 23.0f, 25.0f, 27.0f, 29.0f, 31.0f },
          0.7f, 0.6f, 0.3f, 0.5f, 0.0f },
        { "Skin",    { 1.0f, 1.594f, 2.136f, 2.296f, 2.653f, 2.918f, 3.156f, 3.501f, 3.600f, 4.060f, 4.154f, 4.601f, 4.832f, 4.903f, 5.131f, 5.412f },
          0.5f, 0.12f, 0.9f, 1.4f, -0.2f },
    } };
    return m;
}

// Chords as semitone offsets from the root. Stacked up the octaves in Chord mode.
struct Chord { const char* name; int count; std::array<int, 5> semis; };
inline constexpr int kNumChords = 12;
inline const std::array<Chord, kNumChords>& chords()
{
    static const std::array<Chord, kNumChords> c = { {
        { "Octaves", 1, { 0 } },           { "Fifths", 2, { 0, 7 } },
        { "Major",   3, { 0, 4, 7 } },     { "Minor", 3, { 0, 3, 7 } },
        { "Sus2",    3, { 0, 2, 7 } },     { "Sus4", 3, { 0, 5, 7 } },
        { "Maj7",    4, { 0, 4, 7, 11 } }, { "Min7", 4, { 0, 3, 7, 10 } },
        { "Dom7",    4, { 0, 4, 7, 10 } }, { "Min9", 5, { 0, 3, 7, 10, 14 } },
        { "Add9",    4, { 0, 4, 7, 14 } }, { "Dim",  3, { 0, 3, 6 } },
    } };
    return c;
}

enum class TuneMode { free = 0, note, chord };

inline float midiToHz (float note) noexcept { return 440.0f * std::exp2 ((note - 69.0f) / 12.0f); }

// Frequency ratio of mode k relative to the fundamental, before warping.
inline float modeRatio (TuneMode mode, int material, int chord, int k) noexcept
{
    const auto& mat = materials()[(size_t) std::clamp (material, 0, kNumMaterials - 1)];
    if (mode != TuneMode::chord) return mat.ratio[(size_t) k];
    const auto& c = chords()[(size_t) std::clamp (chord, 0, kNumChords - 1)];
    const int semis = c.semis[(size_t) (k % c.count)] + 12 * (k / c.count);
    return std::exp2 ((float) semis / 12.0f);
}

} // namespace vessel::dsp
