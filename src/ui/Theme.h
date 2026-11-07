#pragma once
// Vessel colour + type tokens. Always dark: deep-water chassis with an aqua
// glow, the LowKei Audio family's hardware look. Every stage has its own
// accent so a control's home reads at a glance. Text is cool off-white on
// near-black, well above 7:1 contrast.

#include <juce_gui_basics/juce_gui_basics.h>
#include <BinaryData.h>

namespace vessel::theme
{
    inline const juce::Colour background  { 0xff060a0d };
    inline const juce::Colour chassisTop  { 0xff111b21 };
    inline const juce::Colour chassisBot  { 0xff070b0e };
    inline const juce::Colour panel       { 0xff111a20 };
    inline const juce::Colour panelHi     { 0xff18252d };
    inline const juce::Colour edge        { 0xff2a3b45 };
    inline const juce::Colour well        { 0xff05080a };
    inline const juce::Colour screen      { 0xff040709 };

    inline const juce::Colour text        { 0xffeaf4f6 };   // primary
    inline const juce::Colour textDim     { 0xffb3c7cd };   // secondary labels (~9:1 on panel)
    inline const juce::Colour textFaint   { 0xff7f939b };   // hints only

    inline const juce::Colour aqua        { 0xff3fe0d0 };   // brand accent / macros
    inline const juce::Colour aquaDeep    { 0xff0b5a5c };
    inline const juce::Colour ice         { 0xffbfe9ff };   // Seal
    inline const juce::Colour shatter     { 0xffff6b7f };   // Shatter

    // Stage accents (same order as the chain: Pour .. Space).
    inline juce::Colour stage (int s)
    {
        static const juce::Colour c[6] = {
            juce::Colour (0xffffb454),   // Pour     - amber
            juce::Colour (0xff3fe0d0),   // Body     - aqua
            juce::Colour (0xffff6b7f),   // Fracture - coral
            juce::Colour (0xff4aa8ff),   // Liquid   - blue
            juce::Colour (0xffb68cff),   // Echo     - violet
            juce::Colour (0xff8fa2ff),   // Space    - periwinkle
        };
        return c[juce::jlimit (0, 5, s)];
    }
    inline const juce::Colour global { 0xffdfeaec };

    // Embedded Barlow Semi Condensed (SIL OFL): identical metrics everywhere.
    inline juce::Typeface::Ptr typeface (bool bold)
    {
        static const juce::Typeface::Ptr medium = juce::Typeface::createSystemTypefaceFor (
            VesselAssets::BarlowSemiCondensedMedium_ttf, VesselAssets::BarlowSemiCondensedMedium_ttfSize);
        static const juce::Typeface::Ptr semiBold = juce::Typeface::createSystemTypefaceFor (
            VesselAssets::BarlowSemiCondensedSemiBold_ttf, VesselAssets::BarlowSemiCondensedSemiBold_ttfSize);
        return bold ? semiBold : medium;
    }

    inline juce::Font font (float size, bool bold = false)
    {
        return juce::Font (juce::FontOptions (typeface (bold)).withHeight (size * 1.12f));
    }

    // Letter-spaced caps for hardware-style legends.
    inline void legend (juce::Graphics& g, const juce::String& s, juce::Rectangle<float> r, juce::Colour c, float size,
                        juce::Justification j = juce::Justification::centred, bool bold = true, float tracking = 0.12f)
    {
        g.setColour (c);
        g.setFont (font (size, bold).withExtraKerningFactor (tracking));
        g.drawText (s.toUpperCase(), r, j, false);
    }

    // Soft radial glow (used behind lit elements).
    inline void glow (juce::Graphics& g, juce::Point<float> c, float radius, juce::Colour col, float alpha)
    {
        juce::ColourGradient grad (col.withAlpha (alpha), c.x, c.y, col.withAlpha (0.0f), c.x + radius, c.y, true);
        g.setGradientFill (grad);
        g.fillEllipse (c.x - radius, c.y - radius, radius * 2.0f, radius * 2.0f);
    }
}
