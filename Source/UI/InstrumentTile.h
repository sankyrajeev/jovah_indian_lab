#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "../Core/Instruments.h"
#include "Widgets.h"

namespace jgl
{

class GrooveLabProcessor;

/**
    One row of the ensemble list: illustration, name, role and a favourite heart.
    Click the row to add/remove the instrument. When it is in the ensemble, a lock
    (keep this pattern) and a regenerate button appear.
*/
class InstrumentRow : public juce::Component, public juce::SettableTooltipClient
{
public:
    InstrumentRow (GrooveLabProcessor& p, InstrumentId id);

    void paint (juce::Graphics&) override;
    void resized() override;
    void mouseUp (const juce::MouseEvent&) override;
    void mouseEnter (const juce::MouseEvent&) override { updateButtons(); repaint(); }
    void mouseExit (const juce::MouseEvent&) override { updateButtons(); repaint(); }

    void refresh();
    InstrumentId getInstrument() const noexcept { return id; }
    bool isFavourite() const noexcept { return favButton.getToggleState(); }
    std::function<void()> onFavouriteChanged;
    std::function<void()> onLoadSample, onVoiceSettings;   // "Your Sample" row only

private:
    void updateButtons();

    GrooveLabProcessor& processor;
    InstrumentId id;
    bool enabled = false;
    bool usesSamples = false;
    IconButton favButton { decor::Icon::Heart, decor::Icon::HeartFilled, "Favourite" };
    IconButton lockButton { decor::Icon::LockOpen, decor::Icon::LockClosed, "Lock: keep this part when generating" };
    IconButton regenButton { decor::Icon::Regenerate, decor::Icon::Regenerate, "Regenerate only this part" };
    IconButton loadButton { decor::Icon::Upload, decor::Icon::Upload, "Load a single-note sample (or drag a WAV onto the plugin)" };
    IconButton settingsButton { decor::Icon::Sliders, decor::Icon::Sliders, "Expression: bend, shake, falls, stutter, octave, vibrato" };
    std::unique_ptr<juce::ParameterAttachment> attachment;
};

} // namespace jgl
