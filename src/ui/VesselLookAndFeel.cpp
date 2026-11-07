#include "VesselLookAndFeel.h"

namespace vessel
{
using namespace juce;

VesselLookAndFeel::VesselLookAndFeel()
{
    setColour (ResizableWindow::backgroundColourId, theme::background);
    setColour (PopupMenu::backgroundColourId, theme::panel);
    setColour (PopupMenu::textColourId, theme::text);
    setColour (PopupMenu::headerTextColourId, theme::aqua);
    setColour (PopupMenu::highlightedBackgroundColourId, theme::aquaDeep);
    setColour (PopupMenu::highlightedTextColourId, theme::text);
    setColour (TextButton::textColourOffId, theme::text);
    setColour (TextButton::textColourOnId, Colour (0xff031214));   // dark text on a lit (aqua) button
    setColour (TextButton::buttonOnColourId, theme::aqua);
    setColour (AlertWindow::backgroundColourId, theme::panel);
    setColour (AlertWindow::textColourId, theme::text);
    setColour (AlertWindow::outlineColourId, theme::edge);
    setColour (TextEditor::backgroundColourId, theme::screen);
    setColour (TextEditor::textColourId, theme::text);
    setColour (TextEditor::outlineColourId, theme::edge);
    setColour (TextEditor::focusedOutlineColourId, theme::aqua);
    setColour (TextEditor::highlightColourId, theme::aqua.withAlpha (0.35f));
    setColour (CaretComponent::caretColourId, theme::text);
    setColour (Label::textColourId, theme::text);
    setColour (TooltipWindow::backgroundColourId, theme::panelHi);
    setColour (TooltipWindow::textColourId, theme::text);
    setColour (TooltipWindow::outlineColourId, theme::edge);
    setColour (ScrollBar::thumbColourId, theme::edge);
}

const Image& grainTexture()
{
    static const Image img = []
    {
        Image im (Image::ARGB, 192, 192, true);
        Random rng (13);
        for (int y = 0; y < im.getHeight(); ++y)
            for (int x = 0; x < im.getWidth(); ++x)
            {
                const float v = rng.nextFloat();
                if (v > 0.55f) im.setPixelAt (x, y, Colours::white.withAlpha (0.012f + 0.02f * rng.nextFloat()));
                else if (v < 0.25f) im.setPixelAt (x, y, Colours::black.withAlpha (0.05f * rng.nextFloat()));
            }
        return im;
    }();
    return img;
}

void drawPanel (Graphics& g, Rectangle<float> r, Colour accent, float corner)
{
    g.setColour (Colours::black.withAlpha (0.6f));
    g.fillRoundedRectangle (r.translated (0.0f, 2.0f), corner);

    ColourGradient face (theme::panelHi, r.getX(), r.getY(), theme::panel.darker (0.35f), r.getX(), r.getBottom(), false);
    g.setGradientFill (face);
    g.fillRoundedRectangle (r, corner);
    {
        Graphics::ScopedSaveState s (g);
        Path clip;
        clip.addRoundedRectangle (r, corner);
        g.reduceClipRegion (clip);
        g.setTiledImageFill (grainTexture(), 0, 0, 1.0f);
        g.fillRect (r);
        // Faint accent bloom from the top edge.
        ColourGradient bloom (accent.withAlpha (0.10f), r.getCentreX(), r.getY(), accent.withAlpha (0.0f),
                              r.getCentreX(), r.getY() + jmin (90.0f, r.getHeight() * 0.5f), false);
        g.setGradientFill (bloom);
        g.fillRect (r);
    }
    g.setColour (theme::edge);
    g.drawRoundedRectangle (r.reduced (0.5f), corner, 1.0f);

    // Accent hairline across the top, fading at the ends.
    auto line = Rectangle<float> (r.getX() + corner * 2.0f, r.getY() + 0.5f, r.getWidth() - corner * 4.0f, 1.2f);
    ColourGradient lg (accent.withAlpha (0.0f), line.getX(), 0, accent.withAlpha (0.0f), line.getRight(), 0, false);
    lg.addColour (0.5, accent.withAlpha (0.7f));
    g.setGradientFill (lg);
    g.fillRect (line);
}

// ---------------------------------------------------------------------------
void VesselLookAndFeel::drawRotarySlider (Graphics& g, int x, int y, int w, int h, float pos,
                                        float startAngle, float endAngle, Slider& s)
{
    const auto bounds = Rectangle<float> ((float) x, (float) y, (float) w, (float) h).reduced (2.0f);
    const float radius = jmin (bounds.getWidth(), bounds.getHeight()) * 0.5f;
    const auto c = bounds.getCentre();
    const auto accent = s.findColour (Slider::rotarySliderFillColourId);
    const bool enabled = s.isEnabled();
    const float angle = startAngle + pos * (endAngle - startAngle);
    const float arcR = radius * 0.84f;
    const float arcW = jmax (2.2f, radius * 0.1f);

    g.setColour (theme::well);
    g.fillEllipse (c.x - radius, c.y - radius, radius * 2, radius * 2);

    Path track;
    track.addCentredArc (c.x, c.y, arcR, arcR, 0.0f, startAngle, endAngle, true);
    g.setColour (Colour (0xff17222a));
    g.strokePath (track, PathStrokeType (arcW, PathStrokeType::curved, PathStrokeType::rounded));

    const auto& props = s.getProperties();
    const float zeroAngle = props.contains ("zeroPos") ? startAngle + (float) props["zeroPos"] * (endAngle - startAngle) : startAngle;

    // Modulation range: a thin outer ring from the knob position to +/- depth.
    if (props.contains ("modDepth") && enabled)
    {
        const float depth = (float) props["modDepth"];
        if (depth > 0.001f)
        {
            const float lo = jlimit (0.0f, 1.0f, pos - depth), hi = jlimit (0.0f, 1.0f, pos + depth);
            Path mod;
            const float mr = radius * 0.98f;
            mod.addCentredArc (c.x, c.y, mr, mr, 0.0f, startAngle + lo * (endAngle - startAngle),
                               startAngle + hi * (endAngle - startAngle), true);
            g.setColour (accent.withAlpha (0.45f));
            g.strokePath (mod, PathStrokeType (jmax (1.5f, arcW * 0.45f), PathStrokeType::curved, PathStrokeType::rounded));
        }
    }

    if (std::abs (angle - zeroAngle) > 0.01f)
    {
        Path value;
        value.addCentredArc (c.x, c.y, arcR, arcR, 0.0f, jmin (zeroAngle, angle), jmax (zeroAngle, angle), true);
        const auto col = enabled ? accent : accent.withSaturation (0.0f).withAlpha (0.35f);
        g.setColour (col.withAlpha (0.16f));
        g.strokePath (value, PathStrokeType (arcW * 3.0f, PathStrokeType::curved, PathStrokeType::rounded));
        g.setColour (col);
        g.strokePath (value, PathStrokeType (arcW, PathStrokeType::curved, PathStrokeType::rounded));
    }

    // Cap: dark sea-glass with a faint accent rim light.
    const float capR = radius * 0.62f;
    auto cap = Rectangle<float> (capR * 2, capR * 2).withCentre (c);
    g.setColour (Colours::black.withAlpha (0.6f));
    g.fillEllipse (cap.translated (0, capR * 0.1f).expanded (1.5f));
    ColourGradient obs (Colour (0xff2c3c45), cap.getX(), cap.getY(), Colour (0xff0a1014), cap.getRight(), cap.getBottom(), false);
    g.setGradientFill (obs);
    g.fillEllipse (cap);
    g.setColour (accent.withAlpha (enabled ? 0.28f : 0.08f));
    g.drawEllipse (cap.reduced (0.6f), 1.0f);

    // Live value dot (where modulation / morph has actually moved it).
    if (props.contains ("livePos") && enabled)
    {
        const float lp = jlimit (0.0f, 1.0f, (float) props["livePos"]);
        if (std::abs (lp - pos) > 0.004f)
        {
            const float la = startAngle + lp * (endAngle - startAngle);
            const Point<float> p (c.x + arcR * std::sin (la), c.y - arcR * std::cos (la));
            theme::glow (g, p, arcW * 2.6f, accent, 0.6f);
            g.setColour (Colours::white);
            g.fillEllipse (Rectangle<float> (arcW * 1.3f, arcW * 1.3f).withCentre (p));
        }
    }

    Path pointer;
    const float pw = jmax (2.0f, capR * 0.13f);
    pointer.addRoundedRectangle (-pw * 0.5f, -capR * 0.9f, pw, capR * 0.48f, pw * 0.5f);
    g.setColour (enabled ? theme::text : theme::textFaint);
    g.fillPath (pointer, AffineTransform::rotation (angle).translated (c));
}

void VesselLookAndFeel::drawLinearSlider (Graphics& g, int x, int y, int w, int h, float pos, float, float,
                                        Slider::SliderStyle, Slider& s)
{
    // Bipolar horizontal bar (used for matrix amounts): fills from the centre.
    auto r = Rectangle<float> ((float) x, (float) y, (float) w, (float) h).reduced (1.0f, (float) h * 0.3f);
    g.setColour (theme::well);
    g.fillRoundedRectangle (r, 3.0f);
    const auto accent = s.findColour (Slider::trackColourId);
    const bool bipolar = s.getMinimum() < 0.0 && s.getMaximum() > 0.0;
    const float zeroX = bipolar ? r.getX() + r.getWidth() * (float) ((0.0 - s.getMinimum()) / (s.getMaximum() - s.getMinimum())) : r.getX();
    const float a = jmin (zeroX, pos), b = jmax (zeroX, pos);
    g.setColour (accent);
    g.fillRoundedRectangle (Rectangle<float> (a, r.getY(), jmax (1.0f, b - a), r.getHeight()), 3.0f);
    g.setColour (theme::edge);
    g.drawRoundedRectangle (r, 3.0f, 1.0f);
    if (bipolar)
    {
        g.setColour (theme::textFaint);
        g.drawVerticalLine ((int) zeroX, r.getY() - 2.0f, r.getBottom() + 2.0f);
    }
}

void VesselLookAndFeel::drawButtonBackground (Graphics& g, Button& b, const Colour&, bool highlighted, bool down)
{
    auto r = b.getLocalBounds().toFloat().reduced (0.5f);
    const bool onState = b.getToggleState();
    const auto accent = b.findColour (TextButton::buttonOnColourId);

    ColourGradient grad (Colour (0xff1f2d35), r.getX(), r.getY(), Colour (0xff111a20), r.getX(), r.getBottom(), false);
    if (onState) grad = ColourGradient (accent.withMultipliedBrightness (0.9f), r.getX(), r.getY(),
                                        accent.darker (0.9f), r.getX(), r.getBottom(), false);
    g.setGradientFill (grad);
    g.fillRoundedRectangle (r, 5.0f);
    if (highlighted && ! onState) { g.setColour (Colours::white.withAlpha (0.06f)); g.fillRoundedRectangle (r, 5.0f); }
    if (down)                     { g.setColour (Colours::black.withAlpha (0.25f)); g.fillRoundedRectangle (r, 5.0f); }
    g.setColour (onState ? accent.brighter (0.3f) : theme::edge);
    g.drawRoundedRectangle (r, 5.0f, 1.0f);
}

void VesselLookAndFeel::drawPopupMenuBackground (Graphics& g, int w, int h)
{
    g.fillAll (theme::panel);
    g.setColour (theme::edge);
    g.drawRect (0, 0, w, h, 1);
}

void VesselLookAndFeel::drawTooltip (Graphics& g, const String& text, int w, int h)
{
    auto r = Rectangle<float> (0, 0, (float) w, (float) h);
    g.setColour (theme::panelHi);
    g.fillRoundedRectangle (r, 5.0f);
    g.setColour (theme::aqua.withAlpha (0.6f));
    g.drawRoundedRectangle (r.reduced (0.5f), 5.0f, 1.0f);
    g.setColour (theme::text);
    g.setFont (theme::font (13.0f));
    g.drawFittedText (text, r.reduced (8.0f, 4.0f).toNearestInt(), Justification::centredLeft, 3);
}

Rectangle<int> VesselLookAndFeel::getTooltipBounds (const String& text, Point<int> pos, Rectangle<int> parent)
{
    const int w = jmin (320, (int) GlyphArrangement::getStringWidth (theme::font (13.0f), text) + 22);
    const int lines = jmax (1, (int) GlyphArrangement::getStringWidth (theme::font (13.0f), text) / 300 + 1);
    const int h = 10 + 17 * lines;
    return Rectangle<int> (pos.x > parent.getCentreX() ? pos.x - (w + 12) : pos.x + 18,
                           pos.y > parent.getCentreY() ? pos.y - (h + 8) : pos.y + 18, w, h)
        .constrainedWithin (parent);
}

} // namespace vessel
