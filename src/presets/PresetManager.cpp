#include "PresetManager.h"
#include "../PluginProcessor.h"

namespace vessel
{
using namespace juce;

PresetManager::PresetManager (VesselProcessor& p) : proc (p)
{
    refreshUserPresets();
}

File PresetManager::getUserPresetFolder()
{
    // Windows: Documents\Vessel\Presets (visible, easy to back up or share)
    return File::getSpecialLocation (File::userDocumentsDirectory).getChildFile ("Vessel").getChildFile ("Presets");
}

void PresetManager::refreshUserPresets()
{
    const auto previous = getCurrentName();
    entries.clear();

    const auto& fps = factoryPresets();
    for (int i = 0; i < (int) fps.size(); ++i)
        entries.add ({ fps[(size_t) i].name, fps[(size_t) i].category, fps[(size_t) i].description, i, {} });

    auto folder = getUserPresetFolder();
    if (folder.isDirectory())
    {
        auto files = folder.findChildFiles (File::findFiles, false, String ("*") + kExtension);
        files.sort();
        for (const auto& f : files)
            entries.add ({ f.getFileNameWithoutExtension(), "User", "Your preset", -1, f });
    }

    for (int k = 0; k < entries.size(); ++k)
        if (entries[k].name == previous) { currentIndex = k; break; }
    sendChangeMessage();
}

String PresetManager::getCurrentName() const
{
    const auto stateName = proc.apvts.state.getProperty (VesselProcessor::kPresetNameProp).toString();
    if (stateName.isNotEmpty()) return stateName;
    return isPositiveAndBelow (currentIndex, entries.size()) ? entries[currentIndex].name : String ("Init");
}

String PresetManager::getCurrentDescription() const
{
    if (isPositiveAndBelow (currentIndex, entries.size()) && entries[currentIndex].name == getCurrentName())
        return entries[currentIndex].description;
    return "Your own vessel. Drag the water line, turn the macros, or try Seal and Shatter.";
}

void PresetManager::step (int delta)
{
    if (entries.isEmpty()) return;
    load ((currentIndex + delta + entries.size()) % entries.size());
}

void PresetManager::applyTree (const ValueTree& tree, const String& name)
{
    proc.undo.beginNewTransaction ("Load " + name);
    auto t = tree.createCopy();
    t.setProperty (VesselProcessor::kPresetNameProp, name, nullptr);
    proc.applyPatchState (t, true);
}

void PresetManager::load (int index)
{
    if (! isPositiveAndBelow (index, entries.size())) return;
    const auto& e = entries.getReference (index);

    if (e.factoryIndex >= 0)
        applyTree (buildFactoryPatch (factoryPresets()[(size_t) e.factoryIndex]), e.name);
    else if (auto xml = parseXML (e.file))
    {
        auto tree = ValueTree::fromXml (*xml);
        if (tree.hasType (proc.apvts.state.getType()))
            applyTree (tree, e.name);
    }

    currentIndex = index;
    sendChangeMessage();
}

ValueTree PresetManager::buildFactoryPatch (const FactoryPreset& fp) const
{
    ValueTree t (proc.apvts.state.getType());

    std::array<float, kNumParams> v {};
    for (int i = 0; i < kNumParams; ++i) v[(size_t) i] = def (i).def;
    for (const auto& [p, val] : fp.values) v[(size_t) idx (p)] = val;

    for (int i = 0; i < kNumParams; ++i)
    {
        ValueTree c ("PARAM");
        c.setProperty ("id", def (i).id, nullptr);
        c.setProperty ("value", v[(size_t) i], nullptr);
        t.appendChild (c, nullptr);
    }

    // Matrix: the standard macro kit for the stages in use, then the preset's own routes.
    std::vector<Route> routes;
    if (fp.macroKit)
    {
        std::array<bool, kNumStages> used {};
        for (int s = 0; s < kNumStages; ++s) used[(size_t) s] = v[(size_t) idx (kStages[s].on)] > 0.5f;
        routes = macroKit (used);
    }
    routes.insert (routes.end(), fp.routes.begin(), fp.routes.end());
    jassert (routes.size() <= (size_t) kMatrixSlots);

    ValueTree matrix (VesselProcessor::kMatrixType);
    for (int s = 0; s < kMatrixSlots; ++s)
    {
        ValueTree slot ("SLOT");
        const bool has = s < (int) routes.size();
        slot.setProperty ("src", has ? (int) routes[(size_t) s].src : 0, nullptr);
        slot.setProperty ("dst", has ? String (def (routes[(size_t) s].dst).id) : String(), nullptr);
        slot.setProperty ("amt", has ? routes[(size_t) s].amount : 0.0f, nullptr);
        matrix.appendChild (slot, nullptr);
    }
    t.appendChild (matrix, nullptr);
    return t;
}

bool PresetManager::saveUserPreset (const String& rawName)
{
    const auto name = File::createLegalFileName (rawName.trim());
    if (name.isEmpty()) return false;
    auto folder = getUserPresetFolder();
    if (! folder.createDirectory()) return false;

    auto state = proc.getPatchState();
    state.setProperty (VesselProcessor::kPresetNameProp, name, nullptr);
    auto xml = state.createXml();
    const auto file = folder.getChildFile (name + kExtension);
    if (xml == nullptr || ! xml->writeTo (file)) return false;

    proc.apvts.state.setProperty (VesselProcessor::kPresetNameProp, name, nullptr);
    refreshUserPresets();
    for (int k = 0; k < entries.size(); ++k)
        if (entries[k].file == file) { currentIndex = k; break; }
    sendChangeMessage();
    return true;
}

// ---------------------------------------------------------------------------
// "Pour Me": a random vessel that is always playable and never runs away.
void PresetManager::randomize (std::uint32_t seed)
{
    Random rng ((int64) seed * 7919 + 17);
    auto rf = [&] (float a, float b) { return a + rng.nextFloat() * (b - a); };
    auto ri = [&] (int n) { return rng.nextInt (n); };

    FactoryPreset fp { "Poured", "Random", "A random vessel. Drag the water line, try the macros, Seal and Shatter.", {}, {} };
    auto& v = fp.values;

    // Body: always on, always tuned to something.
    const int tune = rng.nextFloat() < 0.35f ? 2 : 1;
    v.push_back ({ P::bdMaterial, (float) ri (8) });
    v.push_back ({ P::bdTune, (float) tune });
    v.push_back ({ P::bdRoot, (float) ri (12) });
    v.push_back ({ P::bdOctave, (float) (2 + ri (3)) });
    v.push_back ({ P::bdChord, (float) ri (12) });
    v.push_back ({ P::bdMix, rf (0.35f, 0.8f) });
    v.push_back ({ P::bdDecay, std::exp (rf (std::log (0.3f), std::log (8.0f))) });
    v.push_back ({ P::bdDamping, rf (0.1f, 0.7f) });
    v.push_back ({ P::bdBright, rf (-0.5f, 0.5f) });
    v.push_back ({ P::bdInharm, rng.nextFloat() < 0.6f ? 0.0f : rf (-0.5f, 0.6f) });
    v.push_back ({ P::bdSpread, rf (0.2f, 0.8f) });
    v.push_back ({ P::bdExcite, (float) (rng.nextFloat() < 0.6f ? 0 : ri (3)) });
    v.push_back ({ P::fill, rng.nextFloat() < 0.5f ? 0.0f : rf (0.1f, 0.6f) });
    v.push_back ({ P::poStrike, rf (-0.2f, 0.5f) });
    v.push_back ({ P::poDrive, rng.nextFloat() < 0.6f ? 0.0f : rf (0.1f, 0.5f) });
    v.push_back ({ P::poLowCut, rf (20.0f, 200.0f) });

    // Two or three of the colour stages.
    const bool fr = rng.nextFloat() < 0.4f, lq = rng.nextFloat() < 0.6f, ec = rng.nextFloat() < 0.55f, sp = rng.nextFloat() < 0.75f;
    if (fr)
    {
        v.push_back ({ P::frOn, 1.0f });
        v.push_back ({ P::frMix, rf (0.3f, 0.8f) });
        v.push_back ({ P::frCrack, rf (0.1f, 0.6f) });
        v.push_back ({ P::frGrit, rng.nextFloat() < 0.5f ? 0.0f : rf (0.1f, 0.5f) });
        v.push_back ({ P::frFold, rf (0.0f, 0.5f) });
        v.push_back ({ P::frShard, rng.nextFloat() < 0.6f ? 0.0f : rf (0.1f, 0.4f) });
        v.push_back ({ P::frPos, (float) ri (2) });
    }
    if (lq)
    {
        v.push_back ({ P::lqOn, 1.0f });
        v.push_back ({ P::lqRate, std::exp (rf (std::log (0.08f), std::log (3.0f))) });
        v.push_back ({ P::lqSlosh, rf (0.2f, 0.8f) });
        v.push_back ({ P::lqRipple, rf (0.1f, 0.7f) });
        v.push_back ({ P::lqMix, rf (0.2f, 0.6f) });
        v.push_back ({ P::lqTurb, rng.nextFloat() < 0.5f ? 0.0f : rf (0.1f, 0.7f) });
    }
    if (ec)
    {
        static constexpr int divs[] = { dsp::Div::d4, dsp::Div::d8D, dsp::Div::d8, dsp::Div::d4D, dsp::Div::d16 };
        v.push_back ({ P::ecOn, 1.0f });
        v.push_back ({ P::ecDiv, (float) divs[ri (5)] });
        v.push_back ({ P::ecMix, rf (0.15f, 0.4f) });
        v.push_back ({ P::ecFeedback, rf (0.2f, 0.6f) });
        v.push_back ({ P::ecReturn, rng.nextFloat() < 0.5f ? 0.0f : rf (0.1f, 0.5f) });
        v.push_back ({ P::ecPingPong, rng.nextBool() ? 1.0f : 0.0f });
    }
    if (sp)
    {
        v.push_back ({ P::spOn, 1.0f });
        v.push_back ({ P::spMode, (float) (rng.nextFloat() < 0.7f ? ri (2) : 2 + ri (2)) });
        v.push_back ({ P::spMix, rf (0.15f, 0.4f) });
        v.push_back ({ P::spDecay, rf (1.2f, 7.0f) });
        v.push_back ({ P::spSize, rf (0.3f, 0.9f) });
        v.push_back ({ P::spShimmer, rng.nextFloat() < 0.7f ? 0.0f : rf (0.1f, 0.5f) });
    }

    v.push_back ({ P::lfo1Shape, (float) ri (7) });
    v.push_back ({ P::lfo1Sync, 1.0f });
    v.push_back ({ P::lfo1Div, (float) jlimit (0, dsp::kNumDivisions - 1, 1 + ri (9)) });

    // One signature LFO route into the body.
    static constexpr P bodyTargets[] = { P::bdBright, P::bdDecay, P::fill, P::bdInharm, P::bdSpread };
    fp.routes.push_back ({ Src::lfo1, bodyTargets[ri (5)], rf (0.08f, 0.22f) * (rng.nextBool() ? 1.0f : -1.0f) });

    applyTree (buildFactoryPatch (fp), "Poured #" + String ((int) (seed % 10000)));
    currentIndex = -1;
    sendChangeMessage();
}

// ---------------------------------------------------------------------------
void PresetManager::selectSlot (int slot)
{
    slot = slot == 0 ? 0 : 1;
    if (slot == activeSlot) return;
    slots[activeSlot] = proc.getPatchState();
    if (! slots[slot].isValid()) slots[slot] = slots[activeSlot].createCopy();
    proc.undo.beginNewTransaction ("A/B");
    proc.applyPatchState (slots[slot].createCopy(), true);
    activeSlot = slot;
    sendChangeMessage();
}

void PresetManager::copyActiveToOther()
{
    slots[1 - activeSlot] = proc.getPatchState();
    sendChangeMessage();
}

void PresetManager::restoreSelectionFromState()
{
    const auto name = proc.apvts.state.getProperty (VesselProcessor::kPresetNameProp).toString();
    for (int k = 0; k < entries.size(); ++k)
        if (entries[k].name == name) { currentIndex = k; break; }
    sendChangeMessage();
}

} // namespace vessel
