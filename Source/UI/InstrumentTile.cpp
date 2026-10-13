#include "InstrumentTile.h"

#include "../Core/ParamIDs.h"
#include "../IO/UserSettings.h"
#include "../Plugin/PluginProcessor.h"
#include "Theme.h"

namespace jgl
{

InstrumentRow::InstrumentRow (GrooveLabProcessor& p, InstrumentId i) : processor (p), id (i)
{
    const auto& info = instrumentInfo (id);
    setTooltip (juce::String (info.name) + ": " + info.role + ". Click to add or remove.");
    setMouseCursor (juce::MouseCursor::PointingHandCursor);

    if (auto* param = processor.getState().getParameter (ParamIDs::instrument (id)))
    {
        attachment = std::make_unique<juce::ParameterAttachment> (*param, [this] (float v)
        {
            enabled = v > 0.5f;
            updateButtons();
            repaint();
        });
        attachment->sendInitialUpdate();
    }

    favButton.setClickingTogglesState (true);
    favButton.setToggleState (UserSettings::isFavourite (info.key), juce::dontSendNotification);
    favButton.onClick = [this]
    {
        UserSettings::setFavourite (instrumentInfo (id).key, favButton.getToggleState());
        if (onFavouriteChanged) onFavouriteChanged();
    };

    lockButton.setClickingTogglesState (true);
    lockButton.onClick = [this] { processor.setLocked (id, lockButton.getToggleState()); };
    regenButton.onClick = [this] { processor.regeneratePart (id); };

    for (auto* b : { &favButton, &lockButton, &regenButton })
        addAndMakeVisible (b);

    if (id == InstrumentId::CustomVoice)
    {
        loadButton.onClick = [this] { if (onLoadSample) onLoadSample(); };
        settingsButton.onClick = [this] { if (onVoiceSettings) onVoiceSettings(); };
        addAndMakeVisible (loadButton);
        addAndMakeVisible (settingsButton);
    }
    refresh();
}

void InstrumentRow::updateButtons()
{
    const bool show = enabled && (isMouseOver (true) || lockButton.getToggleState());
    lockButton.setVisible (show || lockButton.getToggleState());
    regenButton.setVisible (enabled && isMouseOver (true));
}

void InstrumentRow::refresh()
{
    lockButton.setToggleState (processor.isLocked (id), juce::dontSendNotification);
    updateButtons();

    bool samples = false;
    if (auto lib = processor.getService().getSamples())
    {
        if (isMelodic (id))
            samples = lib->hasMelodicNotes (id);
        else
            for (int a = 0; a < (int) instrumentInfo (id).articulations.size(); ++a)
                samples = samples || lib->hasArticulation (id, a);
    }
    if (samples != usesSamples)
    {
        usesSamples = samples;
        repaint();
    }
}

void InstrumentRow::paint (juce::Graphics& g)
{
    const auto& info = instrumentInfo (id);
    auto r = getLocalBounds().toFloat();
    const bool over = isMouseOver (true);

    if (enabled)
    {
        juce::ColourGradient hl (Colours::copperLight.withAlpha (0.55f), r.getX(), r.getY(),
                                 Colours::copperLight.withAlpha (0.12f), r.getRight(), r.getY(), false);
        g.setGradientFill (hl);
        g.fillRect (r.reduced (0.0f, 1.0f));
        g.setColour (Colours::copper);
        g.fillRect (r.getX(), r.getY() + 1.0f, 3.0f, r.getHeight() - 2.0f);
    }
    else if (over)
    {
        g.setColour (Colours::copperLight.withAlpha (0.15f));
        g.fillRect (r.reduced (0.0f, 1.0f));
    }

    g.setColour (Colours::line.withAlpha (0.8f));
    g.drawHorizontalLine ((int) r.getBottom() - 1, r.getX() + 14.0f, r.getRight() - 8.0f);

    auto iconArea = r.withWidth (r.getHeight() * 1.45f).reduced (6.0f, 6.0f).translated (12.0f, 0.0f);
    {
        juce::Graphics::ScopedSaveState s (g);
        if (! enabled) g.setOpacity (0.85f);
        decor::drawInstrument (g, id, iconArea);
    }

    auto text = r.withLeft (iconArea.getRight() + 16.0f).withTrimmedRight (id == InstrumentId::CustomVoice ? 140.0f : 90.0f);
    g.setColour (Colours::ink);
    g.setFont (Fonts::serif (20.0f, enabled));
    g.drawText (info.name, text.removeFromTop (r.getHeight() * 0.56f).withTrimmedTop (4.0f), juce::Justification::bottomLeft);
    g.setColour (Colours::inkSoft);
    g.setFont (Fonts::serif (15.0f));
    juce::String role = info.role;
    if (usesSamples) role << "  |  samples";
    if (id == InstrumentId::CustomVoice && processor.getUserVoice() != nullptr)
        role = processor.getUserVoice()->name.substring (0, 18) + "  |  " + processor.describeUserVoice().upToFirstOccurrenceOf (",", false, false);
    g.drawText (role, text, juce::Justification::topLeft);
}

void InstrumentRow::resized()
{
    auto r = getLocalBounds().reduced (6, 0);
    const int s = 26;
    favButton.setBounds (r.removeFromRight (s + 4).withSizeKeepingCentre (s, s));
    if (id == InstrumentId::CustomVoice)
    {
        settingsButton.setBounds (r.removeFromRight (s).withSizeKeepingCentre (s - 4, s - 4));
        loadButton.setBounds (r.removeFromRight (s).withSizeKeepingCentre (s - 4, s - 4));
    }
    regenButton.setBounds (r.removeFromRight (s).withSizeKeepingCentre (s - 2, s - 2));
    lockButton.setBounds (r.removeFromRight (s).withSizeKeepingCentre (s - 2, s - 2));
}

void InstrumentRow::mouseUp (const juce::MouseEvent& e)
{
    if (! e.mouseWasClicked() || attachment == nullptr)
        return;
    if (id == InstrumentId::CustomVoice && processor.getUserVoice() == nullptr && onLoadSample)
    {
        onLoadSample();   // nothing to play yet: ask for a sample first
        return;
    }
    attachment->setValueAsCompleteGesture (enabled ? 0.0f : 1.0f);
}

} // namespace jgl
