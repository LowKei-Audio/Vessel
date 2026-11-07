#pragma once
// Reusable controls, each bound directly to one parameter.
//   Knob       - rotary + legend + value; shows its modulation range and the
//                live (modulated / Shattered) position. Double-click resets.
//                Right-click: modulate with a source, assign to a macro,
//                remove modulation, reset.
//   LedButton  - one-click on/off with a glowing LED (bool params)
//   SegmentSwitch - one-click choice between a few labelled options
//   ChoiceBox  - compact drop-down for long choice lists (types, divisions)
//
// Every control publishes its parameter's help text as a tooltip, which the
// hint bar at the bottom of the window also shows.

#include "VesselLookAndFeel.h"
#include "../PluginProcessor.h"

namespace vessel
{

// ---------------------------------------------------------------------------
class Knob : public juce::Component, public juce::SettableTooltipClient
{
public:
    enum class Size { small, medium, large };

    Knob (VesselProcessor& p, P param, juce::Colour accent, Size sz = Size::medium, juce::String legendOverride = {})
        : proc (p), index (idx (param)), size (sz), name (legendOverride.isNotEmpty() ? legendOverride : juce::String (def (param).label))
    {
        slider.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
        slider.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
        slider.setColour (juce::Slider::rotarySliderFillColourId, accent);
        slider.setRotaryParameters (juce::degreesToRadians (225.0f), juce::degreesToRadians (495.0f), true);
        slider.setMouseDragSensitivity (sz == Size::large ? 300 : 220);
        slider.setPopupMenuEnabled (false);
        addAndMakeVisible (slider);
        attachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (p.apvts, def (param).id, slider);

        if (auto* rp = dynamic_cast<juce::RangedAudioParameter*> (p.apvts.getParameter (def (param).id)))
        {
            slider.setDoubleClickReturnValue (true, rp->convertFrom0to1 (rp->getDefaultValue()));
            const auto& range = rp->getNormalisableRange();
            if (range.start < 0.0f && range.end > 0.0f)
                slider.getProperties().set ("zeroPos", range.convertTo0to1 (0.0f));
        }
        slider.onValueChange = [this] { repaint(); };
        slider.addMouseListener (this, false);
        setTooltip (def (param).help);
    }

    void setTooltip (const juce::String& t) override { SettableTooltipClient::setTooltip (t); slider.setTooltip (t); }
    void setLegend (const juce::String& s) { if (s != name) { name = s; repaint(); } }
    juce::Slider& getSlider() noexcept { return slider; }
    int paramIndex() const noexcept { return index; }

    // Called ~30x per second by the editor: refresh live position + mod depth.
    void tick()
    {
        const float live = proc.uiNorm[(size_t) index].load (std::memory_order_relaxed);
        float depth = 0.0f;
        for (int s : proc.routesTo (index)) depth += std::abs (proc.getSlot (s).amount);
        if (std::abs (live - lastLive) > 0.002f || std::abs (depth - lastDepth) > 0.001f)
        {
            lastLive = live;
            lastDepth = depth;
            slider.getProperties().set ("livePos", live);
            slider.getProperties().set ("modDepth", depth);
            slider.repaint();
        }
    }

    void paint (juce::Graphics& g) override
    {
        const auto valueText = formatValue (def (index), (float) slider.getValue());
        const float ls = size == Size::large ? 13.0f : (size == Size::small ? 10.0f : 11.0f);
        const float vs = size == Size::large ? 14.0f : (size == Size::small ? 11.0f : 12.0f);
        theme::legend (g, name, labelArea, isEnabled() ? theme::textDim : theme::textFaint, ls);
        g.setColour (isEnabled() ? theme::text : theme::textFaint);
        g.setFont (theme::font (vs));
        g.drawText (valueText, valueArea, juce::Justification::centred, false);
    }

    void resized() override
    {
        auto r = getLocalBounds().toFloat();
        const float lh = size == Size::large ? 18.0f : (size == Size::small ? 14.0f : 16.0f);
        labelArea = r.removeFromTop (lh);
        valueArea = r.removeFromBottom (lh + 1.0f);
        const float d = juce::jmin (r.getWidth(), r.getHeight());
        slider.setBounds (r.withSizeKeepingCentre (d, d).toNearestInt());
    }

    void mouseDown (const juce::MouseEvent& e) override
    {
        if (e.mods.isPopupMenu()) showMenu();
    }

private:
    void showMenu()
    {
        const auto& d = def (index);
        juce::PopupMenu m;
        m.addSectionHeader (d.name);
        constexpr int kReset = 1, kRemove = 2, kModBase = 100, kMacroBase = 200;
        if (d.modulatable)
        {
            juce::PopupMenu mod;
            for (int s = (int) Src::lfo1; s <= (int) Src::drift; ++s)
                mod.addItem (kModBase + s, sourceNames()[s]);
            m.addSubMenu ("Modulate with", mod);
            juce::PopupMenu mac;
            for (int k = 0; k < kNumMacros; ++k)
                mac.addItem (kMacroBase + k, "Macro " + juce::String (k + 1) + "  " + kMacroNames[k]);
            m.addSubMenu ("Assign to macro", mac);
            m.addItem (kRemove, "Remove modulation", ! proc.routesTo (index).isEmpty());
        }
        m.addItem (kReset, "Reset to default");

        m.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (this), [this] (int r)
        {
            const auto& dd = def (index);
            if (r == kReset)
            {
                if (auto* p = proc.apvts.getParameter (dd.id)) { p->beginChangeGesture(); p->setValueNotifyingHost (p->getDefaultValue()); p->endChangeGesture(); }
            }
            else if (r == kRemove)
            {
                proc.undo.beginNewTransaction ("Remove modulation");
                proc.removeRoutesTo (index);
            }
            else if (r >= kMacroBase)
            {
                proc.undo.beginNewTransaction ("Assign macro");
                if (proc.addRoute ((Src) ((int) Src::macro1 + r - kMacroBase), index, 0.4f) < 0) warnFull();
            }
            else if (r >= kModBase)
            {
                proc.undo.beginNewTransaction ("Add modulation");
                if (proc.addRoute ((Src) (r - kModBase), index, 0.25f) < 0) warnFull();
            }
        });
    }

    static void warnFull()
    {
        juce::AlertWindow::showMessageBoxAsync (juce::MessageBoxIconType::InfoIcon, "Matrix full",
                                                "All 16 modulation slots are in use. Clear one on the MOD page first.");
    }

    VesselProcessor& proc;
    int index;
    Size size;
    juce::String name;
    juce::Slider slider;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> attachment;
    juce::Rectangle<float> labelArea, valueArea;
    float lastLive = -1.0f, lastDepth = -1.0f;
};

// ---------------------------------------------------------------------------
class LedButton : public juce::Button
{
public:
    LedButton (juce::AudioProcessorValueTreeState& state, P param, juce::Colour accentColour, bool large = false,
               juce::String legend = {})
        : juce::Button (legend.isNotEmpty() ? legend : juce::String (def (param).label)), accent (accentColour), big (large)
    {
        setClickingTogglesState (true);
        setMouseCursor (juce::MouseCursor::PointingHandCursor);
        attachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (state, def (param).id, *this);
        setTooltip (def (param).help);
    }

    void paintButton (juce::Graphics& g, bool highlighted, bool) override
    {
        auto r = getLocalBounds().toFloat();
        const float d = big ? 13.0f : 10.0f;
        auto led = r.removeFromLeft (d + 4.0f).withSizeKeepingCentre (d, d);
        const bool on = getToggleState();
        if (on)
        {
            theme::glow (g, led.getCentre(), d * 1.6f, accent, 0.55f);
            juce::ColourGradient lens (accent.brighter (0.9f), led.getCentreX() - d * 0.2f, led.getCentreY() - d * 0.2f,
                                       accent.darker (0.2f), led.getRight(), led.getBottom(), true);
            g.setGradientFill (lens);
        }
        else
            g.setColour (accent.darker (2.8f).withSaturation (0.4f));
        g.fillEllipse (led);
        g.setColour (juce::Colours::black.withAlpha (0.7f));
        g.drawEllipse (led, 1.0f);
        r.removeFromLeft (big ? 8.0f : 6.0f);
        theme::legend (g, getButtonText(), r, (on || highlighted) ? theme::text : theme::textDim,
                       big ? 15.0f : 11.0f, juce::Justification::centredLeft, true, big ? 0.2f : 0.1f);
    }

private:
    juce::Colour accent;
    bool big;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> attachment;
};

// ---------------------------------------------------------------------------
// One click per choice. Works for choice, bool and int params (pass the int
// param's minimum as firstValue so segment 0 maps to it).
class SegmentSwitch : public juce::Component, public juce::SettableTooltipClient
{
public:
    SegmentSwitch (juce::AudioProcessorValueTreeState& state, P param, juce::StringArray labels_, juce::Colour accentColour,
                   int firstValue = 0)
        : labels (std::move (labels_)), accent (accentColour), offset (firstValue)
    {
        auto* p = state.getParameter (def (param).id);
        jassert (p != nullptr);
        attachment = std::make_unique<juce::ParameterAttachment> (*p, [this] (float v)
        {
            selected = juce::roundToInt (v) - offset;
            repaint();
            if (onChange) onChange (selected);
        });
        attachment->sendInitialUpdate();
        setMouseCursor (juce::MouseCursor::PointingHandCursor);
        setTooltip (def (param).help);
    }

    std::function<void (int)> onChange;
    int getSelected() const noexcept { return selected; }
    void setLegendSize (float s) { legendSize = s; repaint(); }

    void paint (juce::Graphics& g) override
    {
        auto r = getLocalBounds().toFloat().reduced (0.5f);
        g.setColour (theme::well);
        g.fillRoundedRectangle (r, 5.0f);
        g.setColour (theme::edge);
        g.drawRoundedRectangle (r, 5.0f, 1.0f);
        const float w = r.getWidth() / (float) labels.size();
        for (int i = 0; i < labels.size(); ++i)
        {
            auto seg = juce::Rectangle<float> (r.getX() + w * (float) i, r.getY(), w, r.getHeight()).reduced (2.0f);
            const bool on = i == selected;
            if (on)
            {
                g.setColour (accent.withAlpha (0.18f));
                g.fillRoundedRectangle (seg.expanded (1.5f), 4.0f);
                juce::ColourGradient grad (accent, seg.getX(), seg.getY(), accent.darker (0.55f), seg.getX(), seg.getBottom(), false);
                g.setGradientFill (grad);
                g.fillRoundedRectangle (seg, 3.5f);
            }
            else if (i == hovered)
            {
                g.setColour (juce::Colours::white.withAlpha (0.05f));
                g.fillRoundedRectangle (seg, 3.5f);
            }
            theme::legend (g, labels[i], seg, on ? juce::Colours::black : theme::textDim, legendSize,
                           juce::Justification::centred, true, 0.06f);
        }
    }

    void mouseMove (const juce::MouseEvent& e) override { setHovered (indexAt (e.position.x)); }
    void mouseExit (const juce::MouseEvent&) override   { setHovered (-1); }
    void mouseDown (const juce::MouseEvent& e) override
    {
        const int i = indexAt (e.position.x);
        if (i != selected) attachment->setValueAsCompleteGesture ((float) (i + offset));
    }

private:
    int indexAt (float x) const { return juce::jlimit (0, labels.size() - 1, (int) (x / ((float) getWidth() / (float) labels.size()))); }
    void setHovered (int i) { if (i != hovered) { hovered = i; repaint(); } }

    juce::StringArray labels;
    juce::Colour accent;
    int offset = 0, selected = 0, hovered = -1;
    float legendSize = 10.5f;
    std::unique_ptr<juce::ParameterAttachment> attachment;
};

// ---------------------------------------------------------------------------
// Compact drop-down with a legend above it, for long choice lists.
class ChoiceBox : public juce::Component, public juce::SettableTooltipClient
{
public:
    ChoiceBox (juce::AudioProcessorValueTreeState& state, P param, juce::Colour accentColour, juce::String legendText = {})
        : index (idx (param)), accent (accentColour), legend (legendText.isNotEmpty() ? legendText : juce::String (def (param).label))
    {
        auto* p = state.getParameter (def (param).id);
        attachment = std::make_unique<juce::ParameterAttachment> (*p, [this] (float v)
        {
            selected = juce::roundToInt (v);
            repaint();
            if (onChange) onChange (selected);
        });
        attachment->sendInitialUpdate();
        setMouseCursor (juce::MouseCursor::PointingHandCursor);
        setTooltip (def (param).help);
    }

    std::function<void (int)> onChange;
    int getSelected() const noexcept { return selected; }
    int paramIndex() const noexcept { return index; }
    void setShowLegend (bool s) { showLegend = s; repaint(); }

    void paint (juce::Graphics& g) override
    {
        auto r = getLocalBounds().toFloat();
        if (showLegend)
            theme::legend (g, legend, r.removeFromTop (16.0f), theme::textDim, 10.5f);
        auto box = r.reduced (0.5f).withSizeKeepingCentre (r.getWidth() - 1.0f, juce::jmin (r.getHeight(), 30.0f));
        g.setColour (theme::screen);
        g.fillRoundedRectangle (box, 5.0f);
        g.setColour (hover ? accent.withAlpha (0.8f) : theme::edge);
        g.drawRoundedRectangle (box, 5.0f, 1.0f);
        auto inner = box.reduced (9.0f, 0.0f);
        auto arrow = inner.removeFromRight (9.0f);
        juce::Path tri;
        tri.addTriangle (arrow.getX(), arrow.getCentreY() - 2.0f, arrow.getRight(), arrow.getCentreY() - 2.0f,
                         arrow.getCentreX(), arrow.getCentreY() + 3.0f);
        g.setColour (accent);
        g.fillPath (tri);
        g.setColour (theme::text);
        g.setFont (theme::font (13.5f, true));
        g.drawText ((*def (index).choices)[selected], inner, juce::Justification::centredLeft, true);
    }

    void mouseEnter (const juce::MouseEvent&) override { hover = true; repaint(); }
    void mouseExit (const juce::MouseEvent&) override  { hover = false; repaint(); }
    void enablementChanged() override { setAlpha (isEnabled() ? 1.0f : 0.4f); }
    void mouseDown (const juce::MouseEvent&) override
    {
        juce::PopupMenu m;
        const auto& c = *def (index).choices;
        for (int i = 0; i < c.size(); ++i) m.addItem (i + 1, c[i], true, i == selected);
        m.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (this).withMinimumWidth (getWidth()),
                         [this] (int r) { if (r > 0) attachment->setValueAsCompleteGesture ((float) (r - 1)); });
    }

private:
    int index;
    juce::Colour accent;
    juce::String legend;
    int selected = 0;
    bool hover = false, showLegend = true;
    std::unique_ptr<juce::ParameterAttachment> attachment;
};

// A legend above an arbitrary control (for switches in a row of knobs).
class Labelled : public juce::Component
{
public:
    Labelled (juce::String text, juce::Component& c) : legend (std::move (text)), child (c) { addAndMakeVisible (child); }
    void enablementChanged() override { setAlpha (isEnabled() ? 1.0f : 0.4f); }
    void paint (juce::Graphics& g) override
    {
        theme::legend (g, legend, getLocalBounds().toFloat().removeFromTop (16.0f), theme::textDim, 10.5f);
    }
    void resized() override
    {
        auto r = getLocalBounds();
        r.removeFromTop (16);
        child.setBounds (r.withSizeKeepingCentre (r.getWidth(), juce::jmin (r.getHeight(), 28)));
    }

private:
    juce::String legend;
    juce::Component& child;
};

} // namespace vessel
