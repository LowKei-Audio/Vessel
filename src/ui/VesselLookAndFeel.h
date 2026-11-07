#pragma once
// Drawing for knobs, buttons, menus, dialogs and panels.

#include "Theme.h"

namespace vessel
{

class VesselLookAndFeel : public juce::LookAndFeel_V4
{
public:
    VesselLookAndFeel();

    juce::Typeface::Ptr getTypefaceForFont (const juce::Font& f) override { return theme::typeface (f.isBold()); }

    void drawRotarySlider (juce::Graphics&, int x, int y, int w, int h, float pos,
                           float startAngle, float endAngle, juce::Slider&) override;
    void drawLinearSlider (juce::Graphics&, int x, int y, int w, int h, float pos, float minPos, float maxPos,
                           juce::Slider::SliderStyle, juce::Slider&) override;

    void drawButtonBackground (juce::Graphics&, juce::Button&, const juce::Colour&, bool highlighted, bool down) override;
    juce::Font getTextButtonFont (juce::TextButton&, int) override { return theme::font (12.5f, true); }

    void drawPopupMenuBackground (juce::Graphics&, int w, int h) override;
    juce::Font getPopupMenuFont() override { return theme::font (14.5f); }

    juce::Font getAlertWindowTitleFont() override   { return theme::font (17.0f, true); }
    juce::Font getAlertWindowMessageFont() override { return theme::font (14.0f); }

    void drawTooltip (juce::Graphics&, const juce::String& text, int w, int h) override;
    juce::Rectangle<int> getTooltipBounds (const juce::String& text, juce::Point<int> pos, juce::Rectangle<int> parent) override;
};

// Recessed panel with a fine grain and a soft accent line along the top.
void drawPanel (juce::Graphics&, juce::Rectangle<float> r, juce::Colour accent, float corner = 10.0f);

// Fine noise texture shared by the chassis and panels.
const juce::Image& grainTexture();

} // namespace vessel
