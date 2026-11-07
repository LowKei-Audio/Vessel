#pragma once
// Vessel-specific controls built on Controls.h:
//   SyncedRate     - a Rate knob that becomes a Division menu when Sync is on
//   PerformButton  - SEAL / SHATTER: HOLD (momentary) or LATCH (toggle), with
//                    the live ramp shown as a rising fill
//   PerformModeSwitch - the HOLD | LATCH selector under a PerformButton
//   MaterialPicker - eight one-click material tiles with small glyphs
//   MacroBank      - the four macros, with how many targets each one moves
//   StageSwitches  - one-click power for the six stages

#include "Controls.h"
#include "VesselShapes.h"

namespace vessel
{

// ---------------------------------------------------------------------------
class SyncedRate : public juce::Component
{
public:
    SyncedRate (VesselProcessor& p, P rate, P sync, P division, juce::Colour accent, juce::String legend = {})
        : knob (p, rate, accent, Knob::Size::medium, legend), div (p.apvts, division, accent, legend.isNotEmpty() ? legend : juce::String ("Rate"))
    {
        addAndMakeVisible (knob);
        addChildComponent (div);
        auto* sp = p.apvts.getParameter (def (sync).id);
        attachment = std::make_unique<juce::ParameterAttachment> (*sp, [this] (float v)
        {
            synced = v > 0.5f;
            knob.setVisible (! synced);
            div.setVisible (synced);
        });
        attachment->sendInitialUpdate();
    }
    void tick() { knob.tick(); }
    void resized() override
    {
        knob.setBounds (getLocalBounds());
        div.setBounds (getLocalBounds().withSizeKeepingCentre (getWidth(), juce::jmin (getHeight(), 50)));
    }

private:
    Knob knob;
    ChoiceBox div;
    bool synced = false;
    std::unique_ptr<juce::ParameterAttachment> attachment;
};

// ---------------------------------------------------------------------------
class PerformButton : public juce::Component, public juce::SettableTooltipClient
{
public:
    PerformButton (VesselProcessor& p, P param, juce::String title_, juce::String latchProperty, juce::Colour accentColour,
                   std::function<float()> rampSource, juce::String holdHint, juce::String tip)
        : proc (p), title (std::move (title_)), latchProp (std::move (latchProperty)), accent (accentColour),
          ramp (std::move (rampSource)), hint (std::move (holdHint))
    {
        auto* prm = p.apvts.getParameter (def (param).id);
        attachment = std::make_unique<juce::ParameterAttachment> (*prm, [this] (float v) { engaged = v > 0.5f; repaint(); });
        attachment->sendInitialUpdate();
        setMouseCursor (juce::MouseCursor::PointingHandCursor);
        setTooltip (tip);
    }

    bool isLatch() const { return (bool) proc.apvts.state.getProperty (latchProp, false); }
    void setLatch (bool l) { proc.apvts.state.setProperty (latchProp, l, nullptr); repaint(); }
    void setCompact (bool c) { compact = c; repaint(); }

    void tick()
    {
        const float r = ramp ? ramp() : (engaged ? 1.0f : 0.0f);
        if (std::abs (r - level) > 0.002f || engaged) { level = r; pulse += 0.12f; repaint(); }
    }

    void paint (juce::Graphics& g) override
    {
        auto r = getLocalBounds().toFloat().reduced (3.0f);
        const float glowAmt = level * (0.6f + 0.4f * std::sin (pulse) * (engaged ? 1.0f : 0.0f));
        theme::glow (g, r.getCentre(), r.getWidth() * 0.6f, accent, 0.08f + 0.4f * glowAmt);

        g.setColour (theme::screen);
        g.fillRoundedRectangle (r, 10.0f);
        {
            juce::Graphics::ScopedSaveState s (g);
            juce::Path clip;
            clip.addRoundedRectangle (r, 10.0f);
            g.reduceClipRegion (clip);
            auto fill = r.withTop (r.getBottom() - r.getHeight() * level);
            juce::ColourGradient grad (accent, fill.getX(), fill.getY(), accent.darker (1.2f), fill.getX(), r.getBottom(), false);
            g.setGradientFill (grad);
            g.fillRect (fill);
        }
        g.setColour (engaged ? accent : theme::edge);
        g.drawRoundedRectangle (r, 10.0f, engaged ? 2.0f : 1.2f);

        const bool lit = level > 0.55f;
        auto t = r.reduced (8.0f);
        if (compact)
        {
            theme::legend (g, title, t, lit ? juce::Colour (0xff031214) : theme::text, juce::jmin (17.0f, r.getHeight() * 0.38f),
                           juce::Justification::centred, true, 0.22f);
            return;
        }
        theme::legend (g, title, t.removeFromTop (t.getHeight() * 0.6f), lit ? juce::Colour (0xff031214) : theme::text,
                       juce::jmin (22.0f, r.getHeight() * 0.24f), juce::Justification::centredBottom, true, 0.24f);
        theme::legend (g, isLatch() ? "Latch  -  click" : hint, t, lit ? juce::Colour (0xff031214) : theme::textDim, 10.5f,
                       juce::Justification::centredTop);
    }

    void mouseDown (const juce::MouseEvent& e) override
    {
        if (e.mods.isPopupMenu()) { showMenu(); return; }
        if (isLatch()) attachment->setValueAsCompleteGesture (engaged ? 0.0f : 1.0f);
        else           attachment->setValueAsCompleteGesture (1.0f);
    }
    void mouseUp (const juce::MouseEvent& e) override
    {
        if (! e.mods.isPopupMenu() && ! isLatch()) attachment->setValueAsCompleteGesture (0.0f);
    }

    std::function<void()> onModeChange;

private:
    void showMenu()
    {
        juce::PopupMenu m;
        m.addItem (1, "Hold (momentary)", true, ! isLatch());
        m.addItem (2, "Latch (toggle)", true, isLatch());
        m.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (this), [this] (int r)
        {
            if (r == 1) setLatch (false);
            if (r == 2) setLatch (true);
            if (r > 0 && onModeChange) onModeChange();
        });
    }

    VesselProcessor& proc;
    juce::String title, latchProp;
    juce::Colour accent;
    std::function<float()> ramp;
    juce::String hint;
    std::unique_ptr<juce::ParameterAttachment> attachment;
    bool engaged = false, compact = false;
    float level = 0.0f, pulse = 0.0f;
};

class PerformModeSwitch : public juce::Component
{
public:
    explicit PerformModeSwitch (PerformButton& b) : button (b)
    {
        for (auto* t : { &hold, &latch }) { addAndMakeVisible (t); t->setMouseCursor (juce::MouseCursor::PointingHandCursor); }
        hold.onClick = [this] { button.setLatch (false); refresh(); };
        latch.onClick = [this] { button.setLatch (true); refresh(); };
        hold.setTooltip ("Engaged only while the button is held");
        latch.setTooltip ("Toggles on and off with each click");
        button.onModeChange = [this] { refresh(); };
        refresh();
    }
    void resized() override
    {
        auto r = getLocalBounds();
        hold.setBounds (r.removeFromLeft (r.getWidth() / 2).reduced (2, 0));
        latch.setBounds (r.reduced (2, 0));
    }
    void refresh()
    {
        hold.setToggleState (! button.isLatch(), juce::dontSendNotification);
        latch.setToggleState (button.isLatch(), juce::dontSendNotification);
    }

private:
    PerformButton& button;
    juce::TextButton hold { "HOLD" }, latch { "LATCH" };
};

// ---------------------------------------------------------------------------
class MaterialPicker : public juce::Component, public juce::SettableTooltipClient
{
public:
    explicit MaterialPicker (VesselProcessor& p)
    {
        auto* prm = p.apvts.getParameter (def (P::bdMaterial).id);
        attachment = std::make_unique<juce::ParameterAttachment> (*prm, [this] (float v) { selected = juce::roundToInt (v); repaint(); });
        attachment->sendInitialUpdate();
        setMouseCursor (juce::MouseCursor::PointingHandCursor);
        setTooltip (def (P::bdMaterial).help);
    }

    void paint (juce::Graphics& g) override
    {
        const auto& names = Choices::materials;
        for (int i = 0; i < names.size(); ++i)
        {
            auto r = tile (i).reduced (3.0f);
            const bool on = i == selected, hov = i == hovered;
            g.setColour (on ? theme::aqua.withAlpha (0.16f) : theme::well);
            g.fillRoundedRectangle (r, 6.0f);
            g.setColour (on ? theme::aqua : (hov ? theme::edge.brighter (0.4f) : theme::edge));
            g.drawRoundedRectangle (r, 6.0f, on ? 1.6f : 1.0f);

            auto icon = r.reduced (6.0f).withTrimmedBottom (16.0f);
            const float s = juce::jmin (icon.getWidth(), icon.getHeight());
            auto box = juce::Rectangle<float> (s, s).withCentre (icon.getCentre());
            const auto path = shapes::outline (i, box);
            g.setColour ((on ? theme::aqua : theme::textDim).withAlpha (on ? 0.25f : 0.08f));
            g.fillPath (path);
            g.setColour (on ? theme::aqua : theme::textDim);
            g.strokePath (path, juce::PathStrokeType (1.3f));
            theme::legend (g, names[i], r.removeFromBottom (17.0f), on ? theme::text : theme::textDim, 10.0f,
                           juce::Justification::centred, true, 0.06f);
        }
    }

    void mouseMove (const juce::MouseEvent& e) override { const int h = indexAt (e.position); if (h != hovered) { hovered = h; repaint(); } }
    void mouseExit (const juce::MouseEvent&) override  { hovered = -1; repaint(); }
    void mouseDown (const juce::MouseEvent& e) override
    {
        const int i = indexAt (e.position);
        if (i >= 0 && i != selected) attachment->setValueAsCompleteGesture ((float) i);
    }

private:
    static constexpr int kCols = 4;
    juce::Rectangle<float> tile (int i) const
    {
        const float w = (float) getWidth() / kCols, h = (float) getHeight() / 2.0f;
        return { (float) (i % kCols) * w, (float) (i / kCols) * h, w, h };
    }
    int indexAt (juce::Point<float> p) const
    {
        for (int i = 0; i < Choices::materials.size(); ++i) if (tile (i).contains (p)) return i;
        return -1;
    }

    std::unique_ptr<juce::ParameterAttachment> attachment;
    int selected = 0, hovered = -1;
};

// ---------------------------------------------------------------------------
class MacroBank : public juce::Component
{
public:
    MacroBank (VesselProcessor& p, Knob::Size size) : proc (p)
    {
        for (int k = 0; k < kNumMacros; ++k)
        {
            knobs.push_back (std::make_unique<Knob> (p, (P) (idx (P::macro1) + k), theme::aqua, size, kMacroNames[k]));
            addAndMakeVisible (*knobs.back());
        }
    }
    void tick()
    {
        for (auto& k : knobs) k->tick();
        for (int k = 0; k < kNumMacros; ++k)
        {
            int n = 0;
            for (int s = 0; s < kMatrixSlots; ++s)
            {
                const auto sl = proc.getSlot (s);
                if (sl.isActive() && (int) sl.src == (int) Src::macro1 + k) ++n;
            }
            if (n != counts[(size_t) k]) { counts[(size_t) k] = n; repaint(); }
        }
    }
    void paint (juce::Graphics& g) override
    {
        for (int k = 0; k < kNumMacros; ++k)
        {
            auto r = knobs[(size_t) k]->getBounds().toFloat();
            g.setColour (theme::textFaint);
            g.setFont (theme::font (10.5f));
            const int n = counts[(size_t) k];
            g.drawText (n == 0 ? juce::String ("not assigned") : juce::String (n) + (n == 1 ? " target" : " targets"),
                        r.withY (r.getBottom()).withHeight (14.0f), juce::Justification::centred, false);
        }
    }
    void resized() override
    {
        auto r = getLocalBounds();
        const int cw = r.getWidth() / 2, rh = r.getHeight() / 2;
        for (int k = 0; k < kNumMacros; ++k)
            knobs[(size_t) k]->setBounds (juce::Rectangle<int> (r.getX() + (k % 2) * cw, r.getY() + (k / 2) * rh, cw, rh - 14).reduced (6, 2));
    }

private:
    VesselProcessor& proc;
    std::vector<std::unique_ptr<Knob>> knobs;
    std::array<int, kNumMacros> counts { -1, -1, -1, -1 };
};

// ---------------------------------------------------------------------------
class StageSwitches : public juce::Component
{
public:
    explicit StageSwitches (VesselProcessor& p, int columns = 3) : cols (columns)
    {
        for (int s = 0; s < kNumStages; ++s)
        {
            leds.push_back (std::make_unique<LedButton> (p.apvts, kStages[s].on, theme::stage (s), false, kStages[s].name));
            leds.back()->setTooltip (juce::String (kStages[s].name) + ": " + kStages[s].blurb);
            addAndMakeVisible (*leds.back());
        }
    }
    void resized() override
    {
        auto r = getLocalBounds();
        const int rows = (kNumStages + cols - 1) / cols;
        const int rh = r.getHeight() / rows, cw = r.getWidth() / cols;
        for (int s = 0; s < kNumStages; ++s)
            leds[(size_t) s]->setBounds (juce::Rectangle<int> (r.getX() + (s % cols) * cw, r.getY() + (s / cols) * rh, cw, rh).reduced (4, 3));
    }

private:
    int cols;
    std::vector<std::unique_ptr<LedButton>> leds;
};

} // namespace vessel
