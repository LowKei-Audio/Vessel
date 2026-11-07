#pragma once
// The two faces of Vessel:
//   EasyView - the living vessel, material tiles, tuning, Seal and Shatter,
//              four macros and stage switches. Every preset plays from here.
//   ProView  - VESSEL / FX / MOD / GLOBAL pages for deep sound design, with
//              the vessel, Seal, Shatter and the macros always in reach.

#include "Pages.h"
#include "VesselView.h"

namespace vessel
{

inline PerformButton* makeSeal (VesselProcessor& p)
{
    return new PerformButton (p, P::seal, "Seal", "sealLatch", theme::ice, [&p] { return p.uiSeal.load(); }, "Hold to seal",
                              "Seal: what is ringing keeps ringing, nothing new gets in. HOLD = while pressed, LATCH = click on / off");
}
inline PerformButton* makeShatter (VesselProcessor& p)
{
    return new PerformButton (p, P::shatter, "Shatter", "shatterLatch", theme::shatter, [&p] { return p.uiShatter.load(); },
                              "Hold to break", "Shatter: ramps the vessel into pieces (Shatter Time on GLOBAL). HOLD = while pressed, LATCH = click on / off");
}

// ---------------------------------------------------------------------------
class EasyView : public juce::Component
{
public:
    explicit EasyView (VesselProcessor& p)
        : proc (p), store (p), materials (p), vessel (p), seal (makeSeal (p)), shatter (makeShatter (p)),
          sealMode (*seal), shatterMode (*shatter), macros (p, Knob::Size::large), stages (p)
    {
        const auto b = theme::stage (1);
        tune = store.segment (P::bdTune, Choices::tuneModes, b);
        root = store.choice (P::bdRoot, b);
        octave = store.segment (P::bdOctave, { "1", "2", "3", "4", "5", "6" }, b, "Octave", 1);
        chord = store.choice (P::bdChord, b);
        excite = store.segment (P::bdExcite, { "Input", "Hits", "Noise" }, b);
        pitch = store.knob (P::bdPitch, b);
        decay = store.knob (P::bdDecay, b);
        bodyMix = store.knob (P::bdMix, b);
        spread = store.knob (P::bdSpread, b);
        mix = store.knob (P::mix, theme::global);
        output = store.knob (P::outGain, theme::global);

        for (auto* c : std::initializer_list<juce::Component*> { &materials, tune, root, octave, chord, excite, pitch, decay, bodyMix, spread,
                                                                 &vessel, seal.get(), shatter.get(), &sealMode, &shatterMode,
                                                                 &macros, &stages, mix, output })
            addAndMakeVisible (c);
    }

    void tick()
    {
        store.tick();
        vessel.tick();
        seal->tick();
        shatter->tick();
        sealMode.refresh();
        shatterMode.refresh();
        macros.tick();
        // Only the controls that matter for the current tuning are active.
        const int t = juce::roundToInt (proc.apvts.getRawParameterValue (def (P::bdTune).id)->load());
        pitch->setEnabled (t == 0);
        chord->setEnabled (t == 2);
        root->setEnabled (t != 0);
        octave->setEnabled (t != 0);
    }

    void paint (juce::Graphics& g) override
    {
        drawPanel (g, leftPanel.toFloat(), theme::stage (1));
        drawPanel (g, rightPanel.toFloat(), theme::aqua);
        auto title = [&] (const char* s, juce::Rectangle<int> r)
        {
            theme::legend (g, s, r.toFloat(), theme::text, 13.0f, juce::Justification::centredLeft, true, 0.24f);
        };
        title ("Vessel", leftPanel.reduced (18, 10).removeFromTop (22));
        title ("Macros", rightPanel.reduced (18, 10).removeFromTop (22));
        title ("Stages", stagesTitle);
    }

    void resized() override
    {
        auto r = getLocalBounds();
        leftPanel = r.removeFromLeft (330);
        rightPanel = r.removeFromRight (330);
        auto centre = r.reduced (14, 0);

        auto l = leftPanel.reduced (14, 10);
        l.removeFromTop (28);
        materials.setBounds (l.removeFromTop (150));
        l.removeFromTop (8);
        tune->setBounds (l.removeFromTop (50).reduced (2, 0));
        auto row = l.removeFromTop (54);
        root->setBounds (row.removeFromLeft (row.getWidth() / 3).reduced (2, 2));
        octave->setBounds (row.reduced (2, 2));
        row = l.removeFromTop (54);
        chord->setBounds (row.removeFromLeft (row.getWidth() / 2).reduced (2, 2));
        excite->setBounds (row.reduced (2, 2));
        l.removeFromTop (juce::jmax (8, (l.getHeight() - 112) / 2));
        auto knobs = l.removeFromTop (112);
        const int kw = knobs.getWidth() / 4;
        for (auto* k : { pitch, decay, bodyMix, spread }) k->setBounds (knobs.removeFromLeft (kw).reduced (1, 0));

        auto perf = centre.removeFromBottom (120);
        centre.removeFromBottom (10);
        vessel.setBounds (centre);
        auto half = perf.removeFromLeft (perf.getWidth() / 2);
        seal->setBounds (half.removeFromTop (84).reduced (6, 0));
        sealMode.setBounds (half.removeFromTop (30).reduced (40, 2));
        shatter->setBounds (perf.removeFromTop (84).reduced (6, 0));
        shatterMode.setBounds (perf.removeFromTop (30).reduced (40, 2));

        auto rr = rightPanel.reduced (12, 10);
        rr.removeFromTop (28);
        macros.setBounds (rr.removeFromTop (318));
        rr.removeFromTop (6);
        stagesTitle = rr.removeFromTop (20).withTrimmedLeft (6);
        stages.setBounds (rr.removeFromTop (64));
        rr.removeFromTop (6);
        mix->setBounds (rr.removeFromLeft (rr.getWidth() / 2).reduced (12, 0));
        output->setBounds (rr.reduced (12, 0));
    }

private:
    VesselProcessor& proc;
    ControlStore store;
    MaterialPicker materials;
    VesselView vessel;
    std::unique_ptr<PerformButton> seal, shatter;
    PerformModeSwitch sealMode, shatterMode;
    MacroBank macros;
    StageSwitches stages;
    juce::Component *tune = nullptr, *root = nullptr, *octave = nullptr, *chord = nullptr, *excite = nullptr;
    Knob *pitch = nullptr, *decay = nullptr, *bodyMix = nullptr, *spread = nullptr, *mix = nullptr, *output = nullptr;
    juce::Rectangle<int> leftPanel, rightPanel, stagesTitle;
};

// ---------------------------------------------------------------------------
class ProView : public juce::Component
{
public:
    explicit ProView (VesselProcessor& p)
        : vesselPage (p), fxPage (p), modPage (p), globalPage (p), vessel (p), seal (makeSeal (p)), shatter (makeShatter (p)),
          macros (p, Knob::Size::medium)
    {
        const char* names[4] = { "VESSEL", "FX", "MOD", "GLOBAL" };
        for (int i = 0; i < 4; ++i)
        {
            tabs[i].setButtonText (names[i]);
            tabs[i].setClickingTogglesState (false);
            tabs[i].setMouseCursor (juce::MouseCursor::PointingHandCursor);
            tabs[i].onClick = [this, i] { showPage (i); };
            addAndMakeVisible (tabs[i]);
        }
        tabs[0].setTooltip ("Pour and the Body: material, tuning, decay, warp and modes");
        tabs[1].setTooltip ("Fracture, Liquid, Echo and Space");
        tabs[2].setTooltip ("LFOs, envelope, drift and the modulation matrix");
        tabs[3].setTooltip ("Levels, output, Seal and Shatter settings, tempo and seed");
        vessel.setCompact (true);
        seal->setCompact (true);
        shatter->setCompact (true);
        for (auto* c : std::initializer_list<juce::Component*> { &vesselPage, &fxPage, &modPage, &globalPage, &vessel,
                                                                 seal.get(), shatter.get(), &macros })
            addAndMakeVisible (c);
        showPage (0);
    }

    void showPage (int page)
    {
        current = juce::jlimit (0, 3, page);
        juce::Component* pages[4] = { &vesselPage, &fxPage, &modPage, &globalPage };
        for (int i = 0; i < 4; ++i)
        {
            pages[i]->setVisible (i == current);
            tabs[i].setToggleState (i == current, juce::dontSendNotification);
        }
    }

    void tick()
    {
        switch (current)
        {
            case 0: vesselPage.tick(); break;
            case 1: fxPage.tick(); break;
            case 2: modPage.tick(); break;
            default: globalPage.tick(); break;
        }
        vessel.tick();
        seal->tick();
        shatter->tick();
        macros.tick();
    }

    void paint (juce::Graphics& g) override
    {
        drawPanel (g, sidePanel.toFloat(), theme::aqua);
        theme::legend (g, "Macros", macroTitle.toFloat(), theme::text, 12.0f, juce::Justification::centredLeft, true, 0.24f);
    }

    void resized() override
    {
        auto r = getLocalBounds();
        sidePanel = r.removeFromRight (300);
        r.removeFromRight (10);
        auto tabRow = r.removeFromTop (34);
        for (auto& t : tabs) { t.setBounds (tabRow.removeFromLeft (110).reduced (2, 2)); }
        r.removeFromTop (8);
        for (auto* pg : std::initializer_list<juce::Component*> { &vesselPage, &fxPage, &modPage, &globalPage }) pg->setBounds (r);

        auto s = sidePanel.reduced (12, 12);
        vessel.setBounds (s.removeFromTop (250));
        s.removeFromTop (8);
        auto perf = s.removeFromTop (52);
        seal->setBounds (perf.removeFromLeft (perf.getWidth() / 2).reduced (2, 0));
        shatter->setBounds (perf.reduced (2, 0));
        s.removeFromTop (8);
        macroTitle = s.removeFromTop (20).withTrimmedLeft (6);
        macros.setBounds (s);
    }

private:
    juce::TextButton tabs[4];
    VesselPage vesselPage;
    FxPage fxPage;
    ModPage modPage;
    GlobalPage globalPage;
    VesselView vessel;
    std::unique_ptr<PerformButton> seal, shatter;
    MacroBank macros;
    juce::Rectangle<int> sidePanel, macroTitle;
    int current = 0;
};

} // namespace vessel
