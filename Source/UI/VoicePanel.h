#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "Theme.h"
#include "Widgets.h"

namespace jgl
{

class GrooveLabProcessor;

/** Callout panel for "Your Sample": what is loaded, and the expression knobs. */
class VoicePanel : public juce::Component
{
public:
    VoicePanel (GrooveLabProcessor& p, std::function<void()> onLoad);
    void paint (juce::Graphics&) override;
    void resized() override;

private:
    GrooveLabProcessor& processor;
    juce::OwnedArray<Knob> knobs;
    juce::OwnedArray<juce::AudioProcessorValueTreeState::SliderAttachment> attachments;
    IconTextButton loadButton { "LOAD SAMPLE", decor::Icon::Upload };
};

} // namespace jgl
