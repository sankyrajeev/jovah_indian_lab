#include "InstrumentTile.h"

#include "../Core/ParamIDs.h"
#include "../Plugin/PluginProcessor.h"
#include "Theme.h"

namespace jgl
{

IconButton::IconButton (Icon i, const juce::String& tip) : juce::Button (tip), icon (i)
{
    setTooltip (tip);
    setClickingTogglesState (i == Icon::Lock);
}

void IconButton::paintButton (juce::Graphics& g, bool highlighted, bool down)
{
    auto r = getLocalBounds().toFloat().reduced (3.0f);
    const bool on = getToggleState();

    auto colour = on ? Colours::accent : (highlighted ? Colours::text : Colours::textDim);
    if (! isEnabled()) colour = Colours::textFaint.withAlpha (0.5f);
    if (down) colour = colour.brighter (0.2f);
    g.setColour (colour);

    if (icon == Icon::Lock)
    {
        const float w = r.getWidth() * 0.62f;
        auto body = juce::Rectangle<float> (w, r.getHeight() * 0.45f).withCentre ({ r.getCentreX(), r.getY() + r.getHeight() * 0.7f });
        juce::Path shackle;
        const float sw = w * 0.62f;
        const float top = r.getY() + r.getHeight() * 0.12f;
        shackle.startNewSubPath (body.getCentreX() - sw * 0.5f, body.getY());
        shackle.lineTo (body.getCentreX() - sw * 0.5f, top + sw * 0.5f);
        shackle.addCentredArc (body.getCentreX(), top + sw * 0.5f, sw * 0.5f, sw * 0.5f, 0.0f,
                               -juce::MathConstants<float>::halfPi, juce::MathConstants<float>::halfPi);
        if (on) shackle.lineTo (body.getCentreX() + sw * 0.5f, body.getY());
        g.strokePath (shackle, juce::PathStrokeType (1.6f));
        if (on) g.fillRoundedRectangle (body, 1.5f);
        else    g.drawRoundedRectangle (body, 1.5f, 1.4f);
    }
    else
    {
        const auto c = r.getCentre();
        const float rad = r.getWidth() * 0.36f;
        juce::Path arc;
        arc.addCentredArc (c.x, c.y, rad, rad, 0.0f, 0.6f, juce::MathConstants<float>::twoPi - 0.2f, true);
        g.strokePath (arc, juce::PathStrokeType (1.6f));
        const float a = juce::MathConstants<float>::twoPi - 0.2f;
        const juce::Point<float> end (c.x + rad * std::sin (a), c.y - rad * std::cos (a));
        juce::Path head;
        head.addTriangle (end.x - 3.5f, end.y - 3.0f, end.x + 3.5f, end.y - 0.5f, end.x - 0.5f, end.y + 3.5f);
        g.fillPath (head);
    }
}

//==============================================================================
InstrumentTile::InstrumentTile (GrooveLabProcessor& p, InstrumentId i) : processor (p), id (i)
{
    const auto& info = instrumentInfo (id);
    setTooltip (juce::String (info.name) + ": " + info.role + ". Click to add or remove.");

    if (auto* param = processor.getState().getParameter (ParamIDs::instrument (id)))
    {
        attachment = std::make_unique<juce::ParameterAttachment> (*param, [this] (float v)
        {
            enabled = v > 0.5f;
            lockButton.setEnabled (enabled);
            regenButton.setEnabled (enabled);
            repaint();
        });
        attachment->sendInitialUpdate();
    }

    lockButton.onClick = [this] { processor.setLocked (id, lockButton.getToggleState()); };
    regenButton.onClick = [this] { processor.regeneratePart (id); };
    addAndMakeVisible (lockButton);
    addAndMakeVisible (regenButton);
    refresh();
}

void InstrumentTile::refresh()
{
    lockButton.setToggleState (processor.isLocked (id), juce::dontSendNotification);

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

void InstrumentTile::paint (juce::Graphics& g)
{
    const auto& info = instrumentInfo (id);
    auto r = getLocalBounds().toFloat().reduced (1.0f);
    const bool over = isMouseOver (true);

    g.setColour (enabled ? Colours::accent.withAlpha (0.12f) : Colours::panelRaised.withAlpha (over ? 1.0f : 0.7f));
    g.fillRoundedRectangle (r, 8.0f);
    g.setColour (enabled ? Colours::accent : (over ? Colours::textFaint : Colours::outline));
    g.drawRoundedRectangle (r, 8.0f, enabled ? 1.5f : 1.0f);

    auto text = getLocalBounds().reduced (12, 8);
    text.removeFromRight (26);
    g.setColour (enabled ? Colours::text : Colours::textDim);
    g.setFont (uiFont (15.5f, true));
    g.drawText (info.name, text.removeFromTop (20), juce::Justification::centredLeft);

    g.setColour (enabled ? Colours::textDim : Colours::textFaint);
    g.setFont (uiFont (11.5f));
    g.drawText (info.role, text.removeFromTop (16), juce::Justification::centredLeft);

    g.setFont (uiFont (9.5f, true));
    g.setColour (usesSamples ? Colours::play : Colours::textFaint);
    g.drawText (usesSamples ? "SAMPLES" : "MODELLED", text.removeFromTop (14), juce::Justification::centredLeft);
}

void InstrumentTile::resized()
{
    auto r = getLocalBounds().reduced (6);
    auto col = r.removeFromRight (24);
    lockButton.setBounds (col.removeFromTop (24));
    col.removeFromTop (juce::jmax (0, col.getHeight() - 24));
    regenButton.setBounds (col);
}

void InstrumentTile::mouseUp (const juce::MouseEvent& e)
{
    if (e.mouseWasClicked() && attachment != nullptr)
        attachment->setValueAsCompleteGesture (enabled ? 0.0f : 1.0f);
}

} // namespace jgl
