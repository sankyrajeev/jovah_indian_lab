#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "../Engine/LoopResult.h"

namespace jgl
{

class GrooveLabProcessor;

/** Writes the current loop to disk and starts an OS-level file drag (into Ableton, Finder, ...). */
bool startLoopFileDrag (GrooveLabProcessor& processor, juce::Component* source,
                        std::function<void (const juce::String&)> status);

/**
    Dark waveform strip with bar numbers and a playhead. The whole strip (and the grip
    on its right) is a drag source for the loop's WAV file; a click toggles playback.
*/
class WaveformView : public juce::Component, public juce::SettableTooltipClient
{
public:
    explicit WaveformView (GrooveLabProcessor& p);

    void setResult (std::shared_ptr<const LoopResult> r);
    void setPlayhead (float fraction);
    void setBusy (bool b);

    std::function<void (const juce::String&)> onStatus;
    std::function<void()> onClick;

    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;
    void mouseMove (const juce::MouseEvent&) override;

private:
    juce::Rectangle<float> gripArea() const;

    GrooveLabProcessor& processor;
    std::shared_ptr<const LoopResult> result;
    float playhead = -1.0f;
    bool busy = false;
    bool dragStarted = false;
};

} // namespace jgl
