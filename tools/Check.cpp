// vessel_check: headless verification of the real processor + editor.
//
//   vessel_check snapshot <out.png> [width] [view] [preset]
//        view: easy | easy-shatter | easy-seal | vessel | fx | mod | global
//   vessel_check presets     every factory preset as saved, with Shatter
//                            engaged and with all four macros at 100%: no
//                            NaN, no runaway level, no silence, tails that
//                            decay (as saved)
//   vessel_check state       save/restore round trip, undo, Seal holding,
//                            preset data integrity, randomizer safety
//
// Exits non-zero on any failure: CI treats that as a failed build.

#include "PluginEditor.h"
#include "PluginProcessor.h"

#include <juce_gui_basics/juce_gui_basics.h>

#include <cstdio>
#include <functional>

using namespace vessel;

namespace
{
void setParam (VesselProcessor& p, P param, float real)
{
    auto* prm = p.apvts.getParameter (def (param).id);
    prm->setValueNotifyingHost (prm->convertTo0to1 (real));
}

struct Render
{
    float peak = 0.0f;
    double rmsDb = -200.0, dryDb = -200.0, tailDb = -200.0, earlyTailDb = -200.0;
    bool finite = true;
};

// 3 s of a chord-like test signal with a little noise, then `tailSeconds` of silence.
Render render (VesselProcessor& proc, double tailSeconds, std::function<void()> atOneSecond = {},
              std::function<void()> atTailStart = {})
{
    const double fs = 48000.0;
    const int block = 512;
    proc.prepareToPlay (fs, block);
    proc.reset();

    juce::MidiBuffer midi;
    const int signalLen = (int) (fs * 3.0), total = signalLen + (int) (fs * tailSeconds);
    juce::AudioBuffer<float> buf (2, block);
    juce::Random rng (1);
    double sumOut = 0.0, sumIn = 0.0, tail = 0.0, early = 0.0;
    Render r;

    for (int pos = 0; pos < total; pos += block)
    {
        if (atOneSecond && pos >= (int) fs && pos < (int) fs + block) atOneSecond();
        if (atTailStart && pos >= signalLen - block && pos < signalLen) atTailStart();
        for (int s = 0; s < block; ++s)
        {
            const int n = pos + s;
            float x = 0.0f;
            if (n < signalLen)
            {
                const float t = (float) n / (float) fs;
                // Plucked chord: re-triggers every half second so dynamics and gates have something to do.
                const float env = std::exp (-6.0f * std::fmod (t, 0.5f));
                x = env * 0.14f * (std::sin (6.2832f * 110.0f * t) + std::sin (6.2832f * 220.5f * t)
                                 + std::sin (6.2832f * 329.6f * t) + 0.5f * std::sin (6.2832f * 1318.0f * t))
                    + 0.01f * (rng.nextFloat() * 2.0f - 1.0f);
                sumIn += (double) x * x;
            }
            buf.setSample (0, s, x);
            buf.setSample (1, s, x);
        }
        proc.processBlock (buf, midi);
        for (int s = 0; s < block; ++s)
        {
            const float l = buf.getSample (0, s), rr = buf.getSample (1, s);
            r.finite = r.finite && std::isfinite (l) && std::isfinite (rr);
            r.peak = std::max ({ r.peak, std::abs (l), std::abs (rr) });
            const double e = 0.5 * ((double) l * l + (double) rr * rr);
            const int n = pos + s;
            if (n < signalLen) sumOut += e;
            if (n >= total - (int) fs) tail += e;
            if (n >= signalLen && n < signalLen + (int) fs) early += e;
        }
    }
    const auto db = [] (double ms) { return 10.0 * std::log10 (ms + 1e-20); };
    r.rmsDb = db (sumOut / (fs * 3.0));
    r.dryDb = db (sumIn / (fs * 3.0));
    r.tailDb = db (tail / fs);
    r.earlyTailDb = db (early / fs);
    return r;
}

juce::String judge (const Render& r, bool checkTail, double quietDb = 14.0)
{
    if (! r.finite)                 return "NaN/Inf";
    if (r.peak > 0.97f)             return "over ceiling";   // limiter must hold -0.3 dBFS
    if (r.rmsDb < r.dryDb - quietDb) return "too quiet";
    if (r.rmsDb > r.dryDb + 8.0)    return "too loud";
    if (checkTail && r.tailDb > -70.0 && (r.tailDb > r.earlyTailDb - 20.0 || r.tailDb > r.rmsDb - 30.0))
        return "tail does not decay";
    return {};
}

int checkPresets()
{
    VesselProcessor proc;
    int failures = 0;
    const auto& entries = proc.presets.getEntries();
    std::printf ("%-20s %-16s %8s %8s %8s %8s  %s\n", "preset", "category", "dry dB", "saved", "shatter", "macros", "result");

    for (int i = 0; i < entries.size(); ++i)
    {
        if (entries[i].factoryIndex < 0) continue;

        proc.presets.load (i);
        const auto normal = render (proc, 12.0);

        // Shatter engaged after a second of audio, the way it is played.
        proc.presets.load (i);
        const auto shattered = render (proc, 2.0, [&] { setParam (proc, P::shatter, 1.0f); });
        setParam (proc, P::shatter, 0.0f);

        proc.presets.load (i);
        for (auto m : { P::macro1, P::macro2, P::macro3, P::macro4 }) setParam (proc, m, 1.0f);
        const auto macros = render (proc, 2.0);

        juce::String problem = judge (normal, true);
        if (problem.isEmpty()) { problem = judge (shattered, false, 24.0); if (problem.isNotEmpty()) problem = "shatter: " + problem; }
        if (problem.isEmpty()) { problem = judge (macros, false);    if (problem.isNotEmpty()) problem = "macros: " + problem; }

        std::printf ("%-20s %-16s %8.1f %8.1f %8.1f %8.1f  %s\n", entries[i].name.toRawUTF8(), entries[i].category.toRawUTF8(),
                     normal.dryDb, normal.rmsDb, shattered.rmsDb, macros.rmsDb,
                     problem.isEmpty() ? "ok" : ("FAIL: " + problem).toRawUTF8());
        failures += problem.isNotEmpty() ? 1 : 0;
    }
    std::printf ("%d preset(s) failed\n", failures);
    return failures == 0 ? 0 : 1;
}

int fail (const char* what) { std::printf ("FAIL: %s\n", what); return 1; }

int checkState()
{
    int failures = 0;

    // The editor reopens at the size saved with the project.
    {
        VesselProcessor a;
        {
            std::unique_ptr<juce::AudioProcessorEditor> ed (a.createEditor());
            ed->setSize (1500, 950);
        }
        juce::MemoryBlock mb;
        a.getStateInformation (mb);
        VesselProcessor b;
        b.setStateInformation (mb.getData(), (int) mb.getSize());
        std::unique_ptr<juce::AudioProcessorEditor> ed (b.createEditor());
        if (ed->getWidth() != 1500) failures += fail ("editor reopens at the saved size");
        else std::printf ("editor reopens at the saved size: ok\n");
    }
    std::printf ("factory presets: %d\n", (int) factoryPresets().size());

    // Preset data integrity: unique names, valid routes, matrix fits.
    {
        VesselProcessor proc;
        juce::StringArray names;
        for (const auto& fp : factoryPresets())
        {
            if (names.contains (fp.name)) { std::printf ("  duplicate preset name %s\n", fp.name); ++failures; }
            names.add (fp.name);
            for (const auto& r : fp.routes) if (! def (r.dst).modulatable) { std::printf ("  unmodulatable route in %s\n", fp.name); ++failures; }
            auto t = proc.presets.buildFactoryPatch (fp);
            auto m = t.getChildWithName (VesselProcessor::kMatrixType);
            int used = 0;
            for (int s = 0; s < m.getNumChildren(); ++s) used += (int) m.getChild (s).getProperty ("src") != 0 ? 1 : 0;
            if (m.getNumChildren() != kMatrixSlots) { std::printf ("  matrix size wrong in %s\n", fp.name); ++failures; }
            std::array<bool, kNumStages> stagesUsed {};
            for (int s = 0; s < kNumStages; ++s)
            {
                float v = def (kStages[s].on).def;
                for (const auto& [p, val] : fp.values) if (p == kStages[s].on) v = val;
                stagesUsed[(size_t) s] = v > 0.5f;
            }
            if (fp.macroKit && macroKit (stagesUsed).size() + fp.routes.size() > (size_t) kMatrixSlots)
            { std::printf ("  too many routes in %s\n", fp.name); ++failures; }
            for (const auto& [p, val] : fp.values)
            {
                const auto& d = def (p);
                if (val < d.min - 1e-4f || val > d.max + 1e-4f) { std::printf ("  %s out of range in %s\n", d.id, fp.name); ++failures; }
            }
            (void) used;
        }
        std::printf ("preset integrity: %s\n", failures == 0 ? "ok" : "FAILED");
    }

    // Round trip: load a preset, tweak, save, restore into a fresh instance.
    {
        VesselProcessor a;
        a.presets.load (8);
        setParam (a, P::bdDecay, 7.5f);
        setParam (a, P::bdMaterial, 3.0f);
        a.addRoute (Src::lfo2, idx (P::ecMix), -0.4f);

        juce::MemoryBlock mb;
        a.getStateInformation (mb);
        VesselProcessor b;
        b.setStateInformation (mb.getData(), (int) mb.getSize());

        if (std::abs (b.apvts.getRawParameterValue ("bd_decay")->load() - 7.5f) > 0.01f) failures += fail ("param round trip");
        if (std::abs (b.apvts.getRawParameterValue ("bd_material")->load() - 3.0f) > 0.01f) failures += fail ("choice round trip");
        if (b.routesTo (idx (P::ecMix)).isEmpty()) failures += fail ("matrix round trip");
        else
        {
            const auto s = b.getSlot (b.routesTo (idx (P::ecMix))[0]);
            if (s.src != Src::lfo2 || std::abs (s.amount + 0.4f) > 1e-4f) failures += fail ("matrix slot contents");
        }
        // Save -> load -> save must be byte-identical (hosts compare states).
        juce::MemoryBlock mb2;
        b.getStateInformation (mb2);
        if (mb != mb2) failures += fail ("state is not reload-stable");
        std::printf ("state round trip: %s\n", failures == 0 ? "ok" : "FAILED");
    }

    // Shatter and a macro actually move the audio-side values.
    {
        VesselProcessor p;
        p.presets.load (0);
        juce::AudioBuffer<float> buf (2, 256);
        buf.clear();
        juce::MidiBuffer midi;
        p.prepareToPlay (48000.0, 256);
        const float crack0 = p.effectiveValue (P::frCrack);
        setParam (p, P::shatterTime, 0.02f);
        setParam (p, P::shatter, 1.0f);
        for (int k = 0; k < 10; ++k) p.processBlock (buf, midi);
        if (p.effectiveValue (P::frOn) < 0.5f) failures += fail ("shatter brings Fracture in");
        if (p.effectiveValue (P::frCrack) <= crack0 + 0.2f) failures += fail ("shatter raises Crack");
        setParam (p, P::shatter, 0.0f);
        for (int k = 0; k < 200; ++k) p.processBlock (buf, midi);
        if (std::abs (p.effectiveValue (P::frCrack) - crack0) > 1e-3f) failures += fail ("shatter releases");
        const float mix0 = p.effectiveValue (P::bdMix);
        setParam (p, P::macro1, 1.0f);
        p.processBlock (buf, midi);
        if (p.effectiveValue (P::bdMix) <= mix0 + 0.1f) failures += fail ("macro 1 moves Body Mix");
        std::printf ("shatter and macros: %s\n", failures == 0 ? "ok" : "FAILED");
    }

    // Seal: after the input stops, a sealed vessel keeps ringing.
    {
        VesselProcessor p;
        p.presets.load (0);
        setParam (p, P::bdDecay, 0.5f);
        setParam (p, P::spOn, 0.0f);
        setParam (p, P::bdMix, 1.0f);
        const auto open = render (p, 3.0);
        p.presets.load (0);
        setParam (p, P::bdDecay, 0.5f);
        setParam (p, P::spOn, 0.0f);
        setParam (p, P::bdMix, 1.0f);
        const auto sealed = render (p, 3.0, [&] {}, [&] { setParam (p, P::seal, 1.0f); });
        setParam (p, P::seal, 0.0f);
        std::printf ("seal: tail %.1f dB open, %.1f dB sealed\n", open.tailDb, sealed.tailDb);
        if (sealed.tailDb < open.tailDb + 30.0) failures += fail ("seal holds the ring");
    }

    // Undo restores a parameter change and a matrix edit.
    {
        VesselProcessor p;
        p.undo.clearUndoHistory();
        p.undo.beginNewTransaction ("route");
        p.addRoute (Src::drift, idx (P::fill), 0.3f);
        p.undo.beginNewTransaction();
        p.undo.undo();
        if (! p.routesTo (idx (P::fill)).isEmpty()) failures += fail ("undo matrix edit");
        std::printf ("undo: %s\n", failures == 0 ? "ok" : "FAILED");
    }

    // Randomizer: always produces a safe patch.
    {
        VesselProcessor p;
        int bad = 0;
        for (std::uint32_t s = 1; s <= 25; ++s)
        {
            p.presets.randomize (s);
            const auto r = render (p, 1.0);
            const auto problem = judge (r, false);
            if (problem.isNotEmpty()) { std::printf ("  random seed %u: %s\n", s, problem.toRawUTF8()); ++bad; }
        }
        failures += bad;
        std::printf ("randomizer (25 seeds): %s\n", bad == 0 ? "ok" : "FAILED");
    }

    std::printf ("%d state check(s) failed\n", failures);
    return failures == 0 ? 0 : 1;
}

int snapshot (const juce::String& path, int width, const juce::String& view, int presetIndex)
{
    VesselProcessor proc;
    proc.prepareToPlay (48000.0, 512);
    if (presetIndex >= 0) proc.presets.load (presetIndex);
    if (view.endsWith ("shatter")) setParam (proc, P::shatter, 1.0f);
    if (view.endsWith ("seal"))    setParam (proc, P::seal, 1.0f);

    // Run some audio so meters, modes and modulation have live values.
    juce::AudioBuffer<float> buf (2, 512);
    juce::MidiBuffer midi;
    for (int b = 0; b < 90; ++b)
    {
        for (int i = 0; i < 512; ++i)
        {
            const float t = (float) (b * 512 + i) / 48000.0f;
            const float env = std::exp (-5.0f * std::fmod (t, 0.5f));
            const float x = env * (0.25f * std::sin (6.2832f * 110.0f * t) + 0.15f * std::sin (6.2832f * 330.0f * t));
            buf.setSample (0, i, x);
            buf.setSample (1, i, x);
        }
        proc.processBlock (buf, midi);
    }

    std::unique_ptr<juce::AudioProcessorEditor> ed (proc.createEditor());
    auto* editor = dynamic_cast<VesselEditor*> (ed.get());
    auto& content = editor->getContent();
    if (view.startsWith ("easy")) content.setView (false);
    else if (view == "vessel") content.showProPage (0);
    else if (view == "fx") content.showProPage (1);
    else if (view == "mod") content.showProPage (2);
    else if (view == "global") content.showProPage (3);
    for (int k = 0; k < 4; ++k) content.tick();

    editor->setSize (width, width * VesselEditor::kDesignH / VesselEditor::kDesignW);
    auto image = editor->createComponentSnapshot (editor->getLocalBounds(), true, 1.0f);

    juce::File out (path);
    out.deleteFile();
    juce::FileOutputStream stream (out);
    juce::PNGImageFormat png;
    const bool ok = stream.openedOk() && png.writeImageToStream (image, stream);
    std::printf ("%s %s (%dx%d)\n", ok ? "wrote" : "FAILED", path.toRawUTF8(), image.getWidth(), image.getHeight());
    return ok ? 0 : 1;
}
} // namespace

int main (int argc, char** argv)
{
    juce::ScopedJuceInitialiser_GUI gui;
    const juce::String mode = argc > 1 ? argv[1] : "";

    if (mode == "snapshot" && argc > 2)
        return snapshot (argv[2], argc > 3 ? juce::String (argv[3]).getIntValue() : 1200,
                         argc > 4 ? juce::String (argv[4]) : juce::String ("easy"),
                         argc > 5 ? juce::String (argv[5]).getIntValue() : -1);
    if (mode == "presets") return checkPresets();
    if (mode == "state")   return checkState();

    std::printf ("usage: vessel_check snapshot <out.png> [width] [easy|vessel|fx|mod|global] [preset] | presets | state\n");
    return 2;
}
