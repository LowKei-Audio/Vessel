#pragma once
// The eight vessel silhouettes, one per material, drawn in a square box:
// wine glass, singing bowl, bell, cauldron, barrel, amphora, tube and drum.
// liquidRange() says where water can sit inside each one (fractions of the
// box height), so FILL looks right in every shape.

#include <juce_graphics/juce_graphics.h>

namespace vessel::shapes
{

struct Range { float top, bottom; };

inline Range liquidRange (int material)
{
    static constexpr Range r[8] = { { 0.10f, 0.55f }, { 0.40f, 0.79f }, { 0.16f, 0.81f }, { 0.36f, 0.81f },
                                    { 0.16f, 0.85f }, { 0.26f, 0.85f }, { 0.08f, 0.93f }, { 0.32f, 0.83f } };
    return r[juce::jlimit (0, 7, material)];
}

// Outline in unit coordinates (0..1, y down), then fitted to `box`.
inline juce::Path outline (int material, juce::Rectangle<float> box)
{
    juce::Path p;
    switch (juce::jlimit (0, 7, material))
    {
        case 0:   // Glass: wine glass
            p.startNewSubPath (0.22f, 0.08f);
            p.cubicTo (0.20f, 0.40f, 0.36f, 0.55f, 0.47f, 0.56f);
            p.lineTo (0.47f, 0.84f); p.lineTo (0.28f, 0.89f); p.lineTo (0.72f, 0.89f); p.lineTo (0.53f, 0.84f);
            p.lineTo (0.53f, 0.56f);
            p.cubicTo (0.64f, 0.55f, 0.80f, 0.40f, 0.78f, 0.08f);
            p.closeSubPath();
            break;
        case 1:   // Crystal: singing bowl
            p.startNewSubPath (0.07f, 0.38f);
            p.cubicTo (0.09f, 0.72f, 0.30f, 0.81f, 0.5f, 0.81f);
            p.cubicTo (0.70f, 0.81f, 0.91f, 0.72f, 0.93f, 0.38f);
            p.closeSubPath();
            break;
        case 2:   // Bronze: bell
            p.startNewSubPath (0.42f, 0.12f);
            p.lineTo (0.58f, 0.12f);
            p.cubicTo (0.70f, 0.14f, 0.70f, 0.30f, 0.72f, 0.50f);
            p.cubicTo (0.74f, 0.68f, 0.84f, 0.76f, 0.91f, 0.83f);
            p.lineTo (0.09f, 0.83f);
            p.cubicTo (0.16f, 0.76f, 0.26f, 0.68f, 0.28f, 0.50f);
            p.cubicTo (0.30f, 0.30f, 0.30f, 0.14f, 0.42f, 0.12f);
            p.closeSubPath();
            break;
        case 3:   // Iron: cauldron
            p.startNewSubPath (0.12f, 0.30f);
            p.lineTo (0.88f, 0.30f); p.lineTo (0.83f, 0.36f);
            p.cubicTo (0.92f, 0.56f, 0.80f, 0.81f, 0.5f, 0.83f);
            p.cubicTo (0.20f, 0.81f, 0.08f, 0.56f, 0.17f, 0.36f);
            p.closeSubPath();
            break;
        case 4:   // Wood: barrel
            p.startNewSubPath (0.25f, 0.13f);
            p.lineTo (0.75f, 0.13f);
            p.quadraticTo (0.88f, 0.50f, 0.75f, 0.87f);
            p.lineTo (0.25f, 0.87f);
            p.quadraticTo (0.12f, 0.50f, 0.25f, 0.13f);
            p.closeSubPath();
            break;
        case 5:   // Clay: amphora
            p.startNewSubPath (0.39f, 0.10f);
            p.lineTo (0.61f, 0.10f); p.lineTo (0.58f, 0.24f);
            p.cubicTo (0.85f, 0.30f, 0.87f, 0.66f, 0.62f, 0.87f);
            p.lineTo (0.38f, 0.87f);
            p.cubicTo (0.13f, 0.66f, 0.15f, 0.30f, 0.42f, 0.24f);
            p.closeSubPath();
            break;
        case 6:   // Tube: test tube
            p.startNewSubPath (0.38f, 0.05f);
            p.lineTo (0.62f, 0.05f); p.lineTo (0.62f, 0.82f);
            p.quadraticTo (0.62f, 0.95f, 0.5f, 0.95f);
            p.quadraticTo (0.38f, 0.95f, 0.38f, 0.82f);
            p.closeSubPath();
            break;
        default:  // Skin: drum
            p.startNewSubPath (0.13f, 0.30f);
            p.lineTo (0.13f, 0.78f);
            p.quadraticTo (0.5f, 0.90f, 0.87f, 0.78f);
            p.lineTo (0.87f, 0.30f);
            p.quadraticTo (0.5f, 0.42f, 0.13f, 0.30f);
            p.closeSubPath();
            p.addEllipse (0.13f, 0.22f, 0.74f, 0.16f);
            break;
    }
    p.applyTransform (juce::AffineTransform::scale (box.getWidth(), box.getHeight()).translated (box.getX(), box.getY()));
    return p;
}

} // namespace vessel::shapes
