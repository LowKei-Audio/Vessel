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
//   vessel_check static      clicks and static (Serato report, Nov 2026):
//                            every preset switch, Pour Me, A/B and project
//                            load while audio plays; every preset through a
//                            transport that jumps (loop wrap) and restarts
//                            over sound; Seal and Shatter toggled over sound
//   vessel_check static-pair <a> <b>
//                            diagnostic: one preset switch, then again with
//                            each stage forced off (not run by CI)
//
// Exits non-zero on any failure: CI treats that as a failed build.

#include "PluginEditor.h"
#include "PluginProcessor.h"

#include <juce_gui_basics/juce_gui_basics.h>

#include <algorithm>
#include <cstdio>
#include <cstdlib>
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

// ---------------------------------------------------------------------------
// Static / clicks (Serato report, Nov 2026; same method as Starfire 1.0.4).
// Metric: the sharpest sample-to-sample bend of the output (|second difference|)
// near an event, relative to what the sound does on its own. A held soft tone
// gives ~1; an audible click is well above 2.

// A host transport the test drives block by block.
struct TestPlayHead : juce::AudioPlayHead
{
    double ppq = 0.0, bpm = 120.0;
    bool playing = false;
    juce::Optional<PositionInfo> getPosition() const override
    {
        PositionInfo i;
        i.setBpm (bpm);
        i.setIsPlaying (playing);
        i.setPpqPosition (ppq);
        return i;
    }
};

// Soft 3-harmonic tone (no sharp edges of its own) with a short fade-in.
struct SoftTone
{
    double ph = 0.0;
    long n = 0;
    void fill (juce::AudioBuffer<float>& b)
    {
        for (int i = 0; i < b.getNumSamples(); ++i, ++n)
        {
            const float v = 0.35f * (float) (std::sin (ph) + 0.4 * std::sin (2 * ph) + 0.2 * std::sin (3 * ph))
                          * (float) std::min (1.0, (double) n / 240.0);
            ph += 2.0 * juce::MathConstants<double>::pi * 233.08 / 48000.0;
            b.setSample (0, i, v);
            b.setSample (1, i, v);
        }
    }
};

struct Recorder
{
    std::vector<float> l, r;
    void add (const juce::AudioBuffer<float>& b)
    {
        l.insert (l.end(), b.getReadPointer (0), b.getReadPointer (0) + b.getNumSamples());
        r.insert (r.end(), b.getReadPointer (1), b.getReadPointer (1) + b.getNumSamples());
    }
    std::vector<float> bend() const
    {
        std::vector<float> d (l.size(), 0.0f);
        for (size_t i = 2; i < l.size(); ++i)
            d[i] = std::max (std::abs (l[i] - 2 * l[i - 1] + l[i - 2]), std::abs (r[i] - 2 * r[i - 1] + r[i - 2]));
        return d;
    }
};

float sharpest (const std::vector<float>& d, int a, int b)
{
    a = std::max (a, 2); b = std::min (b, (int) d.size());
    return a < b ? *std::max_element (d.begin() + a, d.begin() + b) : 0.0f;
}

bool verboseStatic() { return std::getenv ("VESSEL_VERBOSE") != nullptr; }

// Sharpest moment a patch makes on its own: rendered alone from a fresh plugin for
// 1.5 s, measured after the first 0.3 s (its own start-up is not a switch).
float soloSharpness (const juce::ValueTree& patch)
{
    VesselProcessor proc;
    TestPlayHead head;
    head.playing = true;
    proc.setPlayHead (&head);
    const int block = 512, n = 144 * block;
    proc.prepareToPlay (48000.0, block);
    proc.applyPatchState (patch.createCopy(), true);
    SoftTone tone;
    Recorder rec;
    juce::MidiBuffer midi;
    juce::AudioBuffer<float> buf (2, block);
    for (int done = 0; done < n; done += block)
    {
        tone.fill (buf);
        proc.processBlock (buf, midi);
        rec.add (buf);
        head.ppq += block * head.bpm / 60.0 / 48000.0;
    }
    const auto d = rec.bend();
    return sharpest (d, 28 * block, n);
}

// Sound changes while audio plays. Each switch is watched for 250 ms (the fade out,
// the swap, the fade in and the 150 ms the detectors settle) and compared with the
// sharpest moment either sound makes on its own (rendered alone from a fresh plugin,
// and in this run: the old sound's last second, the new one from 0.5 s on) and with
// the dry input itself, which the switch passes through at the swap. Cracks and
// grit have sharp edges by design; a switch must never be sharper than that. Steps that
// an echo or reverb could replay later are covered by StaticTests (Echo jump, routing,
// Decay).
int checkSoundChanges (float& worstOut)
{
    VesselProcessor proc;
    TestPlayHead head;
    proc.setPlayHead (&head);
    const int block = 512, segment = 144 * block;   // ~1.5 s per sound
    proc.prepareToPlay (48000.0, block);
    SoftTone tone;
    Recorder rec;
    juce::MidiBuffer midi;
    juce::AudioBuffer<float> buf (2, block);
    auto play = [&] (int samples)
    {
        for (int done = 0; done < samples; done += block)
        {
            tone.fill (buf);
            proc.processBlock (buf, midi);
            rec.add (buf);
            head.ppq += block * head.bpm / 60.0 / 48000.0;
        }
    };

    struct Change { int at; juce::String name; juce::ValueTree from, to; };
    std::vector<Change> changes;
    auto change = [&] (const juce::String& name, const std::function<void()>& f)
    {
        Change c { (int) rec.l.size(), name, proc.getPatchState(), {} };
        f();
        c.to = proc.getPatchState();
        changes.push_back (c);
        play (segment);
    };

    head.playing = true;
    const auto& entries = proc.presets.getEntries();
    proc.presets.load (0);
    play (segment);
    for (int i = 1; i < entries.size(); ++i)
        if (entries[i].factoryIndex >= 0)
            change ("preset " + entries[i].name, [&] { proc.presets.load (i); });
    for (std::uint32_t s = 1; s <= 6; ++s)
        change ("Pour Me " + juce::String ((int) s), [&] { proc.presets.randomize (s * 101); });
    proc.presets.load (3);
    play (segment);
    proc.presets.copyActiveToOther();
    proc.presets.selectSlot (1);
    change ("Pour Me into B", [&] { proc.presets.randomize (77); });
    change ("A/B to A", [&] { proc.presets.selectSlot (0); });
    change ("A/B to B", [&] { proc.presets.selectSlot (1); });
    {
        VesselProcessor other;   // a project saved with another preset, loaded while playing
        other.presets.load (entries.size() / 2);
        juce::MemoryBlock mb;
        other.getStateInformation (mb);
        change ("project load", [&] { proc.setStateInformation (mb.getData(), (int) mb.getSize()); });
    }

    // The dry input itself: the switch passes it through at the swap, which is never a click.
    float toneSharpness = 0.0f;
    {
        SoftTone t2;
        juce::AudioBuffer<float> b2 (2, 48000);
        t2.fill (b2);
        Recorder r2;
        r2.add (b2);
        const auto d2 = r2.bend();
        toneSharpness = sharpest (d2, 480, 48000);
    }

    const auto d = rec.bend();
    int fails = 0;
    float worst = 0.0f;
    for (const auto& c : changes)
    {
        const float ref = std::max ({ toneSharpness, soloSharpness (c.from), soloSharpness (c.to),
                                      sharpest (d, c.at - 96 * block, c.at - block),
                                      sharpest (d, c.at + 48 * block, c.at + segment - block) }) + 1.0e-6f;
        const auto it = std::max_element (d.begin() + c.at - 16, d.begin() + c.at + 24 * block);
        const float ratio = *it / ref;
        worst = std::max (worst, ratio);
        const bool bad = ratio >= 1.25f;
        fails += bad ? 1 : 0;
        if (bad || verboseStatic())
            std::printf ("  %-4s %-40s %6.2f  (%.1f ms)\n", bad ? "FAIL" : "ok", c.name.toRawUTF8(), ratio,
                         (double) (it - d.begin() - c.at) / 48.0);
        // Diagnostic: VX_DUMP=<file> VX_DUMP_AT=<change name> writes that change's left
        // channel (raw float32, from 0.3 s before it) for plotting.
        if (const char* at = std::getenv ("VX_DUMP_AT"))
            if (c.name == juce::String (at))
                if (auto* f = std::fopen (std::getenv ("VX_DUMP"), "wb"))
                {
                    std::fwrite (rec.l.data() + c.at - 14400, sizeof (float), 14400 + (size_t) segment, f);
                    std::fclose (f);
                }
    }
    std::printf ("sound changes: %d of %d clicked, worst %.2f (limit 1.25)\n", fails, (int) changes.size(), worst);
    worstOut = worst;
    return fails;
}

// One preset through a transport that jumps back while playing, as a loop wrap does.
// The jump (2.37 beats) is deliberately off the grid of every synced division, so
// a synced LFO or slosh lands somewhere new (a whole-bar loop would hide that for
// divisions of a bar or less, but a 1-beat loop in Serato against a 1-bar LFO
// would not). With `gap`, the host also reports "stopped" for the block before
// the jump, as a restart over sound does.
// Returns the sharpest bend near the jump relative to a control render of the same
// preset without the jump (same moment), so a preset's own evolving texture (Tide
// Pool's random currents) is not mistaken for a loop click.
std::vector<float> loopRender (int presetIndex, bool jump, bool gap, int at, int total)
{
    VesselProcessor proc;
    TestPlayHead head;
    proc.setPlayHead (&head);
    const int block = 512;
    proc.prepareToPlay (48000.0, block);
    proc.presets.load (presetIndex);
    SoftTone tone;
    Recorder rec;
    juce::MidiBuffer midi;
    juce::AudioBuffer<float> buf (2, block);
    head.ppq = 8.0;
    for (int pos = 0; pos < total; pos += block)
    {
        if (jump && pos == at) head.ppq -= 2.37;
        head.playing = ! (gap && pos == at - block);
        tone.fill (buf);
        proc.processBlock (buf, midi);
        rec.add (buf);
        head.ppq += block * 2.0 / 48000.0;
    }
    return rec.bend();
}

float loopSpike (int presetIndex, bool gap)
{
    const int block = 512, at = 96 * block, total = at + 64 * block;
    const auto d = loopRender (presetIndex, true, gap, at, total);
    const auto c = loopRender (presetIndex, false, false, at, total);
    std::vector<float> ref (c.begin() + at / 3, c.begin() + at - 4800);
    std::sort (ref.begin(), ref.end());
    const float floorRef = ref[(size_t) (0.999 * (double) (ref.size() - 1))];
    const float control = std::max (floorRef, sharpest (c, at - block - 16, at + 2400)) + 1.0e-6f;
    return sharpest (d, at - block - 16, at + 2400) / control;
}

// A performance button switched on and off over sound, compared with the sound itself
// once settled in each state (Shatter's cracks and grit, the input returning after
// Seal are the design; a press must not be sharper than either settled state).
// Shatter Time is set to 0.5 s (its default; presets go up to 8 s risers) so the
// fully-shattered sound is reached and measured, and the ramp is at its steepest.
float toggleSpike (int presetIndex, P button)
{
    VesselProcessor proc;
    TestPlayHead head;
    head.playing = true;
    proc.setPlayHead (&head);
    const int block = 512, seg = 256 * block;   // ~2.7 s per state
    proc.prepareToPlay (48000.0, block);
    proc.presets.load (presetIndex);
    setParam (proc, P::shatterTime, 0.5f);
    SoftTone tone;
    Recorder rec;
    juce::MidiBuffer midi;
    juce::AudioBuffer<float> buf (2, block);
    std::vector<int> at;
    for (int k = 0; k < 5; ++k)
    {
        if (k > 0) { at.push_back ((int) rec.l.size()); setParam (proc, button, k % 2 == 1 ? 1.0f : 0.0f); }
        for (int done = 0; done < seg; done += block)
        {
            tone.fill (buf);
            proc.processBlock (buf, midi);
            rec.add (buf);
            head.ppq += block * 2.0 / 48000.0;
        }
    }
    // Diagnostic: VX_DUMP=<file> VX_DUMP_TOGGLE=<preset index>seal|shatter writes the whole render
    // (left channel, raw float32; toggles every 256 blocks).
    if (const char* which = std::getenv ("VX_DUMP_TOGGLE"))
        if (juce::String (which) == juce::String (presetIndex) + (button == P::seal ? "seal" : "shatter"))
            if (auto* f = std::fopen (std::getenv ("VX_DUMP"), "wb"))
            {
                std::fwrite (rec.l.data(), sizeof (float), rec.l.size(), f);
                std::fclose (f);
            }
    const auto d = rec.bend();
    float worst = 0.0f;
    for (size_t k = 0; k < at.size(); ++k)
    {
        const int a = at[k];
        const float ref = std::max (sharpest (d, a - seg + 64 * block, a - block), sharpest (d, a + 64 * block, a + seg - block)) + 1.0e-6f;
        const auto it = std::max_element (d.begin() + a - 16, d.begin() + a + 64 * block);
        const float r = *it / ref;
        worst = std::max (worst, r);
        if (std::getenv ("VX_TOGGLE_DETAIL") != nullptr && r >= 1.25f)   // detail from 1.25 up
            std::printf ("    preset %d %s %s: %.2f at %.1f ms\n", presetIndex, button == P::seal ? "Seal" : "Shatter",
                         k % 2 == 0 ? "on" : "off", r, (double) (it - d.begin() - a) / 48.0);
    }
    return worst;
}

constexpr float kToggleLimit = 1.5f;

// How long a sound change takes, from the preset load until the new sound is fully
// in (fade to dry, hold, swap, fade in). README promises about 50 ms.
int checkChangeTime()
{
    VesselProcessor proc;
    TestPlayHead head;
    head.playing = true;
    proc.setPlayHead (&head);
    const int block = 64;   // a small host block, so the time is measured finely
    proc.prepareToPlay (48000.0, block);
    proc.presets.load (1);
    SoftTone tone;
    juce::MidiBuffer midi;
    juce::AudioBuffer<float> buf (2, block);
    for (int b = 0; b < 200; ++b) { tone.fill (buf); proc.processBlock (buf, midi); }
    proc.presets.load (5);
    int samples = 0;
    bool started = false;
    for (int b = 0; b < 400; ++b)
    {
        tone.fill (buf);
        proc.processBlock (buf, midi);
        samples += block;
        started = started || proc.getSoundTransition() > 0.0f;
        if (started && proc.getSoundTransition() == 0.0f) break;
    }
    const double ms = samples / 48.0;
    std::printf ("sound change time: %.1f ms from load to fully in (limit 64)\n", ms);
    return started && ms <= 64.0 ? 0 : 1;
}

int checkStatic()
{
    int fails = checkChangeTime();
    float worstChange = 0.0f;
    fails += checkSoundChanges (worstChange);

    VesselProcessor probe;
    const auto& entries = probe.presets.getEntries();
    float worstWrap = 0.0f, worstGap = 0.0f, worstSeal = 0.0f, worstShatter = 0.0f;
    int wrapFails = 0, toggleFails = 0, presets = 0;
    for (int i = 0; i < entries.size(); ++i)
    {
        if (entries[i].factoryIndex < 0) continue;
        ++presets;
        const float wrap = loopSpike (i, false), gap = loopSpike (i, true);
        const float seal = toggleSpike (i, P::seal), shatter = toggleSpike (i, P::shatter);
        worstWrap = std::max (worstWrap, wrap); worstGap = std::max (worstGap, gap);
        worstSeal = std::max (worstSeal, seal); worstShatter = std::max (worstShatter, shatter);
        // Toggles: 1.5, not 1.25. Their settled sounds vary up to ~1.2x between two
        // stretches of themselves (Shatter's cracks, Noise excitation, shimmer), so 1.25
        // would leave no margin; 1.5 is that spread plus the same 25%. (Seal and Shatter
        // measured clean in 1.0.1 too: this is a guard against regressions.)
        const bool badLoop = wrap >= 2.5f || gap >= 2.5f, badToggle = seal >= kToggleLimit || shatter >= kToggleLimit;
        wrapFails += badLoop ? 1 : 0;
        toggleFails += badToggle ? 1 : 0;
        if (badLoop || badToggle || verboseStatic())
            std::printf ("  %-4s %-22s wrap %6.2f  restart %6.2f  seal %6.2f  shatter %6.2f\n",
                         badLoop || badToggle ? "FAIL" : "ok", entries[i].name.toRawUTF8(), wrap, gap, seal, shatter);
    }
    std::printf ("loops: %d of %d presets clicked, worst wrap %.2f, worst restart %.2f (limit 2.5)\n",
                 wrapFails, presets, worstWrap, worstGap);
    std::printf ("Seal/Shatter: %d of %d presets clicked, worst Seal %.2f, worst Shatter %.2f (limit 1.5)\n",
                 toggleFails, presets, worstSeal, worstShatter);
    fails += wrapFails + toggleFails;
    std::printf ("%s\n", fails == 0 ? "STATIC CHECK PASSED" : "STATIC CHECK FAILED");
    return fails == 0 ? 0 : 1;
}

// Diagnostic (not part of CI): switch preset `a` -> `b` over a held tone and report the
// switch's bend ratio, then again with each stage forced off in both presets, so the
// stage that makes a click stands out. VX_DUMP=<file> writes the left channel (raw
// float32, from 0.3 s before the switch) of the all-stages render.
float pairRatio (int a, int b, int stageOff, double& peakMs)
{
    VesselProcessor proc;
    TestPlayHead head;
    head.playing = true;
    proc.setPlayHead (&head);
    const int block = 512, segment = 144 * block;
    proc.prepareToPlay (48000.0, block);
    SoftTone tone;
    Recorder rec;
    juce::MidiBuffer midi;
    juce::AudioBuffer<float> buf (2, block);
    auto play = [&] (int samples)
    {
        for (int done = 0; done < samples; done += block)
        {
            tone.fill (buf);
            proc.processBlock (buf, midi);
            rec.add (buf);
            head.ppq += block * 2.0 / 48000.0;
        }
    };
    auto load = [&] (int i)
    {
        proc.presets.load (i);
        if (stageOff >= 0) setParam (proc, kStages[stageOff].on, 0.0f);
    };
    load (a);
    const auto from = proc.getPatchState();
    play (segment);
    const int at = (int) rec.l.size();
    load (b);
    const auto to = proc.getPatchState();
    play (segment);
    const auto d = rec.bend();
    const float ref = std::max (soloSharpness (from), soloSharpness (to)) + 1.0e-6f;
    const auto it = std::max_element (d.begin() + at - 16, d.begin() + at + segment - block);
    peakMs = (double) (it - d.begin() - at) / 48.0;
    if (stageOff < 0)
        if (const char* dump = std::getenv ("VX_DUMP"))
            if (auto* f = std::fopen (dump, "wb"))
            {
                std::fwrite (rec.l.data() + at - 14400, sizeof (float), 14400 + (size_t) segment, f);
                std::fclose (f);
            }
    return *it / ref;
}

int staticPair (int a, int b)
{
    VesselProcessor probe;
    const auto& e = probe.presets.getEntries();
    if (! juce::isPositiveAndBelow (a, e.size()) || ! juce::isPositiveAndBelow (b, e.size())) return 2;
    double ms = 0.0;
    std::printf ("%s -> %s\n", e[a].name.toRawUTF8(), e[b].name.toRawUTF8());
    const float all = pairRatio (a, b, -1, ms);
    std::printf ("  all stages         %6.2f at %5.1f ms\n", all, ms);
    for (int s = 0; s < kNumStages; ++s)
    {
        const float r = pairRatio (a, b, s, ms);
        std::printf ("  without %-10s %6.2f at %5.1f ms\n", kStages[s].name, r, ms);
    }
    return 0;
}

// Diagnostic (not part of CI): which settings make switch a -> b click? Applies preset a,
// then a patch that has only some of b's changed values (through applyPatchState, like
// a real preset load), and narrows that set down. Usage: vessel_check static-bisect <a> <b>
float subsetRatio (const juce::ValueTree& ta, const juce::ValueTree& tb, const std::vector<juce::String>& ids, bool matrix)
{
    VesselProcessor proc;
    TestPlayHead head;
    head.playing = true;
    proc.setPlayHead (&head);
    const int block = 512, segment = 48 * block;
    proc.prepareToPlay (48000.0, block);
    SoftTone tone;
    Recorder rec;
    juce::MidiBuffer midi;
    juce::AudioBuffer<float> buf (2, block);
    auto play = [&] (int samples)
    {
        for (int done = 0; done < samples; done += block)
        {
            tone.fill (buf);
            proc.processBlock (buf, midi);
            rec.add (buf);
            head.ppq += block * 2.0 / 48000.0;
        }
    };
    proc.applyPatchState (ta.createCopy(), true);
    play (segment);
    auto mixed = ta.createCopy();
    for (const auto& id : ids)
        mixed.getChildWithProperty ("id", id).setProperty ("value", tb.getChildWithProperty ("id", id).getProperty ("value"), nullptr);
    if (matrix)
    {
        auto m = mixed.getChildWithName ("MATRIX");
        m.copyPropertiesAndChildrenFrom (tb.getChildWithName ("MATRIX"), nullptr);
    }
    const int at = (int) rec.l.size();
    proc.applyPatchState (mixed, true);
    play (segment);
    const auto d = rec.bend();
    const float ref = std::max (sharpest (d, at - segment + 8 * block, at - block),
                                sharpest (d, at + 12 * block, at + segment - block)) + 1.0e-6f;
    return sharpest (d, at - 16, at + 12 * block) / ref;
}

// A preset index, or a Pour Me patch when negative (-seed).
juce::ValueTree patchFor (VesselProcessor& probe, int which)
{
    if (which >= 0) probe.presets.load (which);
    else probe.presets.randomize ((std::uint32_t) -which);
    return probe.getPatchState();
}

int staticBisect (int a, int b)
{
    VesselProcessor probe;
    const auto ta = patchFor (probe, a);
    const auto tb = patchFor (probe, b);
    std::vector<juce::String> diff;
    for (int i = 0; i < kNumParams; ++i)
    {
        const juce::String id (def (i).id);
        if ((float) ta.getChildWithProperty ("id", id).getProperty ("value") != (float) tb.getChildWithProperty ("id", id).getProperty ("value"))
            diff.push_back (id);
    }
    std::printf ("%d settings differ; all + matrix: %.2f, settings only: %.2f, matrix only: %.2f\n", (int) diff.size(),
                 subsetRatio (ta, tb, diff, true), subsetRatio (ta, tb, diff, false), subsetRatio (ta, tb, {}, true));
    // Greedy: drop each setting in turn while the switch still clicks.
    auto set = diff;
    const bool withMatrix = subsetRatio (ta, tb, set, false) < 1.25f;
    for (size_t k = 0; k < set.size();)
    {
        auto trial = set;
        trial.erase (trial.begin() + (long) k);
        if (subsetRatio (ta, tb, trial, withMatrix) >= 1.25f) set = trial; else ++k;
    }
    std::printf ("smallest clicking set (%s matrix): ", withMatrix ? "with" : "without");
    for (const auto& id : set) std::printf ("%s ", id.toRawUTF8());
    std::printf ("-> %.2f\n", subsetRatio (ta, tb, set, withMatrix));
    for (const auto& id : set)
        std::printf ("  %-14s %s -> %s\n", id.toRawUTF8(), ta.getChildWithProperty ("id", id).getProperty ("value").toString().toRawUTF8(),
                     tb.getChildWithProperty ("id", id).getProperty ("value").toString().toRawUTF8());
    return 0;
}

// Diagnostic (not part of CI): Seal and Shatter scores for one preset.
int toggleOne (int presetIndex)
{
    std::printf ("seal %.2f  shatter %.2f\n", toggleSpike (presetIndex, P::seal), toggleSpike (presetIndex, P::shatter));
    return 0;
}

// Diagnostic (not part of CI): render preset <i> over the soft tone for 3 s, with no
// changes, and write the left channel (raw float32) to <file>. Used to null-test that a
// settled preset sounds the same between two versions.
int renderOne (int presetIndex, const char* file)
{
    VesselProcessor proc;
    TestPlayHead head;
    head.playing = true;
    proc.setPlayHead (&head);
    proc.presets.load (presetIndex);       // before playback starts: not a sound change
    proc.prepareToPlay (48000.0, 512);
    SoftTone tone;
    Recorder rec;
    juce::MidiBuffer midi;
    juce::AudioBuffer<float> buf (2, 512);
    for (int b = 0; b < 282; ++b)
    {
        tone.fill (buf);
        proc.processBlock (buf, midi);
        rec.add (buf);
        head.ppq += 512 * 2.0 / 48000.0;
    }
    if (auto* f = std::fopen (file, "wb"))
    {
        std::fwrite (rec.l.data(), sizeof (float), rec.l.size(), f);
        std::fclose (f);
    }
    return 0;
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
    if (mode == "static")  return checkStatic();
    if (mode == "static-pair" && argc > 3)
        return staticPair (juce::String (argv[2]).getIntValue(), juce::String (argv[3]).getIntValue());
    if (mode == "static-render" && argc > 3)
        return renderOne (juce::String (argv[2]).getIntValue(), argv[3]);
    if (mode == "static-toggle" && argc > 2)
        return toggleOne (juce::String (argv[2]).getIntValue());
    if (mode == "static-bisect" && argc > 3)
        return staticBisect (juce::String (argv[2]).getIntValue(), juce::String (argv[3]).getIntValue());

    std::printf ("usage: vessel_check snapshot <out.png> [width] [easy|vessel|fx|mod|global] [preset] | presets | state | static | static-pair <a> <b>\n");
    return 2;
}
