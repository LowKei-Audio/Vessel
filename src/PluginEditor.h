#pragma once
// Editor window. Everything is laid out once at a fixed 1200x760 design size
// inside `content`, then scaled as a whole, so the UI stays proportional and
// crisp (all vector drawing) from 70% to 200%.

#include "PluginProcessor.h"
#include "ui/VesselLookAndFeel.h"
#include "ui/PresetBar.h"
#include "ui/Views.h"

namespace vessel
{

// Bottom bar: help for whatever is under the mouse, the preset's story when
// nothing is, plus input / output meters and limiter activity.
class HintBar : public juce::Component
{
public:
    explicit HintBar (VesselProcessor& p) : proc (p) {}

    void tick()
    {
        juce::String t;
        if (auto* c = juce::Desktop::getInstance().getMainMouseSource().getComponentUnderMouse())
            for (auto* x = c; x != nullptr && t.isEmpty(); x = x->getParentComponent())
                if (auto* tc = dynamic_cast<juce::TooltipClient*> (x)) t = tc->getTooltip();
        if (t.isEmpty()) t = proc.presets.getCurrentDescription();
        inL = juce::jmax (proc.uiInPeak.load(), inL * 0.85f);
        outL = juce::jmax (proc.uiOutPeak.load(), outL * 0.85f);
        gr = proc.uiGainReduction.load();
        if (t != text) text = t;
        repaint();
    }

    void paint (juce::Graphics& g) override
    {
        auto r = getLocalBounds().toFloat();
        g.setColour (theme::screen);
        g.fillRoundedRectangle (r, 6.0f);
        g.setColour (theme::edge);
        g.drawRoundedRectangle (r.reduced (0.5f), 6.0f, 1.0f);

        auto meters = r.removeFromRight (300.0f).reduced (12.0f, 9.0f);
        auto meter = [&] (juce::Rectangle<float> m, const char* label, float v, juce::Colour c)
        {
            theme::legend (g, label, m.removeFromLeft (30.0f), theme::textDim, 10.0f, juce::Justification::centredLeft);
            auto bar = m.withSizeKeepingCentre (m.getWidth(), 6.0f);
            g.setColour (theme::well);
            g.fillRoundedRectangle (bar, 3.0f);
            const float db = juce::Decibels::gainToDecibels (v, -60.0f);
            g.setColour (v > 0.95f ? theme::shatter : c);
            g.fillRoundedRectangle (bar.withWidth (bar.getWidth() * juce::jlimit (0.0f, 1.0f, (db + 60.0f) / 60.0f)), 3.0f);
        };
        auto top = meters.removeFromTop (meters.getHeight() * 0.5f);
        meter (top.removeFromLeft (top.getWidth() * 0.75f), "In", inL, theme::textDim);
        meter (meters.removeFromLeft (meters.getWidth() * 0.75f), "Out", outL, theme::aqua);
        theme::legend (g, gr < 0.99f ? "Limit" : "", top.reduced (4.0f, 0.0f), theme::shatter, 9.5f, juce::Justification::centredRight);

        g.setColour (theme::textDim);
        g.setFont (theme::font (14.0f));
        g.drawFittedText (text, r.reduced (16.0f, 4.0f).toNearestInt(), juce::Justification::centredLeft, 2);
    }

private:
    VesselProcessor& proc;
    juce::String text;
    float inL = 0.0f, outL = 0.0f, gr = 1.0f;
};

class VesselEditor : public juce::AudioProcessorEditor, private juce::Timer
{
public:
    static constexpr int kDesignW = 1200, kDesignH = 760;

    explicit VesselEditor (VesselProcessor&);
    ~VesselEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

    // The unscaled design-size canvas (also used for snapshot tests).
    class Content : public juce::Component
    {
    public:
        explicit Content (VesselProcessor&);
        void paint (juce::Graphics&) override;
        void resized() override;
        void tick();
        void setView (bool pro);
        bool isPro() const noexcept { return pro; }
        void showProPage (int page);   // for snapshots

    private:
        VesselProcessor& proc;
        PresetBar presetBar;
        juce::TextButton easyTab { "EASY" }, proTab { "PRO" }, undoBtn { "UNDO" }, redoBtn { "REDO" };
        EasyView easy;
        ProView proView;
        HintBar hint;
        bool pro = false;
    };

    Content& getContent() noexcept { return content; }

private:
    void timerCallback() override { content.tick(); }

    VesselLookAndFeel lnf;
    VesselProcessor& proc;
    Content content;
    juce::TooltipWindow tooltips { this, 700 };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (VesselEditor)
};

} // namespace vessel
