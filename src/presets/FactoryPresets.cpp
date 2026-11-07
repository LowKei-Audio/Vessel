#include "FactoryPresets.h"
#include "../dsp/Module.h"

namespace vessel
{

// ---------------------------------------------------------------------------
// Standard macro kit. For each macro, the first matching candidates (in
// priority order) for the stages a preset uses become routes, so the four
// macros always do something meaningful:
//   1 Resonance - body level, ring time and width
//   2 Motion    - slosh, ripple and water speed (stereo movement when Liquid is off)
//   3 Fracture  - cracks, grit and folding (drive and warp when Fracture is off)
//   4 Space     - echoes, room and width
std::vector<Route> macroKit (const std::array<bool, kNumStages>& used)
{
    struct Cand { int stage; P dst; float amount; };   // stage -1 = always available
    auto pick = [&] (Src src, std::initializer_list<Cand> list, int maxRoutes)
    {
        std::vector<Route> r;
        for (const auto& c : list)
        {
            if ((int) r.size() >= maxRoutes) break;
            if (c.stage < 0 || used[(size_t) c.stage]) r.push_back ({ src, c.dst, c.amount });
        }
        return r;
    };

    std::vector<Route> all;
    auto add = [&] (std::vector<Route> r) { all.insert (all.end(), r.begin(), r.end()); };
    enum { pour = 0, body, fracture, liquid, echo, space };   // same order as kStages

    add (pick (Src::macro1, { { body, P::bdMix, 0.35f }, { body, P::bdDecay, 0.25f }, { body, P::bdSpread, 0.25f } }, 3));
    add (pick (Src::macro2, { { liquid, P::lqSlosh, 0.45f }, { liquid, P::lqRipple, 0.35f }, { liquid, P::lqRate, 0.2f },
                              { -1, P::bdSpread, 0.35f }, { -1, P::width, 0.2f } }, 3));
    add (pick (Src::macro3, { { fracture, P::frCrack, 0.45f }, { fracture, P::frGrit, 0.35f }, { fracture, P::frFold, 0.35f },
                              { -1, P::poDrive, 0.5f }, { -1, P::bdInharm, 0.3f } }, 3));
    add (pick (Src::macro4, { { space, P::spMix, 0.3f }, { echo, P::ecMix, 0.3f }, { space, P::spDecay, 0.2f },
                              { echo, P::ecFeedback, 0.2f }, { -1, P::width, 0.25f } }, 3));
    return all;
}

// ---------------------------------------------------------------------------
namespace
{
using namespace dsp;

// Choice indices, for readability.
namespace Mat { enum { glass = 0, crystal, bronze, iron, wood, clay, tube, skin }; }
namespace Tun { enum { free = 0, note, chord }; }
namespace Note { enum { C = 0, Cs, D, Ds, E, F, Fs, G, Gs, A, As, B }; }
namespace Ch { enum { octaves = 0, fifths, major, minor, sus2, sus4, maj7, min7, dom7, min9, add9, dim }; }
namespace Ex { enum { input = 0, transients, noise }; }
namespace Sp { enum { hall = 0, plate, reverse, gated }; }
namespace Lfo { enum { sine = 0, tri, sawUp, sawDown, square, sh, smooth }; }

constexpr float on = 1.0f;

std::vector<FactoryPreset> build()
{
    std::vector<FactoryPreset> v;

    // ======================================================================= Init
    v.push_back ({ "Init", "Init",
        "A clean glass vessel tuned to A, with a touch of room. Drag the water line or turn Resonance.",
        { { P::bdMix, 0.5f }, { P::spOn, on }, { P::spMix, 0.15f } } });

    // ======================================================================= Glass & Crystal
    v.push_back ({ "Wine Glass", "Glass & Crystal",
        "A thin wine glass singing along with the input. Drag the water line to pour it full.",
        { { P::bdMaterial, Mat::glass }, { P::bdMix, 0.55f }, { P::bdDecay, 3.0f }, { P::bdDamping, 0.35f },
          { P::fill, 0.15f }, { P::lqOn, on }, { P::lqSlosh, 0.25f }, { P::lqRipple, 0.15f }, { P::lqMix, 0.25f },
          { P::spOn, on }, { P::spMix, 0.2f } } });
    v.push_back ({ "Crystal Bowl", "Glass & Crystal",
        "A singing bowl: paired modes beat slowly against each other. Long, pure and meditative.",
        { { P::bdMaterial, Mat::crystal }, { P::bdRoot, Note::D }, { P::bdOctave, 3 }, { P::bdMix, 0.65f },
          { P::bdDecay, 8.0f }, { P::bdDamping, 0.2f }, { P::bdSpread, 0.5f }, { P::poTilt, -0.2f },
          { P::spOn, on }, { P::spMix, 0.25f }, { P::spDecay, 6.0f }, { P::spSize, 0.8f } } });
    v.push_back ({ "Glass Harmonica", "Glass & Crystal",
        "Bowed glass: noise shaped by your sound keeps the rims singing. Lovely on pads and vocals.",
        { { P::bdMaterial, Mat::glass }, { P::bdExcite, Ex::noise }, { P::bdRoot, Note::E }, { P::bdOctave, 4 },
          { P::bdMix, 0.5f }, { P::bdDecay, 2.5f }, { P::bdModes, 8 }, { P::bdBright, -0.2f },
          { P::lqOn, on }, { P::lqSlosh, 0.2f }, { P::lqRipple, 0.3f }, { P::lqMix, 0.35f }, { P::lqRate, 0.25f },
          { P::spOn, on }, { P::spMix, 0.3f }, { P::spDecay, 4.5f } } });
    v.push_back ({ "Ice Chimes", "Glass & Crystal",
        "Only the hits ring the vessel: drums and plucks scatter icy, tuned chimes.",
        { { P::bdMaterial, Mat::crystal }, { P::bdExcite, Ex::transients }, { P::bdOctave, 5 }, { P::bdRoot, Note::B },
          { P::bdMix, 0.45f }, { P::bdDecay, 3.5f }, { P::bdBright, 0.4f }, { P::poStrike, 0.6f },
          { P::ecOn, on }, { P::ecDiv, Div::d8D }, { P::ecMix, 0.25f }, { P::ecFeedback, 0.4f }, { P::ecPingPong, on },
          { P::spOn, on }, { P::spMix, 0.25f }, { P::spMode, Sp::plate } } });
    v.push_back ({ "Frosted Pane", "Glass & Crystal",
        "Dark, short glass with a soft room. Adds a hard, clear edge without ringing over the mix.",
        { { P::bdMaterial, Mat::glass }, { P::bdMix, 0.35f }, { P::bdDecay, 0.6f }, { P::bdDamping, 0.6f },
          { P::bdBright, -0.4f }, { P::bdRoot, Note::G }, { P::spOn, on }, { P::spMix, 0.15f }, { P::spDecay, 1.5f } } });
    v.push_back ({ "Shimmer Glass", "Glass & Crystal",
        "Glass into a shimmering hall: every note leaves a rising octave halo.",
        { { P::bdMaterial, Mat::glass }, { P::bdMix, 0.45f }, { P::bdDecay, 4.0f }, { P::bdRoot, Note::C }, { P::bdOctave, 4 },
          { P::spOn, on }, { P::spMix, 0.35f }, { P::spDecay, 8.0f }, { P::spShimmer, 0.5f }, { P::spSize, 0.85f } } });

    // ======================================================================= Metal & Bells
    v.push_back ({ "Bronze Bell", "Metal & Bells",
        "A tuned church bell: hum, prime, minor-third tierce and nominal. Great on piano and keys.",
        { { P::bdMaterial, Mat::bronze }, { P::bdRoot, Note::C }, { P::bdOctave, 4 }, { P::bdMix, 0.5f },
          { P::bdDecay, 5.0f }, { P::bdDamping, 0.25f }, { P::poStrike, 0.3f },
          { P::spOn, on }, { P::spMix, 0.25f }, { P::spDecay, 5.0f } } });
    v.push_back ({ "Temple Gong", "Metal & Bells",
        "A low bronze gong that blooms and swims. Hit it with drums or a sub.",
        { { P::bdMaterial, Mat::bronze }, { P::bdRoot, Note::A }, { P::bdOctave, 2 }, { P::bdMix, 0.6f },
          { P::bdDecay, 9.0f }, { P::bdInharm, 0.25f }, { P::bdSpread, 0.6f },
          { P::lqOn, on }, { P::lqSlosh, 0.5f }, { P::lqRate, 0.15f }, { P::lqRipple, 0.1f }, { P::lqMix, 0.2f },
          { P::spOn, on }, { P::spMix, 0.3f }, { P::spDecay, 7.0f }, { P::spSize, 0.9f } } });
    v.push_back ({ "Iron Pot", "Metal & Bells",
        "Clangy, close cast iron. Turns percussion into kitchen-sink industrial hits.",
        { { P::bdMaterial, Mat::iron }, { P::bdExcite, Ex::transients }, { P::bdRoot, Note::F }, { P::bdOctave, 3 },
          { P::bdMix, 0.5f }, { P::bdDecay, 1.2f }, { P::bdInharm, 0.3f }, { P::poDrive, 0.3f }, { P::poStrike, 0.4f },
          { P::spOn, on }, { P::spMix, 0.12f }, { P::spDecay, 1.0f }, { P::spSize, 0.3f } } });
    v.push_back ({ "Steel Plate", "Metal & Bells",
        "A free steel plate: dense, bright metallic shimmer behind the source.",
        { { P::bdMaterial, Mat::iron }, { P::bdTune, Tun::free }, { P::bdPitch, 180.0f }, { P::bdMix, 0.4f },
          { P::bdDecay, 4.0f }, { P::bdDamping, 0.15f }, { P::bdBright, 0.3f }, { P::bdSpread, 0.7f },
          { P::spOn, on }, { P::spMix, 0.2f }, { P::spMode, Sp::plate }, { P::spDecay, 3.0f } } });
    v.push_back ({ "Tuned Anvil", "Metal & Bells",
        "Short, hard iron strikes tuned to your root. Drives through the mix.",
        { { P::bdMaterial, Mat::iron }, { P::bdExcite, Ex::transients }, { P::bdRoot, Note::D }, { P::bdOctave, 4 },
          { P::bdMix, 0.45f }, { P::bdDecay, 0.7f }, { P::bdDamping, 0.5f }, { P::poStrike, 0.7f }, { P::poDrive, 0.2f },
          { P::frOn, on }, { P::frMix, 0.35f }, { P::frCrack, 0.0f }, { P::frFold, 0.3f } } });
    v.push_back ({ "Cathedral Bells", "Metal & Bells",
        "Bronze bells tuned to a major chord, ringing into a huge hall.",
        { { P::bdMaterial, Mat::bronze }, { P::bdTune, Tun::chord }, { P::bdChord, Ch::major }, { P::bdRoot, Note::G },
          { P::bdOctave, 3 }, { P::bdMix, 0.5f }, { P::bdDecay, 6.0f }, { P::bdSpread, 0.5f },
          { P::ecOn, on }, { P::ecDiv, Div::d4 }, { P::ecMix, 0.18f }, { P::ecFeedback, 0.3f },
          { P::spOn, on }, { P::spMix, 0.35f }, { P::spDecay, 9.0f }, { P::spSize, 1.0f }, { P::spPredelay, 60.0f } } });

    // ======================================================================= Wood & Clay
    v.push_back ({ "Wood Box", "Wood & Clay",
        "A small wooden box: warm, short and woody. Body for thin synths and samples.",
        { { P::bdMaterial, Mat::wood }, { P::bdRoot, Note::A }, { P::bdOctave, 2 }, { P::bdMix, 0.45f },
          { P::bdDecay, 1.5f }, { P::bdSpread, 0.4f } } });
    v.push_back ({ "Clay Jar", "Wood & Clay",
        "A hollow clay jar: dull, round and earthy. Plucks and claps get a hand-made body.",
        { { P::bdMaterial, Mat::clay }, { P::bdRoot, Note::E }, { P::bdOctave, 3 }, { P::bdMix, 0.5f },
          { P::bdDecay, 2.0f }, { P::bdBright, -0.2f }, { P::spOn, on }, { P::spMix, 0.12f }, { P::spDecay, 1.2f } } });
    v.push_back ({ "Marimba Body", "Wood & Clay",
        "Hits ring a tuned wooden bar. Turns hats and snares into melodic wood.",
        { { P::bdMaterial, Mat::wood }, { P::bdExcite, Ex::transients }, { P::bdTune, Tun::chord }, { P::bdChord, Ch::minor },
          { P::bdRoot, Note::C }, { P::bdOctave, 4 }, { P::bdMix, 0.55f }, { P::bdDecay, 2.5f }, { P::poStrike, 0.5f },
          { P::spOn, on }, { P::spMix, 0.15f } } });
    v.push_back ({ "Hollow Gourd", "Wood & Clay",
        "A dry, hollow gourd with a bouncing echo. Organic and rhythmic.",
        { { P::bdMaterial, Mat::clay }, { P::bdRoot, Note::G }, { P::bdOctave, 2 }, { P::bdMix, 0.5f }, { P::bdDecay, 0.9f },
          { P::bdModes, 10 }, { P::ecOn, on }, { P::ecDiv, Div::d16 }, { P::ecMix, 0.2f }, { P::ecFeedback, 0.3f },
          { P::ecTone, 3000.0f } } });
    v.push_back ({ "Udu Bottle", "Wood & Clay",
        "Clay with water in it: the pitch dips as you pour. Automate Fill for talking drums.",
        { { P::bdMaterial, Mat::clay }, { P::bdExcite, Ex::transients }, { P::bdRoot, Note::D }, { P::bdOctave, 3 },
          { P::bdMix, 0.6f }, { P::bdDecay, 1.4f }, { P::fill, 0.4f }, { P::bdGlide, 120.0f }, { P::poStrike, 0.5f },
          { P::lqOn, on }, { P::lqSlosh, 0.6f }, { P::lqRipple, 0.0f }, { P::lqRate, 1.5f } },
        { { Src::env, P::fill, -0.3f } } });

    // ======================================================================= Tube & Skin
    v.push_back ({ "Pipe Body", "Tube & Skin",
        "A closed pipe: odd harmonics only, hollow and reedy. Breathy pads become organ-like.",
        { { P::bdMaterial, Mat::tube }, { P::bdRoot, Note::C }, { P::bdOctave, 3 }, { P::bdMix, 0.55f },
          { P::bdDecay, 2.5f }, { P::bdBright, -0.1f }, { P::spOn, on }, { P::spMix, 0.25f }, { P::spDecay, 4.0f } } });
    v.push_back ({ "Didge Tube", "Tube & Skin",
        "A long low tube driven by noise: a droning, buzzing breath under anything.",
        { { P::bdMaterial, Mat::tube }, { P::bdExcite, Ex::noise }, { P::bdRoot, Note::E }, { P::bdOctave, 1 },
          { P::bdMix, 0.5f }, { P::bdDecay, 1.0f }, { P::poDrive, 0.3f },
          { P::lqOn, on }, { P::lqSlosh, 0.3f }, { P::lqRate, 0.6f }, { P::lqRipple, 0.0f } },
        { { Src::lfo1, P::bdBright, 0.25f } } });
    v.push_back ({ "Kick Skin", "Tube & Skin",
        "A tuned drum skin under your kick: a short, pitched boom locked to the key.",
        { { P::bdMaterial, Mat::skin }, { P::bdExcite, Ex::transients }, { P::bdRoot, Note::F }, { P::bdOctave, 1 },
          { P::bdMix, 0.5f }, { P::bdDecay, 2.0f }, { P::bdModes, 6 }, { P::poStrike, 0.6f }, { P::bassMono, 120.0f } } });
    v.push_back ({ "Tom Body", "Tube & Skin",
        "Snares and claps grow a tom shell. Pitch it with the Root.",
        { { P::bdMaterial, Mat::skin }, { P::bdExcite, Ex::transients }, { P::bdRoot, Note::A }, { P::bdOctave, 2 },
          { P::bdMix, 0.5f }, { P::bdDecay, 2.5f }, { P::bdGlide, 0.0f }, { P::poStrike, 0.4f },
          { P::spOn, on }, { P::spMix, 0.15f }, { P::spMode, Sp::gated }, { P::spDecay, 1.5f } } });

    // ======================================================================= Harmonic
    v.push_back ({ "Minor Choir", "Harmonic",
        "Glass tuned to a minor chord: any sound becomes a sung chord in your key.",
        { { P::bdMaterial, Mat::glass }, { P::bdTune, Tun::chord }, { P::bdChord, Ch::minor }, { P::bdRoot, Note::A },
          { P::bdOctave, 3 }, { P::bdMix, 0.55f }, { P::bdDecay, 3.5f }, { P::bdExcite, Ex::noise }, { P::bdModes, 12 },
          { P::lqOn, on }, { P::lqSlosh, 0.15f }, { P::lqRipple, 0.35f }, { P::lqMix, 0.3f },
          { P::spOn, on }, { P::spMix, 0.3f }, { P::spDecay, 5.0f } } });
    v.push_back ({ "Major Glow", "Harmonic",
        "Warm crystal major chord with a slow, bright halo. Instant uplift on drums or noise.",
        { { P::bdMaterial, Mat::crystal }, { P::bdTune, Tun::chord }, { P::bdChord, Ch::maj7 }, { P::bdRoot, Note::F },
          { P::bdOctave, 3 }, { P::bdMix, 0.5f }, { P::bdDecay, 4.0f },
          { P::spOn, on }, { P::spMix, 0.3f }, { P::spShimmer, 0.3f }, { P::spDecay, 6.0f } } });
    v.push_back ({ "Sus Halo", "Harmonic",
        "Open sus2 harmony in a wide glass: neither happy nor sad, just floating.",
        { { P::bdMaterial, Mat::glass }, { P::bdTune, Tun::chord }, { P::bdChord, Ch::sus2 }, { P::bdRoot, Note::D },
          { P::bdOctave, 3 }, { P::bdMix, 0.5f }, { P::bdDecay, 5.0f }, { P::bdSpread, 0.7f },
          { P::ecOn, on }, { P::ecDiv, Div::d4D }, { P::ecMix, 0.2f }, { P::ecFeedback, 0.45f }, { P::ecReturn, 0.3f },
          { P::spOn, on }, { P::spMix, 0.3f } } });
    v.push_back ({ "Min9 Dream", "Harmonic",
        "A lush minor-ninth cloud with liquid motion. Lo-fi keys and pads love it.",
        { { P::bdMaterial, Mat::wood }, { P::bdTune, Tun::chord }, { P::bdChord, Ch::min9 }, { P::bdRoot, Note::E },
          { P::bdOctave, 3 }, { P::bdMix, 0.5f }, { P::bdDecay, 3.0f }, { P::bdDamping, 0.5f },
          { P::lqOn, on }, { P::lqSlosh, 0.35f }, { P::lqRipple, 0.45f }, { P::lqMix, 0.4f }, { P::lqRate, 0.2f },
          { P::spOn, on }, { P::spMix, 0.25f } } });
    v.push_back ({ "Fifths Drone", "Harmonic",
        "A low drone in fifths, excited by noise and sealed when you need it to hold.",
        { { P::bdMaterial, Mat::tube }, { P::bdTune, Tun::chord }, { P::bdChord, Ch::fifths }, { P::bdRoot, Note::C },
          { P::bdOctave, 2 }, { P::bdExcite, Ex::noise }, { P::bdMix, 0.6f }, { P::bdDecay, 8.0f },
          { P::spOn, on }, { P::spMix, 0.3f }, { P::spDecay, 8.0f }, { P::sealSpace, on } } });
    v.push_back ({ "Dim Tension", "Harmonic",
        "Diminished iron: dark, uneasy harmony for builds and film moments.",
        { { P::bdMaterial, Mat::iron }, { P::bdTune, Tun::chord }, { P::bdChord, Ch::dim }, { P::bdRoot, Note::B },
          { P::bdOctave, 2 }, { P::bdMix, 0.5f }, { P::bdDecay, 4.0f }, { P::bdInharm, 0.15f },
          { P::lqOn, on }, { P::lqSlosh, 0.4f }, { P::lqTurb, 0.5f }, { P::lqRipple, 0.2f },
          { P::spOn, on }, { P::spMix, 0.3f }, { P::spMode, Sp::reverse } } });

    // ======================================================================= Liquid
    v.push_back ({ "Water Glass", "Liquid",
        "Half-full and swirling: the pitch wobbles like water moving in a glass.",
        { { P::bdMaterial, Mat::glass }, { P::bdMix, 0.55f }, { P::bdDecay, 3.0f }, { P::fill, 0.5f },
          { P::lqOn, on }, { P::lqSlosh, 0.7f }, { P::lqRate, 0.5f }, { P::lqRipple, 0.3f }, { P::lqMix, 0.35f },
          { P::spOn, on }, { P::spMix, 0.2f } } });
    v.push_back ({ "Sloshing Bowl", "Liquid",
        "A crystal bowl rocked back and forth in time with the track.",
        { { P::bdMaterial, Mat::crystal }, { P::bdMix, 0.6f }, { P::bdDecay, 6.0f }, { P::fill, 0.3f },
          { P::lqOn, on }, { P::lqSync, on }, { P::lqDiv, Div::bar1 }, { P::lqSlosh, 0.8f }, { P::lqRipple, 0.4f },
          { P::lqMix, 0.4f }, { P::spOn, on }, { P::spMix, 0.25f } } });
    v.push_back ({ "Underwater", "Liquid",
        "Sunk: dark, full, turbulent and wobbling. Classic breakdown move.",
        { { P::bdMaterial, Mat::clay }, { P::bdMix, 0.5f }, { P::bdDecay, 2.0f }, { P::fill, 0.85f }, { P::poTilt, -0.7f },
          { P::lqOn, on }, { P::lqSlosh, 0.6f }, { P::lqTurb, 0.6f }, { P::lqRipple, 0.8f }, { P::lqMix, 0.6f },
          { P::lqRate, 0.4f }, { P::spOn, on }, { P::spMix, 0.3f }, { P::spDamp, 2500.0f } } });
    v.push_back ({ "Tide Pool", "Liquid",
        "Slow currents through a shallow bronze pool. Evolving, never the same twice.",
        { { P::bdMaterial, Mat::bronze }, { P::bdRoot, Note::Fs }, { P::bdOctave, 3 }, { P::bdMix, 0.5f }, { P::bdDecay, 5.0f },
          { P::lqOn, on }, { P::lqSlosh, 0.5f }, { P::lqTurb, 0.8f }, { P::lqRipple, 0.4f }, { P::lqMix, 0.4f },
          { P::lqRate, 0.12f }, { P::spOn, on }, { P::spMix, 0.3f }, { P::spDecay, 6.0f } },
        { { Src::drift, P::fill, 0.15f } } });
    v.push_back ({ "Rain Barrel", "Liquid",
        "Hits drip into a wooden barrel: tuned plinks with liquid echoes.",
        { { P::bdMaterial, Mat::wood }, { P::bdExcite, Ex::transients }, { P::bdOctave, 4 }, { P::bdMix, 0.5f },
          { P::bdDecay, 1.5f }, { P::fill, 0.35f }, { P::poStrike, 0.5f },
          { P::lqOn, on }, { P::lqSlosh, 0.5f }, { P::lqRate, 2.0f }, { P::lqRipple, 0.3f },
          { P::ecOn, on }, { P::ecDiv, Div::d16 }, { P::ecMix, 0.25f }, { P::ecFeedback, 0.5f }, { P::ecPingPong, on } } });

    // ======================================================================= Fractured
    v.push_back ({ "Cracked Glass", "Fractured",
        "A glass with a crack in it: glassy crackles and folded edges over a ringing body.",
        { { P::bdMaterial, Mat::glass }, { P::bdMix, 0.5f }, { P::bdDecay, 2.5f },
          { P::frOn, on }, { P::frMix, 0.6f }, { P::frCrack, 0.45f }, { P::frFold, 0.3f },
          { P::spOn, on }, { P::spMix, 0.2f } } });
    v.push_back ({ "Broken Bell", "Fractured",
        "A split bronze bell: shards ring-modulated against its own pitch.",
        { { P::bdMaterial, Mat::bronze }, { P::bdRoot, Note::E }, { P::bdOctave, 3 }, { P::bdMix, 0.55f }, { P::bdDecay, 4.0f },
          { P::bdInharm, 0.3f }, { P::frOn, on }, { P::frMix, 0.5f }, { P::frShard, 0.5f }, { P::frCrack, 0.2f },
          { P::frFold, 0.0f }, { P::spOn, on }, { P::spMix, 0.25f } } });
    v.push_back ({ "Crushed Bronze", "Fractured",
        "Bit-crushed metal: grimy, lo-fi bell hits.",
        { { P::bdMaterial, Mat::bronze }, { P::bdExcite, Ex::transients }, { P::bdMix, 0.5f }, { P::bdDecay, 2.0f },
          { P::frOn, on }, { P::frMix, 0.7f }, { P::frGrit, 0.55f }, { P::frCrack, 0.1f }, { P::frFold, 0.0f },
          { P::frTone, -0.2f } } });
    v.push_back ({ "Shattered Choir", "Fractured",
        "A minor-chord choir cracked from the inside: Fracture before the Body.",
        { { P::bdMaterial, Mat::glass }, { P::bdTune, Tun::chord }, { P::bdChord, Ch::min7 }, { P::bdRoot, Note::G },
          { P::bdOctave, 3 }, { P::bdMix, 0.6f }, { P::bdDecay, 3.0f },
          { P::frOn, on }, { P::frPos, 1.0f }, { P::frMix, 0.8f }, { P::frCrack, 0.6f }, { P::frGrit, 0.3f },
          { P::spOn, on }, { P::spMix, 0.3f } } });
    v.push_back ({ "Fault Line", "Fractured",
        "Heavy fracture with dropouts: pieces breaking away in time with the echoes.",
        { { P::bdMaterial, Mat::iron }, { P::bdMix, 0.5f }, { P::bdDecay, 1.5f },
          { P::frOn, on }, { P::frMix, 0.7f }, { P::frCrack, 0.75f }, { P::frFold, 0.4f }, { P::frGrit, 0.2f },
          { P::ecOn, on }, { P::ecDiv, Div::d8 }, { P::ecMix, 0.25f }, { P::ecFeedback, 0.4f } } });

    // ======================================================================= Rhythm
    v.push_back ({ "Snare Ring", "Rhythm",
        "Each snare rings a tuned glass. Puts your drums in key.",
        { { P::bdMaterial, Mat::glass }, { P::bdExcite, Ex::transients }, { P::bdRoot, Note::A }, { P::bdOctave, 4 },
          { P::bdMix, 0.4f }, { P::bdDecay, 1.0f }, { P::poStrike, 0.5f }, { P::poLowCut, 200.0f },
          { P::spOn, on }, { P::spMix, 0.15f }, { P::spMode, Sp::plate } } });
    v.push_back ({ "Hat Shimmer", "Rhythm",
        "Hats and shakers grow a bright crystal tail.",
        { { P::bdMaterial, Mat::crystal }, { P::bdExcite, Ex::transients }, { P::bdOctave, 5 }, { P::bdMix, 0.35f },
          { P::bdDecay, 1.8f }, { P::bdBright, 0.3f }, { P::poLowCut, 600.0f },
          { P::ecOn, on }, { P::ecDiv, Div::d16 }, { P::ecMix, 0.2f }, { P::ecFeedback, 0.35f }, { P::ecPingPong, on } } });
    v.push_back ({ "Tuned Kick", "Rhythm",
        "A clay body under the kick, tuned and mono below 150 Hz. Fat, pitched punch.",
        { { P::bdMaterial, Mat::clay }, { P::bdExcite, Ex::transients }, { P::bdRoot, Note::G }, { P::bdOctave, 1 },
          { P::bdMix, 0.4f }, { P::bdDecay, 0.8f }, { P::bdModes, 5 }, { P::poStrike, 0.5f }, { P::poDrive, 0.2f },
          { P::bassMono, 150.0f } } });
    v.push_back ({ "Echo Body Loop", "Rhythm",
        "Echoes feed back into the vessel: every repeat rings it again, building a tuned groove.",
        { { P::bdMaterial, Mat::wood }, { P::bdExcite, Ex::transients }, { P::bdTune, Tun::chord }, { P::bdChord, Ch::min7 },
          { P::bdRoot, Note::C }, { P::bdOctave, 3 }, { P::bdMix, 0.5f }, { P::bdDecay, 1.5f },
          { P::ecOn, on }, { P::ecDiv, Div::d8D }, { P::ecMix, 0.3f }, { P::ecFeedback, 0.45f }, { P::ecReturn, 0.6f },
          { P::ecPingPong, on } } });
    v.push_back ({ "Tribal Clay", "Rhythm",
        "Clay pots and wooden echoes. Turns a drum loop into hand percussion.",
        { { P::bdMaterial, Mat::clay }, { P::bdExcite, Ex::transients }, { P::bdTune, Tun::chord }, { P::bdChord, Ch::fifths },
          { P::bdRoot, Note::D }, { P::bdOctave, 2 }, { P::bdMix, 0.55f }, { P::bdDecay, 1.0f }, { P::poStrike, 0.6f },
          { P::ecOn, on }, { P::ecDiv, Div::d16 }, { P::ecMix, 0.2f }, { P::ecFeedback, 0.25f } } });

    // ======================================================================= DJ Tools
    v.push_back ({ "Seal Drop", "DJ Tools",
        "Set to latch: hit SEAL before the drop to hold a tuned ring (and its room), release on the one.",
        { { P::bdMaterial, Mat::glass }, { P::bdTune, Tun::chord }, { P::bdChord, Ch::minor }, { P::bdMix, 0.45f },
          { P::bdDecay, 3.0f }, { P::spOn, on }, { P::spMix, 0.3f }, { P::spDecay, 5.0f }, { P::sealSpace, on } } });
    v.push_back ({ "Key Wash", "DJ Tools",
        "Set the Root to the track's key: a harmonic wash that sits in key under any blend.",
        { { P::bdMaterial, Mat::crystal }, { P::bdTune, Tun::chord }, { P::bdChord, Ch::fifths }, { P::bdMix, 0.4f },
          { P::bdDecay, 6.0f }, { P::bdExcite, Ex::noise }, { P::poLowCut, 250.0f },
          { P::spOn, on }, { P::spMix, 0.35f }, { P::spDecay, 8.0f }, { P::bassMono, 200.0f } } });
    v.push_back ({ "Shatter Riser", "DJ Tools",
        "Hold SHATTER through the build: the vessel breaks apart over 8 seconds. Let go to drop.",
        { { P::bdMaterial, Mat::glass }, { P::bdMix, 0.45f }, { P::bdDecay, 2.5f }, { P::shatterTime, 8.0f },
          { P::shatterDepth, 1.0f }, { P::ecOn, on }, { P::ecDiv, Div::d8 }, { P::ecMix, 0.15f }, { P::ecFeedback, 0.4f },
          { P::spOn, on }, { P::spMix, 0.25f } } });
    v.push_back ({ "Echo Ring Out", "DJ Tools",
        "Long tuned echoes ringing the vessel. Turn Space up to wash out of a track.",
        { { P::bdMaterial, Mat::bronze }, { P::bdMix, 0.35f }, { P::bdDecay, 3.0f },
          { P::ecOn, on }, { P::ecDiv, Div::d4 }, { P::ecMix, 0.35f }, { P::ecFeedback, 0.6f }, { P::ecReturn, 0.4f },
          { P::spOn, on }, { P::spMix, 0.3f }, { P::spDecay, 6.0f } } });
    v.push_back ({ "Pour Sweep", "DJ Tools",
        "The Motion macro pours the vessel full and back: a liquid, pitched filter sweep.",
        { { P::bdMaterial, Mat::glass }, { P::bdMix, 0.5f }, { P::bdDecay, 2.0f }, { P::lqOn, on }, { P::lqSlosh, 0.3f },
          { P::lqRipple, 0.3f }, { P::lqMix, 0.3f }, { P::spOn, on }, { P::spMix, 0.2f } },
        { { Src::macro2, P::fill, 0.9f } } });

    // ======================================================================= Vocal
    v.push_back ({ "Vocal Vessel", "Vocal",
        "Vocals sung into a glass: a tuned, airy halo that follows the words.",
        { { P::bdMaterial, Mat::glass }, { P::bdTune, Tun::chord }, { P::bdChord, Ch::octaves }, { P::bdMix, 0.35f },
          { P::bdDecay, 1.5f }, { P::bdDamping, 0.4f }, { P::poLowCut, 180.0f },
          { P::spOn, on }, { P::spMix, 0.2f }, { P::spMode, Sp::plate } } });
    v.push_back ({ "Choir in a Jar", "Vocal",
        "One voice becomes a muffled choir inside a clay jar.",
        { { P::bdMaterial, Mat::clay }, { P::bdTune, Tun::chord }, { P::bdChord, Ch::add9 }, { P::bdExcite, Ex::noise },
          { P::bdMix, 0.5f }, { P::bdDecay, 2.5f }, { P::lqOn, on }, { P::lqRipple, 0.4f }, { P::lqMix, 0.4f },
          { P::lqSlosh, 0.2f }, { P::spOn, on }, { P::spMix, 0.25f } } });
    v.push_back ({ "Robot Bowl", "Vocal",
        "Metallic, ring-modulated speech in a steel bowl.",
        { { P::bdMaterial, Mat::iron }, { P::bdTune, Tun::free }, { P::bdPitch, 140.0f }, { P::bdMix, 0.5f }, { P::bdDecay, 0.6f },
          { P::frOn, on }, { P::frMix, 0.5f }, { P::frShard, 0.6f }, { P::frCrack, 0.0f }, { P::frFold, 0.0f } } });

    return v;
}
} // namespace

const std::vector<FactoryPreset>& factoryPresets()
{
    static const std::vector<FactoryPreset> presets = build();
    return presets;
}

} // namespace vessel
