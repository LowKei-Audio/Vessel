#pragma once
// The living vessel: the heart of the UI.
//   - The silhouette follows the Material.
//   - Drag up / down anywhere to pour water in or out (FILL); scroll for fine
//     steps; double-click to empty it. The surface sloshes with the Liquid
//     clock, ripples with the input and bubbles with Turbulence.
//   - Rings radiate from the vessel as its modes sing (one per mode, brighter
//     the louder it rings); the strip at the bottom shows every mode's level
//     at its pitch.
//   - Cracks spread with Fracture; SHATTER breaks it into drifting pieces;
//     SEAL closes a frosted lid on top.

#include "Controls.h"
#include "VesselShapes.h"

namespace vessel
{

class VesselView : public juce::Component, public juce::SettableTooltipClient
{
public:
    explicit VesselView (VesselProcessor& p) : proc (p)
    {
        auto* prm = p.apvts.getParameter (def (P::fill).id);
        fillAttachment = std::make_unique<juce::ParameterAttachment> (*prm, [this] (float v) { setFill = v; });
        fillAttachment->sendInitialUpdate();
        setMouseCursor (juce::MouseCursor::UpDownResizeCursor);
        setTooltip ("Drag up or down to pour water in or out (Fill). Scroll for fine steps, double-click to empty.");

        // Crack paths (unit coords), fixed so they always grow the same way.
        juce::Random rng (7);
        for (auto& c : cracks)
        {
            juce::Point<float> pt (0.3f + 0.4f * rng.nextFloat(), 0.2f + 0.5f * rng.nextFloat());
            c.startNewSubPath (pt);
            const float dir = rng.nextFloat() * juce::MathConstants<float>::twoPi;
            for (int s = 0; s < 6; ++s)
            {
                const float a = dir + (rng.nextFloat() - 0.5f) * 1.4f;
                pt += juce::Point<float> (std::cos (a), std::sin (a)) * (0.03f + 0.04f * rng.nextFloat());
                c.lineTo (pt);
            }
        }
    }

    void setCompact (bool c) { compact = c; repaint(); }

    void tick()
    {
        time += 1.0f / 30.0f;
        auto n = [this] (P p) { return proc.uiNorm[(size_t) idx (p)].load (std::memory_order_relaxed); };
        material = juce::roundToInt (proc.apvts.getRawParameterValue (def (P::bdMaterial).id)->load());
        fill = n (P::fill);
        const bool liquidOn = proc.uiStageOn[dsp::liquid].load();
        slosh = liquidOn ? n (P::lqSlosh) : 0.0f;
        turbulence = liquidOn ? n (P::lqTurb) : 0.0f;
        sloshPhase = proc.uiSloshPhase.load();
        bodyLevel = juce::jmax (proc.uiBodyLevel.load(), bodyLevel * 0.9f);
        inLevel = juce::jmax (proc.uiInPeak.load(), inLevel * 0.85f);
        crack = proc.uiStageOn[dsp::fracture].load() ? juce::jmax (n (P::frCrack), n (P::frFold) * 0.5f, n (P::frGrit) * 0.5f) * n (P::frMix) : 0.0f;
        shatter = proc.uiShatter.load();
        seal += ((proc.uiSeal.load() > 0.5f ? 1.0f : 0.0f) - seal) * 0.35f;
        for (int k = 0; k < dsp::kMaxModes; ++k)
        {
            // Square root: quiet modes still show, loud ones do not swamp the view.
            modeLevel[(size_t) k] = std::sqrt (juce::jmax (0.0f, proc.uiModeLevel[(size_t) k].load (std::memory_order_relaxed)));
            modeHz[(size_t) k] = proc.uiModeHz[(size_t) k].load (std::memory_order_relaxed);
        }
        fundamental = proc.uiFundamental.load();
        repaint();
    }

    juce::String tuningText() const
    {
        auto raw = [this] (P p) { return proc.apvts.getRawParameterValue (def (p).id)->load(); };
        const int tune = juce::roundToInt (raw (P::bdTune));
        if (tune == 0) return formatValue (def (P::bdPitch), raw (P::bdPitch));
        const auto note = Choices::notes[juce::roundToInt (raw (P::bdRoot))] + juce::String (juce::roundToInt (raw (P::bdOctave)));
        return tune == 2 ? note + "  " + Choices::chords[juce::roundToInt (raw (P::bdChord))] : note;
    }

    void paint (juce::Graphics& g) override
    {
        auto bounds = getLocalBounds().toFloat();
        g.setColour (theme::screen);
        g.fillRoundedRectangle (bounds, 10.0f);
        {
            // Faint caustic light from below.
            juce::ColourGradient cg (theme::aquaDeep.withAlpha (0.25f + 0.25f * bodyLevel), bounds.getCentreX(), bounds.getBottom(),
                                     theme::aquaDeep.withAlpha (0.0f), bounds.getCentreX(), bounds.getY() + bounds.getHeight() * 0.25f, true);
            g.setGradientFill (cg);
            g.fillRoundedRectangle (bounds, 10.0f);
        }
        g.setColour (theme::edge);
        g.drawRoundedRectangle (bounds.reduced (0.5f), 10.0f, 1.0f);

        auto area = bounds.reduced (compact ? 10.0f : 16.0f);
        auto strip = area.removeFromBottom (compact ? 18.0f : 30.0f);
        auto header = area.removeFromTop (compact ? 16.0f : 22.0f);
        const float side = juce::jmin (area.getWidth(), area.getHeight());
        box = juce::Rectangle<float> (side, side).withCentre (area.getCentre());
        const auto c = box.getCentre();

        theme::glow (g, c, side * 0.75f, seal > 0.5f ? theme::ice : theme::aqua, 0.05f + 0.3f * juce::jmin (1.0f, bodyLevel));

        // Rings: the vessel's outline radiating outward as its modes ring.
        const auto shape = shapes::outline (material, box);
        for (int k = 0; k < 6; ++k)
        {
            const float lvl = juce::jmin (1.0f, modeLevel[(size_t) k] * 1.5f);
            if (lvl < 0.02f) continue;
            const float grow = 1.0f + 0.045f * (float) (k + 1) + 0.012f * std::sin (time * (3.0f + (float) k));
            g.setColour ((seal > 0.5f ? theme::ice : theme::aqua).withAlpha (0.32f * lvl));
            g.strokePath (shape, juce::PathStrokeType (1.2f), juce::AffineTransform::scale (grow, grow, c.x, c.y));
        }

        if (shatter < 0.01f)
            drawVessel (g, shape);
        else
        {
            // Six wedges drifting apart from the centre.
            for (int w = 0; w < 6; ++w)
            {
                const float a0 = juce::MathConstants<float>::twoPi * (float) w / 6.0f + 0.3f;
                const float a1 = a0 + juce::MathConstants<float>::twoPi / 6.0f;
                juce::Path wedge;
                wedge.startNewSubPath (c);
                wedge.lineTo (c + juce::Point<float> (std::cos (a0), std::sin (a0)) * side * 1.5f);
                wedge.lineTo (c + juce::Point<float> (std::cos (a1), std::sin (a1)) * side * 1.5f);
                wedge.closeSubPath();
                const float mid = 0.5f * (a0 + a1);
                const auto off = juce::Point<float> (std::cos (mid), std::sin (mid)) * shatter * side * 0.07f;
                juce::Graphics::ScopedSaveState s (g);
                g.reduceClipRegion (wedge);
                g.addTransform (juce::AffineTransform::rotation (shatter * 0.08f * (w % 2 ? 1.0f : -1.0f), c.x, c.y).translated (off));
                drawVessel (g, shape);
            }
        }

        // Header: material + tuning.
        theme::legend (g, Choices::materials[material], header, theme::aqua, compact ? 10.5f : 13.0f, juce::Justification::centredLeft, true, 0.2f);
        theme::legend (g, tuningText(), header, theme::text, compact ? 10.5f : 13.0f, juce::Justification::centredRight, true, 0.08f);

        // Mode strip: each mode's level at its pitch (log scale 30 Hz .. 16 kHz).
        g.setColour (theme::well);
        g.fillRoundedRectangle (strip, 4.0f);
        const float lo = std::log (30.0f), hi = std::log (16000.0f);
        for (int k = 0; k < dsp::kMaxModes; ++k)
        {
            if (modeHz[(size_t) k] < 20.0f) continue;
            const float x = strip.getX() + 4.0f + (strip.getWidth() - 8.0f) * juce::jlimit (0.0f, 1.0f, (std::log (modeHz[(size_t) k]) - lo) / (hi - lo));
            const float hgt = (strip.getHeight() - 6.0f) * juce::jlimit (0.05f, 1.0f, modeLevel[(size_t) k] * 1.4f);
            g.setColour ((k == 0 ? theme::text : theme::aqua).withAlpha (0.35f + 0.65f * juce::jmin (1.0f, modeLevel[(size_t) k] * 2.0f)));
            g.fillRect (juce::Rectangle<float> (x - 1.0f, strip.getBottom() - 3.0f - hgt, 2.5f, hgt));
        }
        if (! compact)
        {
            theme::legend (g, "Fill " + juce::String (juce::roundToInt (fill * 100.0f)) + "%", header.withY (strip.getY() - 22.0f).withHeight (18.0f),
                           theme::textDim, 11.0f, juce::Justification::centredRight);
            if (hover && ! dragging)
                theme::legend (g, "Drag to pour", header.withY (strip.getY() - 22.0f).withHeight (18.0f), theme::textFaint, 11.0f,
                               juce::Justification::centredLeft);
        }
    }

    // Mouse: pour --------------------------------------------------------------------------------
    void mouseEnter (const juce::MouseEvent&) override { hover = true; }
    void mouseExit (const juce::MouseEvent&) override  { hover = false; }
    void mouseDown (const juce::MouseEvent& e) override
    {
        dragging = true;
        dragStartFill = setFill;
        dragStartY = e.position.y;
        fillAttachment->beginGesture();
    }
    void mouseDrag (const juce::MouseEvent& e) override
    {
        const auto r = shapes::liquidRange (material);
        const float span = juce::jmax (40.0f, box.getHeight() * (r.bottom - r.top));
        fillAttachment->setValueAsPartOfGesture (juce::jlimit (0.0f, 1.0f, dragStartFill - (e.position.y - dragStartY) / span));
    }
    void mouseUp (const juce::MouseEvent&) override { dragging = false; fillAttachment->endGesture(); }
    void mouseDoubleClick (const juce::MouseEvent&) override { fillAttachment->setValueAsCompleteGesture (0.0f); }
    void mouseWheelMove (const juce::MouseEvent&, const juce::MouseWheelDetails& w) override
    {
        fillAttachment->setValueAsCompleteGesture (juce::jlimit (0.0f, 1.0f, setFill + (w.deltaY > 0 ? 0.02f : -0.02f)));
    }

private:
    void drawVessel (juce::Graphics& g, const juce::Path& shape)
    {
        const auto r = shapes::liquidRange (material);
        const float top = box.getY() + box.getHeight() * r.top, bottom = box.getY() + box.getHeight() * r.bottom;

        // Glass body: dark, translucent, lit from the edge.
        juce::ColourGradient body (juce::Colour (0xff15242b).withAlpha (0.85f), box.getX(), box.getY(),
                                   juce::Colour (0xff070d10).withAlpha (0.9f), box.getRight(), box.getBottom(), false);
        g.setGradientFill (body);
        g.fillPath (shape);

        // Water.
        if (fill > 0.005f)
        {
            juce::Graphics::ScopedSaveState s (g);
            g.reduceClipRegion (shape);
            const float level = bottom - fill * (bottom - top);
            const float tilt = slosh * 0.22f * std::sin (juce::MathConstants<float>::twoPi * sloshPhase);
            const float amp = 1.5f + box.getHeight() * (0.012f * slosh + 0.03f * juce::jmin (1.0f, inLevel));
            juce::Path water, surface;
            const float x0 = box.getX(), x1 = box.getRight();
            for (float x = x0; x <= x1 + 0.5f; x += 4.0f)
            {
                const float u = (x - x0) / box.getWidth();
                const float y = level + tilt * (x - box.getCentreX()) + amp * std::sin (juce::MathConstants<float>::twoPi * (u * 1.6f + time * 0.55f))
                              + 0.5f * amp * std::sin (juce::MathConstants<float>::twoPi * (u * 3.7f - time * 0.9f));
                if (x == x0) { water.startNewSubPath (x, y); surface.startNewSubPath (x, y); }
                else         { water.lineTo (x, y); surface.lineTo (x, y); }
            }
            water.lineTo (x1, box.getBottom());
            water.lineTo (x0, box.getBottom());
            water.closeSubPath();
            const auto tint = seal > 0.5f ? theme::ice : theme::aqua;
            juce::ColourGradient wg (tint.withAlpha (0.55f), 0.0f, level, theme::aquaDeep.withAlpha (0.9f), 0.0f, bottom, false);
            g.setGradientFill (wg);
            g.fillPath (water);
            g.setColour (juce::Colours::white.withAlpha (0.55f));
            g.strokePath (surface, juce::PathStrokeType (1.4f));

            // Bubbles rise with turbulence.
            if (turbulence > 0.02f)
            {
                g.setColour (juce::Colours::white.withAlpha (0.35f * turbulence));
                for (int b = 0; b < 9; ++b)
                {
                    const float u = 0.2f + 0.6f * std::fmod (0.37f * (float) b + 0.13f, 1.0f);
                    const float rise = std::fmod (time * (0.15f + 0.05f * (float) b) + 0.29f * (float) b, 1.0f);
                    const float y = bottom - rise * (bottom - level);
                    const float rad = 1.5f + 1.5f * (float) (b % 3);
                    g.drawEllipse (box.getX() + u * box.getWidth() - rad, y - rad, rad * 2.0f, rad * 2.0f, 1.0f);
                }
            }
        }

        // Frost when sealed.
        if (seal > 0.02f)
        {
            g.setColour (theme::ice.withAlpha (0.12f * seal));
            g.fillPath (shape);
        }

        // Outline and a glass highlight.
        g.setColour ((seal > 0.5f ? theme::ice : theme::aqua.interpolatedWith (theme::text, 0.5f)).withAlpha (0.9f));
        g.strokePath (shape, juce::PathStrokeType (2.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        {
            juce::Graphics::ScopedSaveState s (g);
            g.reduceClipRegion (shape);
            juce::ColourGradient hl (juce::Colours::white.withAlpha (0.10f), box.getX() + box.getWidth() * 0.3f, 0.0f,
                                     juce::Colours::white.withAlpha (0.0f), box.getX() + box.getWidth() * 0.45f, 0.0f, false);
            g.setGradientFill (hl);
            g.fillPath (shape);
        }

        // Cracks.
        const int n = (int) std::ceil (juce::jlimit (0.0f, 1.0f, crack + shatter) * (float) cracks.size());
        for (int k = 0; k < n; ++k)
        {
            g.setColour (theme::shatter.interpolatedWith (juce::Colours::white, 0.4f).withAlpha (0.75f));
            g.strokePath (cracks[(size_t) k], juce::PathStrokeType (1.1f),
                          juce::AffineTransform::scale (box.getWidth(), box.getHeight()).translated (box.getX(), box.getY()));
        }

        // The lid.
        if (seal > 0.02f)
        {
            auto lid = juce::Rectangle<float> (box.getWidth() * 0.62f, 6.0f).withCentre ({ box.getCentreX(), top - 5.0f });
            theme::glow (g, lid.getCentre(), lid.getWidth() * 0.5f, theme::ice, 0.3f * seal);
            g.setColour (theme::ice.withAlpha (seal));
            g.fillRoundedRectangle (lid, 3.0f);
            theme::legend (g, "Sealed", lid.withHeight (16.0f).translated (0.0f, -20.0f), theme::ice.withAlpha (seal), 11.0f);
        }
    }

    VesselProcessor& proc;
    std::unique_ptr<juce::ParameterAttachment> fillAttachment;
    std::array<juce::Path, 10> cracks;
    std::array<float, dsp::kMaxModes> modeLevel {}, modeHz {};
    juce::Rectangle<float> box;
    int material = 0;
    float time = 0.0f, fill = 0.0f, setFill = 0.0f, slosh = 0.0f, turbulence = 0.0f, sloshPhase = 0.0f;
    float bodyLevel = 0.0f, inLevel = 0.0f, crack = 0.0f, shatter = 0.0f, seal = 0.0f, fundamental = 220.0f;
    float dragStartFill = 0.0f, dragStartY = 0.0f;
    bool compact = false, hover = false, dragging = false;
};

} // namespace vessel
