#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "../Core/Instruments.h"
#include "Theme.h"
#include "Widgets.h"

namespace jgl
{

class GrooveLabProcessor;

/** Callout panel for recorded melodic sounds. For "Your Sample": what is loaded, the expression
    knobs and the envelope. For other instruments playing recorded notes: just the envelope. */
class VoicePanel : public juce::Component
{
public:
    VoicePanel (GrooveLabProcessor& p, InstrumentId instrument, std::function<void()> onLoad);
    void paint (juce::Graphics&) override;
    void resized() override;

private:
    Knob& addKnob (juce::OwnedArray<Knob>& into, const char* paramId, const char* caption, const char* tip);

    GrooveLabProcessor& processor;
    const InstrumentId instrument;
    const bool isVoice;
    juce::OwnedArray<Knob> expressionKnobs, envelopeKnobs;
    juce::OwnedArray<juce::AudioProcessorValueTreeState::SliderAttachment> attachments;
    IconTextButton loadButton { "LOAD SAMPLE", decor::Icon::Upload };
    juce::Rectangle<int> envelopeCaption;
};

} // namespace jgl
