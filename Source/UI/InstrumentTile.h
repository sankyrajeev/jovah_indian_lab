#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "../Core/Instruments.h"

namespace jgl
{

class GrooveLabProcessor;

/** Small vector-icon button (lock / regenerate). */
class IconButton : public juce::Button
{
public:
    enum class Icon { Lock, Regenerate };
    IconButton (Icon icon, const juce::String& tooltip);
    void paintButton (juce::Graphics&, bool highlighted, bool down) override;

private:
    Icon icon;
};

/** Selectable instrument tile: click to enable, lock to keep its pattern, regenerate just this part. */
class InstrumentTile : public juce::Component, public juce::SettableTooltipClient
{
public:
    InstrumentTile (GrooveLabProcessor& p, InstrumentId id);

    void paint (juce::Graphics&) override;
    void resized() override;
    void mouseUp (const juce::MouseEvent&) override;
    void mouseEnter (const juce::MouseEvent&) override { repaint(); }
    void mouseExit (const juce::MouseEvent&) override { repaint(); }

    /** Called by the editor timer: lock state & sample source may change. */
    void refresh();

private:
    GrooveLabProcessor& processor;
    InstrumentId id;
    bool enabled = false;
    bool usesSamples = false;
    IconButton lockButton { IconButton::Icon::Lock, "Lock: keep this pattern when generating" };
    IconButton regenButton { IconButton::Icon::Regenerate, "Regenerate only this part" };
    std::unique_ptr<juce::ParameterAttachment> attachment;
};

} // namespace jgl
