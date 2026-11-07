#include "PluginEditor.h"

namespace vessel
{
using namespace juce;

static constexpr const char* kUiWidthProp = "uiWidth";
static constexpr const char* kUiViewProp = "uiView";

// ---------------------------------------------------------------------------
VesselEditor::Content::Content (VesselProcessor& p)
    : proc (p), presetBar (p), easy (p), proView (p), hint (p)
{
    for (auto* c : std::initializer_list<Component*> { &presetBar, &easyTab, &proTab, &undoBtn, &redoBtn, &easy, &proView, &hint })
        addAndMakeVisible (c);
    for (auto* b : { &easyTab, &proTab, &undoBtn, &redoBtn }) b->setMouseCursor (MouseCursor::PointingHandCursor);

    easyTab.onClick = [this] { setView (false); };
    proTab.onClick = [this] { setView (true); };
    undoBtn.onClick = [this] { proc.undo.undo(); };
    redoBtn.onClick = [this] { proc.undo.redo(); };
    easyTab.setTooltip ("Easy view: the vessel, tuning, Seal, Shatter and the macros. Every preset is playable here.");
    proTab.setTooltip ("Pro view: every stage, the modulation matrix and global settings");
    undoBtn.setTooltip ("Undo");
    redoBtn.setTooltip ("Redo");

    setSize (kDesignW, kDesignH);
    setView ((bool) proc.apvts.state.getProperty (kUiViewProp, false));
}

void VesselEditor::Content::setView (bool shouldBePro)
{
    pro = shouldBePro;
    proc.apvts.state.setProperty (kUiViewProp, pro, nullptr);
    easy.setVisible (! pro);
    proView.setVisible (pro);
    easyTab.setToggleState (! pro, dontSendNotification);
    proTab.setToggleState (pro, dontSendNotification);
}

void VesselEditor::Content::showProPage (int page)
{
    setView (true);
    proView.showPage (page);
}

void VesselEditor::Content::tick()
{
    if (pro) proView.tick(); else easy.tick();
    hint.tick();
    undoBtn.setEnabled (proc.undo.canUndo());
    redoBtn.setEnabled (proc.undo.canRedo());
}

void VesselEditor::Content::paint (Graphics& g)
{
    // Chassis: dark gradient, fine grain, an aqua glow from below like light through water.
    ColourGradient chassis (theme::chassisTop, 0, 0, theme::chassisBot, 0, (float) getHeight(), false);
    g.setGradientFill (chassis);
    g.fillAll();
    ColourGradient underglow (theme::aquaDeep.withAlpha (0.30f), (float) getWidth() * 0.5f, (float) getHeight() * 1.1f,
                          theme::aquaDeep.withAlpha (0.0f), (float) getWidth() * 0.5f, (float) getHeight() * 0.35f, true);
    g.setGradientFill (underglow);
    g.fillAll();
    g.setTiledImageFill (grainTexture(), 0, 0, 0.8f);
    g.fillAll();

    // Wordmark with a red bloom.
    auto logo = Rectangle<float> (24.0f, 12.0f, 250.0f, 42.0f);
    theme::glow (g, { logo.getX() + 110.0f, logo.getCentreY() }, 120.0f, theme::aqua, 0.12f);
    theme::legend (g, "Vessel", logo.removeFromTop (28.0f), theme::text, 26.0f, Justification::centredLeft, true, 0.42f);
    theme::legend (g, "LowKei Audio  -  Pour. Ring. Seal. Shatter.", logo, theme::aqua, 9.5f, Justification::centredLeft, true, 0.16f);

    g.setColour (Colours::black.withAlpha (0.6f));
    g.drawHorizontalLine (66, 16.0f, (float) getWidth() - 16.0f);
    g.setColour (theme::aqua.withAlpha (0.18f));
    g.drawHorizontalLine (67, 16.0f, (float) getWidth() - 16.0f);

    g.setColour (theme::textFaint);
    g.setFont (theme::font (11.0f));
    g.drawText ("v" JucePlugin_VersionString, Rectangle<int> (getWidth() - 90, getHeight() - 16, 80, 14), Justification::centredRight, false);
}

void VesselEditor::Content::resized()
{
    auto top = Rectangle<int> (0, 14, getWidth(), 40).reduced (20, 0);
    top.removeFromLeft (270);
    auto right = top.removeFromRight (330);
    redoBtn.setBounds (right.removeFromRight (62).withSizeKeepingCentre (62, 32));
    right.removeFromRight (6);
    undoBtn.setBounds (right.removeFromRight (62).withSizeKeepingCentre (62, 32));
    right.removeFromRight (18);
    proTab.setBounds (right.removeFromRight (78).withSizeKeepingCentre (78, 32));
    right.removeFromRight (4);
    easyTab.setBounds (right.removeFromRight (78).withSizeKeepingCentre (78, 32));
    presetBar.setBounds (top.reduced (10, 0));

    auto main = Rectangle<int> (20, 80, getWidth() - 40, getHeight() - 80 - 66);
    easy.setBounds (main);
    proView.setBounds (main);
    hint.setBounds (20, getHeight() - 58, getWidth() - 40, 42);
}

// ---------------------------------------------------------------------------
VesselEditor::VesselEditor (VesselProcessor& p)
    : AudioProcessorEditor (p), proc (p), content (p)
{
    // Read the saved size first: setting the resize limits below resizes the
    // editor to its minimum, and resized() would overwrite the saved width.
    const int savedW = proc.apvts.state.getProperty (kUiWidthProp, kDesignW);

    setLookAndFeel (&lnf);
    content.setLookAndFeel (&lnf);
    addAndMakeVisible (content);

    setResizable (true, true);
    setResizeLimits (kDesignW * 7 / 10, kDesignH * 7 / 10, kDesignW * 2, kDesignH * 2);
    getConstrainer()->setFixedAspectRatio ((double) kDesignW / (double) kDesignH);

    const int w = jlimit (kDesignW * 7 / 10, kDesignW * 2, savedW);
    setSize (w, w * kDesignH / kDesignW);
    startTimerHz (30);
}

VesselEditor::~VesselEditor()
{
    stopTimer();
    content.setLookAndFeel (nullptr);
    setLookAndFeel (nullptr);
}

void VesselEditor::paint (Graphics& g) { g.fillAll (theme::background); }

void VesselEditor::resized()
{
    const float scale = (float) getWidth() / (float) kDesignW;
    content.setTransform (AffineTransform::scale (scale));
    content.setBounds (0, 0, kDesignW, kDesignH);
    proc.apvts.state.setProperty (kUiWidthProp, getWidth(), nullptr);
}

} // namespace vessel
