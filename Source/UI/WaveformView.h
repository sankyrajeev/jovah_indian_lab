#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "../Engine/LoopResult.h"

namespace jgl
{

class GrooveLabProcessor;

/** Writes the current loop to disk and starts an OS-level file drag (into Ableton, Finder, ...). */
bool startLoopFileDrag (GrooveLabProcessor& processor, juce::Component* source,
                        std::function<void (const juce::String&)> status);

/** Waveform overview with bar grid and playhead. The whole view is a drag source for the WAV. */
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

private:
    GrooveLabProcessor& processor;
    std::shared_ptr<const LoopResult> result;
    float playhead = -1.0f;
    bool busy = false;
    bool dragStarted = false;
};

/** Explicit "drag me" handle next to the waveform. */
class DragHandle : public juce::Component, public juce::SettableTooltipClient
{
public:
    explicit DragHandle (GrooveLabProcessor& p);
    std::function<void (const juce::String&)> onStatus;

    void paint (juce::Graphics&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override { dragStarted = false; }
    void mouseEnter (const juce::MouseEvent&) override { repaint(); }
    void mouseExit (const juce::MouseEvent&) override { repaint(); }

private:
    GrooveLabProcessor& processor;
    bool dragStarted = false;
};

} // namespace jgl
