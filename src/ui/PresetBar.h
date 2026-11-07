#pragma once
// Top-bar preset browser:
//   [<] [ Category / Preset Name  v ] [>] [SAVE] [A][B]  [dice]
// Click the name for a categorised menu. The dice is "Pour Me": a random but
// musical vessel.

#include "VesselLookAndFeel.h"
#include "../PluginProcessor.h"

namespace vessel
{

class PresetBar : public juce::Component, private juce::ChangeListener
{
public:
    explicit PresetBar (VesselProcessor& p) : proc (p), presets (p.presets)
    {
        for (auto* b : { &prev, &next, &save, &slotA, &slotB })
        {
            addAndMakeVisible (b);
            b->setMouseCursor (juce::MouseCursor::PointingHandCursor);
        }
        addAndMakeVisible (nameButton);
        addAndMakeVisible (dice);

        prev.onClick = [this] { presets.previous(); };
        next.onClick = [this] { presets.next(); };
        save.onClick = [this] { showSaveDialog(); };
        nameButton.onClick = [this] { showMenu(); };
        slotA.onClick = [this] { presets.selectSlot (0); };
        slotB.onClick = [this] { presets.selectSlot (1); };
        dice.onClick = [this] { presets.randomize ((std::uint32_t) juce::Random::getSystemRandom().nextInt (1 << 30)); };

        prev.setTooltip ("Previous preset");
        next.setTooltip ("Next preset");
        save.setTooltip ("Save the current sound as a preset");
        slotA.setTooltip ("Compare: setting A");
        slotB.setTooltip ("Compare: setting B (starts as a copy of A)");
        dice.setTooltip ("Pour Me: a random, musical vessel");

        presets.addChangeListener (this);
        refresh();
    }

    ~PresetBar() override { presets.removeChangeListener (this); }

    void resized() override
    {
        auto r = getLocalBounds();
        dice.setBounds (r.removeFromRight (42));
        r.removeFromRight (10);
        slotB.setBounds (r.removeFromRight (32));
        r.removeFromRight (4);
        slotA.setBounds (r.removeFromRight (32));
        r.removeFromRight (10);
        save.setBounds (r.removeFromRight (56));
        r.removeFromRight (6);
        prev.setBounds (r.removeFromLeft (30));
        next.setBounds (r.removeFromRight (30));
        nameButton.setBounds (r.reduced (5, 0));
    }

private:
    class NameDisplay : public juce::Button
    {
    public:
        NameDisplay() : juce::Button ("preset") { setMouseCursor (juce::MouseCursor::PointingHandCursor); }
        juce::String category, name;

        void paintButton (juce::Graphics& g, bool highlighted, bool) override
        {
            auto r = getLocalBounds().toFloat().reduced (0.5f);
            g.setColour (theme::screen);
            g.fillRoundedRectangle (r, 5.0f);
            g.setColour (highlighted ? theme::aqua.withAlpha (0.7f) : theme::edge);
            g.drawRoundedRectangle (r, 5.0f, 1.0f);
            auto inner = r.reduced (12.0f, 0.0f);
            auto arrow = inner.removeFromRight (10.0f);
            juce::Path tri;
            tri.addTriangle (arrow.getX(), arrow.getCentreY() - 2.5f, arrow.getRight(), arrow.getCentreY() - 2.5f,
                             arrow.getCentreX(), arrow.getCentreY() + 3.5f);
            g.setColour (theme::aqua);
            g.fillPath (tri);
            auto cat = inner.removeFromTop (inner.getHeight() * 0.42f);
            theme::legend (g, category, cat.withTrimmedTop (2.0f), theme::aqua, 9.5f, juce::Justification::bottomLeft);
            g.setColour (theme::text);
            g.setFont (theme::font (16.0f, true));
            g.drawText (name, inner, juce::Justification::topLeft, true);
        }
    };

    class DiceButton : public juce::Button
    {
    public:
        DiceButton() : juce::Button ("dice") { setMouseCursor (juce::MouseCursor::PointingHandCursor); }
        void paintButton (juce::Graphics& g, bool highlighted, bool down) override
        {
            auto r = getLocalBounds().toFloat().reduced (3.0f);
            const float s = juce::jmin (r.getWidth(), r.getHeight());
            auto d = juce::Rectangle<float> (s, s).withCentre (r.getCentre());
            if (highlighted) theme::glow (g, d.getCentre(), s * 0.9f, theme::aqua, 0.35f);
            g.setColour (down ? theme::aquaDeep : theme::panelHi);
            g.fillRoundedRectangle (d, 6.0f);
            g.setColour (highlighted ? theme::aqua : theme::edge);
            g.drawRoundedRectangle (d, 6.0f, 1.2f);
            g.setColour (theme::aqua);
            const float pr = s * 0.09f;
            for (auto p : { juce::Point<float> (0.28f, 0.28f), juce::Point<float> (0.72f, 0.28f), juce::Point<float> (0.5f, 0.5f),
                            juce::Point<float> (0.28f, 0.72f), juce::Point<float> (0.72f, 0.72f) })
                g.fillEllipse (d.getX() + p.x * s - pr, d.getY() + p.y * s - pr, pr * 2, pr * 2);
        }
    };

    void changeListenerCallback (juce::ChangeBroadcaster*) override { refresh(); }

    void refresh()
    {
        const auto& entries = presets.getEntries();
        const int i = presets.getCurrentIndex();
        nameButton.name = presets.getCurrentName();
        nameButton.category = juce::isPositiveAndBelow (i, entries.size()) && entries[i].name == nameButton.name
                                  ? entries[i].category : juce::String ("Random / Edited");
        slotA.setToggleState (! presets.isBActive(), juce::dontSendNotification);
        slotB.setToggleState (presets.isBActive(), juce::dontSendNotification);
        nameButton.repaint();
    }

    void showMenu()
    {
        presets.refreshUserPresets();
        juce::PopupMenu menu;
        const auto& entries = presets.getEntries();
        juce::String lastCat;
        juce::PopupMenu sub;
        auto flush = [&]
        {
            if (lastCat.isNotEmpty()) menu.addSubMenu (lastCat, sub);
            sub = juce::PopupMenu();
        };
        for (int i = 0; i < entries.size(); ++i)
        {
            if (entries[i].category != lastCat) { flush(); lastCat = entries[i].category; }
            sub.addItem (i + 1, entries[i].name, true, i == presets.getCurrentIndex());
        }
        flush();
        menu.addSeparator();
        constexpr int kSave = 10000, kFolder = 10001, kCopy = 10002;
        menu.addItem (kSave, "Save As...");
        menu.addItem (kCopy, presets.isBActive() ? "Copy B to A" : "Copy A to B");
        menu.addItem (kFolder, "Open Preset Folder");
        menu.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (nameButton).withMinimumWidth (nameButton.getWidth()),
                            [this] (int result)
                            {
                                if (result == kSave)        showSaveDialog();
                                else if (result == kCopy)   presets.copyActiveToOther();
                                else if (result == kFolder) { auto f = PresetManager::getUserPresetFolder(); f.createDirectory(); f.startAsProcess(); }
                                else if (result > 0)        presets.load (result - 1);
                            });
    }

    void showSaveDialog()
    {
        auto* w = new juce::AlertWindow ("Save Preset", "Name your preset:", juce::MessageBoxIconType::NoIcon, this);
        w->addTextEditor ("name", presets.getCurrentName());
        w->addButton ("Save", 1, juce::KeyPress (juce::KeyPress::returnKey));
        w->addButton ("Cancel", 0, juce::KeyPress (juce::KeyPress::escapeKey));
        w->enterModalState (true, juce::ModalCallbackFunction::create ([this, w] (int result)
        {
            if (result == 1 && ! presets.saveUserPreset (w->getTextEditorContents ("name")))
                juce::AlertWindow::showMessageBoxAsync (juce::MessageBoxIconType::WarningIcon, "Preset not saved",
                                                        "Please check the name and that the Documents folder is writable.");
        }), true);
    }

    VesselProcessor& proc;
    PresetManager& presets;
    juce::TextButton prev { "<" }, next { ">" }, save { "SAVE" }, slotA { "A" }, slotB { "B" };
    NameDisplay nameButton;
    DiceButton dice;
};

} // namespace vessel
