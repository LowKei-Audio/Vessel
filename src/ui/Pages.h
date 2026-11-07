#pragma once
// The Pro view's four pages:
//   VESSEL - Pour and the Body (material, tuning, decay, warp, modes...)
//   FX     - Fracture, Liquid, Echo and Space
//   MOD    - LFO 1, LFO 2, Envelope, Drift and the 16-slot matrix
//   GLOBAL - levels, output, performance settings, tempo, seed, about

#include "Widgets.h"

namespace vessel
{

// ---------------------------------------------------------------------------
// A titled panel with an optional power LED; children laid out in rows of
// equal-width cells.
class Panel : public juce::Component
{
public:
    Panel (juce::String titleText, juce::Colour accentColour, juce::Component* powerSwitch = nullptr)
        : title (std::move (titleText)), accent (accentColour), power (powerSwitch)
    {
        if (power != nullptr) addAndMakeVisible (*power);
    }

    void addRow (std::vector<juce::Component*> row, float weight = 1.0f)
    {
        for (auto* c : row) if (c != nullptr) addAndMakeVisible (c);
        rows.push_back ({ std::move (row), weight });
    }

    void paint (juce::Graphics& g) override
    {
        drawPanel (g, getLocalBounds().toFloat(), accent);
        if (power == nullptr)
            theme::legend (g, title, getLocalBounds().toFloat().reduced (16.0f, 10.0f).removeFromTop (20.0f), theme::text, 13.0f,
                           juce::Justification::centredLeft, true, 0.24f);
    }

    void resized() override
    {
        auto r = getLocalBounds().reduced (12, 8);
        auto head = r.removeFromTop (26);
        if (power != nullptr) power->setBounds (head.removeFromLeft (180).withTrimmedLeft (4));
        r.removeFromTop (4);
        float total = 0.0f;
        for (auto& row : rows) total += row.weight;
        float y = (float) r.getY();
        for (auto& row : rows)
        {
            const float h = (float) r.getHeight() * row.weight / juce::jmax (0.001f, total);
            auto rr = juce::Rectangle<float> ((float) r.getX(), y, (float) r.getWidth(), h).toNearestInt();
            y += h;
            const int n = (int) row.cells.size();
            for (int i = 0; i < n; ++i)
                if (auto* c = row.cells[(size_t) i])
                    c->setBounds (rr.withX (rr.getX() + rr.getWidth() * i / n).withWidth (rr.getWidth() / n).reduced (3, 2));
        }
    }

private:
    struct Row { std::vector<juce::Component*> cells; float weight; };
    juce::String title;
    juce::Colour accent;
    juce::Component* power;
    std::vector<Row> rows;
};

// Owns knobs and other controls so pages can declare them in one line each.
class ControlStore
{
public:
    explicit ControlStore (VesselProcessor& p) : proc (p) {}

    Knob* knob (P p, juce::Colour c, juce::String legend = {}, Knob::Size s = Knob::Size::medium)
    {
        knobs.push_back (std::make_unique<Knob> (proc, p, c, s, legend));
        return knobs.back().get();
    }
    LedButton* led (P p, juce::Colour c, bool large = false, juce::String legend = {})
    {
        leds.push_back (std::make_unique<LedButton> (proc.apvts, p, c, large, legend));
        return leds.back().get();
    }
    ChoiceBox* choice (P p, juce::Colour c, juce::String legend = {})
    {
        choices.push_back (std::make_unique<ChoiceBox> (proc.apvts, p, c, legend));
        return choices.back().get();
    }
    juce::Component* segment (P p, juce::StringArray labels, juce::Colour c, juce::String legend = {}, int firstValue = 0)
    {
        segments.push_back (std::make_unique<SegmentSwitch> (proc.apvts, p, std::move (labels), c, firstValue));
        labelled.push_back (std::make_unique<Labelled> (legend.isNotEmpty() ? legend : juce::String (def (p).label), *segments.back()));
        return labelled.back().get();
    }
    juce::Component* labelledLed (P p, juce::Colour c, juce::String legend = {})
    {
        auto* l = led (p, c, false, "On");
        labelled.push_back (std::make_unique<Labelled> (legend.isNotEmpty() ? legend : juce::String (def (p).label), *l));
        return labelled.back().get();
    }
    SyncedRate* rate (P r, P sync, P div, juce::Colour c, juce::String legend = {})
    {
        rates.push_back (std::make_unique<SyncedRate> (proc, r, sync, div, c, legend));
        return rates.back().get();
    }
    void tick()
    {
        for (auto& k : knobs) k->tick();
        for (auto& r : rates) r->tick();
    }
    Knob* findKnob (P p)
    {
        for (auto& k : knobs) if (k->paramIndex() == idx (p)) return k.get();
        return nullptr;
    }

private:
    VesselProcessor& proc;
    std::vector<std::unique_ptr<Knob>> knobs;
    std::vector<std::unique_ptr<LedButton>> leds;
    std::vector<std::unique_ptr<ChoiceBox>> choices;
    std::vector<std::unique_ptr<SegmentSwitch>> segments;
    std::vector<std::unique_ptr<Labelled>> labelled;
    std::vector<std::unique_ptr<SyncedRate>> rates;
};

// ---------------------------------------------------------------------------
class VesselPage : public juce::Component
{
public:
    explicit VesselPage (VesselProcessor& p)
        : proc (p), store (p),
          pour ("Pour", theme::stage (0), store.led (P::poOn, theme::stage (0), true, "Pour")),
          body ("Body", theme::stage (1), store.led (P::bdOn, theme::stage (1), true, "Body"))
    {
        const auto a = theme::stage (0), b = theme::stage (1);
        pour.addRow ({ store.knob (P::poDrive, a), store.knob (P::poStrike, a), store.knob (P::poTilt, a), store.knob (P::poLowCut, a) });

        root = store.choice (P::bdRoot, b);
        octave = store.segment (P::bdOctave, { "1", "2", "3", "4", "5", "6" }, b, "Octave", 1);
        chord = store.choice (P::bdChord, b);
        body.addRow ({ store.choice (P::bdMaterial, b), store.segment (P::bdTune, Choices::tuneModes, b), root, octave,
                       chord, store.segment (P::bdExcite, { "Input", "Hits", "Noise" }, b) }, 0.6f);
        body.addRow ({ store.knob (P::bdPitch, b), store.knob (P::bdFine, b), store.knob (P::bdDecay, b),
                       store.knob (P::bdDamping, b), store.knob (P::bdBright, b), store.knob (P::bdInharm, b) });
        body.addRow ({ store.knob (P::bdSpread, b), store.knob (P::bdModes, b), store.knob (P::bdGlide, b),
                       store.knob (P::fill, b), store.knob (P::bdMix, b), nullptr });
        addAndMakeVisible (pour);
        addAndMakeVisible (body);
    }

    void tick()
    {
        store.tick();
        // Free pitch only matters in Free tuning; root/chord only in Note/Chord.
        const int tune = juce::roundToInt (proc.apvts.getRawParameterValue (def (P::bdTune).id)->load());
        if (auto* k = store.findKnob (P::bdPitch)) k->setEnabled (tune == 0);
        root->setEnabled (tune != 0);
        octave->setEnabled (tune != 0);
        chord->setEnabled (tune == 2);
    }

    void resized() override
    {
        auto r = getLocalBounds();
        pour.setBounds (r.removeFromTop (150));
        r.removeFromTop (10);
        body.setBounds (r);
    }

private:
    VesselProcessor& proc;
    ControlStore store;
    Panel pour, body;
    juce::Component *root = nullptr, *octave = nullptr, *chord = nullptr;
};

// ---------------------------------------------------------------------------
class FxPage : public juce::Component
{
public:
    explicit FxPage (VesselProcessor& p)
        : store (p),
          fracture ("Fracture", theme::stage (2), store.led (P::frOn, theme::stage (2), true, "Fracture")),
          liquid ("Liquid", theme::stage (3), store.led (P::lqOn, theme::stage (3), true, "Liquid")),
          echo ("Echo", theme::stage (4), store.led (P::ecOn, theme::stage (4), true, "Echo")),
          space ("Space", theme::stage (5), store.led (P::spOn, theme::stage (5), true, "Space"))
    {
        const auto f = theme::stage (2), l = theme::stage (3), e = theme::stage (4), s = theme::stage (5);
        fracture.addRow ({ store.segment (P::frPos, { "Post", "Pre" }, f), store.knob (P::frCrack, f), store.knob (P::frGrit, f), store.knob (P::frFold, f) });
        fracture.addRow ({ store.knob (P::frShard, f), store.knob (P::frTone, f), store.knob (P::frMix, f), nullptr });

        liquid.addRow ({ store.labelledLed (P::lqSync, l), store.rate (P::lqRate, P::lqSync, P::lqDiv, l),
                         store.knob (P::lqSlosh, l), store.knob (P::lqRipple, l) });
        liquid.addRow ({ store.knob (P::lqTurb, l), store.knob (P::lqMix, l), nullptr, nullptr });

        echo.addRow ({ store.labelledLed (P::ecSync, e), store.rate (P::ecTime, P::ecSync, P::ecDiv, e, "Time"),
                       store.knob (P::ecFeedback, e), store.knob (P::ecTone, e) });
        echo.addRow ({ store.knob (P::ecWobble, e), store.knob (P::ecReturn, e), store.labelledLed (P::ecPingPong, e), store.knob (P::ecMix, e) });

        space.addRow ({ store.choice (P::spMode, s), store.knob (P::spSize, s),
                        store.knob (P::spDecay, s), store.knob (P::spPredelay, s), store.knob (P::spMix, s) });
        space.addRow ({ store.knob (P::spDamp, s), store.knob (P::spLowCut, s), store.knob (P::spShimmer, s),
                        store.knob (P::spMod, s), nullptr });

        for (auto* c : { &fracture, &liquid, &echo, &space }) addAndMakeVisible (c);
    }

    void tick() { store.tick(); }

    void resized() override
    {
        auto r = getLocalBounds();
        auto top = r.removeFromTop ((r.getHeight() - 10) / 2);
        r.removeFromTop (10);
        fracture.setBounds (top.removeFromLeft ((top.getWidth() - 10) / 2));
        top.removeFromLeft (10);
        liquid.setBounds (top);
        echo.setBounds (r.removeFromLeft ((r.getWidth() - 10) / 2));
        r.removeFromLeft (10);
        space.setBounds (r);
    }

private:
    ControlStore store;
    Panel fracture, liquid, echo, space;
};

// ---------------------------------------------------------------------------
// Live meter for a modulation source (bipolar or 0..1).
class SourceMeter : public juce::Component
{
public:
    SourceMeter (std::function<float()> fn, bool bipolarSource, juce::Colour c) : get (std::move (fn)), bipolar (bipolarSource), colour (c) {}
    void tick() { const float v = get(); if (std::abs (v - value) > 0.003f) { value = v; repaint(); } }
    void paint (juce::Graphics& g) override
    {
        auto r = getLocalBounds().toFloat().withSizeKeepingCentre ((float) getWidth(), 8.0f);
        g.setColour (theme::well);
        g.fillRoundedRectangle (r, 4.0f);
        const float zero = bipolar ? r.getCentreX() : r.getX();
        const float x = bipolar ? r.getCentreX() + 0.5f * r.getWidth() * juce::jlimit (-1.0f, 1.0f, value)
                                : r.getX() + r.getWidth() * juce::jlimit (0.0f, 1.0f, value);
        g.setColour (colour);
        g.fillRoundedRectangle (juce::Rectangle<float> (juce::jmin (zero, x), r.getY(), juce::jmax (2.0f, std::abs (x - zero)), r.getHeight()), 4.0f);
    }

private:
    std::function<float()> get;
    bool bipolar;
    juce::Colour colour;
    float value = 0.0f;
};

// One matrix slot: [n] [source] [destination] [amount] [x]
class MatrixRow : public juce::Component
{
public:
    MatrixRow (VesselProcessor& p, int slotIndex) : proc (p), slot (slotIndex)
    {
        for (auto* b : { &srcBtn, &dstBtn, &clearBtn }) { addAndMakeVisible (b); b->setMouseCursor (juce::MouseCursor::PointingHandCursor); }
        amount.setSliderStyle (juce::Slider::LinearHorizontal);
        amount.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
        amount.setRange (-1.0, 1.0, 0.001);
        amount.setDoubleClickReturnValue (true, 0.0);
        amount.setColour (juce::Slider::trackColourId, theme::aqua);
        amount.setPopupDisplayEnabled (true, true, nullptr);
        amount.textFromValueFunction = [] (double v) { return (v > 0 ? "+" : "") + juce::String (juce::roundToInt (v * 100.0)) + "%"; };
        addAndMakeVisible (amount);
        amount.onDragStart = [this] { proc.undo.beginNewTransaction ("Mod amount"); };
        amount.onValueChange = [this]
        {
            auto s = proc.getSlot (slot);
            if (std::abs (s.amount - (float) amount.getValue()) < 1.0e-4f) return;
            s.amount = (float) amount.getValue();
            proc.setSlot (slot, s);
        };
        srcBtn.onClick = [this] { showSourceMenu(); };
        dstBtn.onClick = [this] { showDestMenu(); };
        clearBtn.onClick = [this] { proc.undo.beginNewTransaction ("Clear slot"); proc.setSlot (slot, {}); };
        srcBtn.setTooltip ("Modulation source");
        dstBtn.setTooltip ("What it moves");
        amount.setTooltip ("How far, in either direction (double-click: 0)");
        clearBtn.setTooltip ("Clear this slot");
        refresh();
    }

    void refresh()
    {
        const auto s = proc.getSlot (slot);
        srcBtn.setButtonText (sourceNames()[(int) s.src]);
        dstBtn.setButtonText (s.dst >= 0 ? juce::String (def (s.dst).name) : juce::String ("-"));
        if (! amount.isMouseButtonDown()) amount.setValue (s.amount, juce::dontSendNotification);
        active = s.isActive();
        repaint();
    }

    void paint (juce::Graphics& g) override
    {
        g.setColour (active ? theme::aqua : theme::textFaint);
        g.setFont (theme::font (12.0f, true));
        g.drawText (juce::String (slot + 1), getLocalBounds().removeFromLeft (24), juce::Justification::centred, false);
    }

    void resized() override
    {
        auto r = getLocalBounds().reduced (0, 2);
        r.removeFromLeft (26);
        clearBtn.setBounds (r.removeFromRight (26));
        r.removeFromRight (6);
        srcBtn.setBounds (r.removeFromLeft (104));
        r.removeFromLeft (6);
        dstBtn.setBounds (r.removeFromLeft (r.getWidth() * 55 / 100));
        r.removeFromLeft (6);
        amount.setBounds (r);
    }

private:
    void showSourceMenu()
    {
        juce::PopupMenu m;
        for (int s = 0; s < kNumSources; ++s) m.addItem (s + 1, sourceNames()[s], true, (int) proc.getSlot (slot).src == s);
        m.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (srcBtn), [this] (int r)
        {
            if (r <= 0) return;
            auto s = proc.getSlot (slot);
            s.src = (Src) (r - 1);
            if (s.src != Src::none && s.amount == 0.0f) s.amount = 0.25f;
            proc.undo.beginNewTransaction ("Mod source");
            proc.setSlot (slot, s);
        });
    }

    void showDestMenu()
    {
        juce::PopupMenu m;
        auto addGroup = [&] (const juce::String& name, int first, int last)
        {
            juce::PopupMenu sub;
            for (int i = first; i <= last; ++i)
                if (def (i).modulatable) sub.addItem (i + 1, def (i).name, true, proc.getSlot (slot).dst == i);
            m.addSubMenu (name, sub);
        };
        addGroup ("Global", idx (P::inGain), idx (P::bassMono));
        for (int s = 0; s < kNumStages; ++s) addGroup (kStages[s].name, idx (kStages[s].on), idx (kStages[s].last));
        addGroup ("Modulators", idx (P::lfo1Shape), idx (P::driftSmooth));
        m.addSeparator();
        m.addItem (100000, "None");
        m.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (dstBtn), [this] (int r)
        {
            if (r <= 0) return;
            auto s = proc.getSlot (slot);
            s.dst = r == 100000 ? -1 : r - 1;
            if (s.dst >= 0 && s.src == Src::none) s.src = Src::lfo1;
            if (s.dst >= 0 && s.amount == 0.0f) s.amount = 0.25f;
            proc.undo.beginNewTransaction ("Mod destination");
            proc.setSlot (slot, s);
        });
    }

    VesselProcessor& proc;
    int slot;
    juce::TextButton srcBtn, dstBtn, clearBtn { "x" };
    juce::Slider amount;
    bool active = false;
};

class ModPage : public juce::Component
{
public:
    explicit ModPage (VesselProcessor& p)
        : proc (p), store (p),
          lfo1 ("LFO 1", theme::aqua), lfo2 ("LFO 2", theme::aqua), envP ("Envelope", theme::aqua), driftP ("Drift", theme::aqua),
          matrix ("Modulation Matrix", theme::aqua),
          m1 ([&p] { return p.uiLfo[0].load(); }, true, theme::aqua), m2 ([&p] { return p.uiLfo[1].load(); }, true, theme::aqua),
          m3 ([&p] { return p.uiEnv.load(); }, false, theme::aqua), m4 ([&p] { return p.uiDrift.load(); }, true, theme::aqua)
    {
        const auto c = theme::aqua;
        lfo1.addRow ({ store.choice (P::lfo1Shape, c), store.labelledLed (P::lfo1Sync, c), store.rate (P::lfo1Rate, P::lfo1Sync, P::lfo1Div, c) });
        lfo1.addRow ({ &m1 }, 0.25f);
        lfo2.addRow ({ store.choice (P::lfo2Shape, c), store.labelledLed (P::lfo2Sync, c), store.rate (P::lfo2Rate, P::lfo2Sync, P::lfo2Div, c) });
        lfo2.addRow ({ &m2 }, 0.25f);
        envP.addRow ({ store.knob (P::envAttack, c), store.knob (P::envRelease, c), store.knob (P::envGain, c) });
        envP.addRow ({ &m3 }, 0.25f);
        driftP.addRow ({ store.knob (P::driftRate, c), store.knob (P::driftSmooth, c), nullptr });
        driftP.addRow ({ &m4 }, 0.25f);

        for (int i = 0; i < kMatrixSlots; ++i)
        {
            rows.push_back (std::make_unique<MatrixRow> (p, i));
            matrix.addAndMakeVisible (*rows.back());
        }
        clearAll.onClick = [this]
        {
            proc.undo.beginNewTransaction ("Clear matrix");
            proc.clearMatrix();
        };
        clearAll.setTooltip ("Clear every modulation slot (macros included)");
        matrix.addAndMakeVisible (clearAll);

        for (auto* x : { &lfo1, &lfo2, &envP, &driftP, &matrix }) addAndMakeVisible (x);
    }

    void tick()
    {
        store.tick();
        for (auto* m : { &m1, &m2, &m3, &m4 }) m->tick();
        if (++refreshCounter % 6 == 0) for (auto& r : rows) r->refresh();
    }

    void resized() override
    {
        auto r = getLocalBounds();
        auto left = r.removeFromLeft (360);
        const int h = (left.getHeight() - 30) / 4;
        for (auto* x : { &lfo1, &lfo2, &envP, &driftP })
        {
            x->setBounds (left.removeFromTop (h));
            left.removeFromTop (10);
        }
        r.removeFromLeft (10);
        matrix.setBounds (r);
        auto mr = r.withZeroOrigin().reduced (14, 10);
        clearAll.setBounds (mr.removeFromTop (24).removeFromRight (90));
        mr.removeFromTop (8);
        const int rh = mr.getHeight() / kMatrixSlots;
        for (auto& row : rows) row->setBounds (mr.removeFromTop (rh));
    }

private:
    VesselProcessor& proc;
    ControlStore store;
    Panel lfo1, lfo2, envP, driftP, matrix;
    SourceMeter m1, m2, m3, m4;
    std::vector<std::unique_ptr<MatrixRow>> rows;
    juce::TextButton clearAll { "CLEAR ALL" };
    int refreshCounter = 0;
};

// ---------------------------------------------------------------------------
class GlobalPage : public juce::Component
{
public:
    explicit GlobalPage (VesselProcessor& p)
        : store (p), levels ("Levels & Output", theme::global), perform ("Performance", theme::shatter),
          timing ("Tempo & Random", theme::global), about ("About Vessel", theme::aqua)
    {
        const auto g = theme::global;
        levels.addRow ({ store.knob (P::inGain, g), store.knob (P::outGain, g), store.knob (P::mix, g), store.knob (P::width, g),
                         store.knob (P::bassMono, g), store.labelledLed (P::delta, g), store.labelledLed (P::limiter, g) });
        perform.addRow ({ store.knob (P::shatterTime, theme::shatter), store.knob (P::shatterDepth, theme::shatter),
                          store.labelledLed (P::sealSpace, theme::ice) });
        timing.addRow ({ store.knob (P::tempo, g), store.knob (P::seed, g) });
        for (auto* x : { &levels, &perform, &timing, &about }) addAndMakeVisible (x);
        about.addRow ({ &aboutText });
        aboutText.setText ("Vessel " JucePlugin_VersionString "  -  LowKei Audio\n\n"
                           "Pour any sound into a resonant body. The Body is up to 16 tuned modes shaped by its Material; "
                           "Fill lowers them like water in a glass. Seal holds the ring, Shatter breaks it apart.\n\n"
                           "Right-click any knob to modulate it or assign it to a macro. Free Tempo drives synced effects "
                           "in the standalone app or when the host sends no tempo.",
                           juce::dontSendNotification);
        aboutText.setJustificationType (juce::Justification::topLeft);
        aboutText.setFont (theme::font (14.0f));
        aboutText.setColour (juce::Label::textColourId, theme::textDim);
    }

    void tick() { store.tick(); }

    void resized() override
    {
        auto r = getLocalBounds();
        auto top = r.removeFromTop (r.getHeight() / 2 - 5);
        levels.setBounds (top);
        r.removeFromTop (10);
        auto bl = r.removeFromLeft (r.getWidth() * 55 / 100);
        perform.setBounds (bl.removeFromTop (bl.getHeight() / 2 - 5));
        bl.removeFromTop (10);
        timing.setBounds (bl);
        r.removeFromLeft (10);
        about.setBounds (r);
    }

private:
    ControlStore store;
    Panel levels, perform, timing, about;
    juce::Label aboutText;
};

} // namespace vessel
