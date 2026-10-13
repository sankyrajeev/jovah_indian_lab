#include "WaveformView.h"

#include "../Plugin/PluginProcessor.h"
#include "Theme.h"

namespace jgl
{

bool startLoopFileDrag (GrooveLabProcessor& processor, juce::Component* source,
                        std::function<void (const juce::String&)> status)
{
    juce::File file;
    const auto res = processor.prepareDragFile (file);
    if (res.failed())
    {
        if (status) status (res.getErrorMessage());
        return false;
    }

    if (status) status ("Dragging " + file.getFileName() + "  (also saved in " + file.getParentDirectory().getFullPathName() + ")");

    const bool started = juce::DragAndDropContainer::performExternalDragDropOfFiles (
        juce::StringArray { file.getFullPathName() }, false, source,
        [status, name = file.getFileName()] { if (status) status ("Dropped " + name); });

    juce::Logger::writeToLog ("JGL: external drag of " + file.getFullPathName() + (started ? " started" : " could not start"));
    if (! started && status)
        status ("This host did not accept a file drag. The WAV is saved at " + file.getFullPathName());
    return started;
}

//==============================================================================
WaveformView::WaveformView (GrooveLabProcessor& p) : processor (p)
{
    setTooltip ("Drag the waveform into your DAW to drop the loop as a WAV file. Click to play / stop.");
    setMouseCursor (juce::MouseCursor::DraggingHandCursor);
}

void WaveformView::setResult (std::shared_ptr<const LoopResult> r) { result = std::move (r); repaint(); }

void WaveformView::setPlayhead (float f)
{
    if (std::abs (f - playhead) > 0.0005f)
    {
        playhead = f;
        repaint();
    }
}

void WaveformView::setBusy (bool b)
{
    if (b != busy) { busy = b; repaint(); }
}

void WaveformView::paint (juce::Graphics& g)
{
    auto r = getLocalBounds().toFloat();
    g.setColour (Colours::panel);
    g.fillRoundedRectangle (r, 8.0f);
    g.setColour (Colours::outline);
    g.drawRoundedRectangle (r.reduced (0.5f), 8.0f, 1.0f);

    auto area = r.reduced (10.0f, 8.0f);
    area.removeFromTop (14.0f);

    if (result == nullptr || result->peakMax.empty())
    {
        g.setColour (Colours::textFaint);
        g.setFont (uiFont (14.0f));
        g.drawText ("Rendering...", area, juce::Justification::centred);
        return;
    }

    // Bar / beat grid
    const int bars = result->arrangement.bars;
    const int beats = bars * 4;
    for (int b = 0; b <= beats; ++b)
    {
        const float x = area.getX() + area.getWidth() * (float) b / (float) beats;
        const bool barLine = b % 4 == 0;
        g.setColour (barLine ? Colours::outline.brighter (0.15f) : Colours::outline.withAlpha (0.5f));
        g.drawVerticalLine ((int) x, area.getY(), area.getBottom());
        if (barLine && b < beats)
        {
            g.setColour (Colours::textFaint);
            g.setFont (uiFont (10.0f, true));
            g.drawText (juce::String (b / 4 + 1), juce::Rectangle<float> (x + 3.0f, r.getY() + 4.0f, 30.0f, 12.0f),
                        juce::Justification::centredLeft);
        }
    }

    // Peaks
    const int n = (int) result->peakMax.size();
    const float mid = area.getCentreY();
    const float half = area.getHeight() * 0.5f;
    juce::Path wave;
    const int w = juce::jmax (1, (int) area.getWidth());
    for (int px = 0; px < w; ++px)
    {
        const int b0 = px * n / w;
        const int b1 = juce::jmax (b0 + 1, (px + 1) * n / w);
        float lo = 0.0f, hi = 0.0f;
        for (int b = b0; b < b1 && b < n; ++b)
        {
            lo = juce::jmin (lo, result->peakMin[(size_t) b]);
            hi = juce::jmax (hi, result->peakMax[(size_t) b]);
        }
        const float x = area.getX() + (float) px;
        wave.addRectangle (x, mid - hi * half, 1.0f, juce::jmax (1.0f, (hi - lo) * half));
    }

    const bool playing = playhead >= 0.0f;
    g.setColour ((playing ? Colours::accent : Colours::accent.withAlpha (0.75f)).withMultipliedAlpha (busy ? 0.5f : 1.0f));
    g.fillPath (wave);

    if (playing)
    {
        const float x = area.getX() + area.getWidth() * playhead;
        g.setColour (Colours::text);
        g.drawLine (x, area.getY() - 2.0f, x, area.getBottom() + 2.0f, 1.5f);
    }

    if (busy)
    {
        g.setColour (Colours::textDim);
        g.setFont (uiFont (11.0f, true));
        g.drawText ("RENDERING", r.reduced (10.0f, 4.0f).removeFromTop (12.0f), juce::Justification::centredRight);
    }
}

void WaveformView::mouseDown (const juce::MouseEvent&) { dragStarted = false; }

void WaveformView::mouseDrag (const juce::MouseEvent& e)
{
    if (! dragStarted && e.getDistanceFromDragStart() > 6)
    {
        dragStarted = true;
        startLoopFileDrag (processor, this, onStatus);
    }
}

void WaveformView::mouseUp (const juce::MouseEvent& e)
{
    if (! dragStarted && e.mouseWasClicked() && onClick)
        onClick();
    dragStarted = false;
}

//==============================================================================
DragHandle::DragHandle (GrooveLabProcessor& p) : processor (p)
{
    setTooltip ("Drag into Ableton Live (or any DAW / Finder) to drop the loop as a WAV file.");
    setMouseCursor (juce::MouseCursor::DraggingHandCursor);
}

void DragHandle::paint (juce::Graphics& g)
{
    auto r = getLocalBounds().toFloat().reduced (0.5f);
    const bool over = isMouseOver();
    g.setColour (over ? Colours::accent.withAlpha (0.18f) : Colours::panelRaised);
    g.fillRoundedRectangle (r, 8.0f);
    g.setColour (over ? Colours::accent : Colours::outline);
    g.drawRoundedRectangle (r, 8.0f, 1.2f);

    // Grip dots
    auto icon = r.removeFromTop (r.getHeight() * 0.55f);
    g.setColour (over ? Colours::accent : Colours::textDim);
    for (int row = 0; row < 3; ++row)
        for (int col = 0; col < 2; ++col)
            g.fillEllipse (icon.getCentreX() - 6.0f + col * 9.0f, icon.getCentreY() - 8.0f + row * 7.0f, 3.5f, 3.5f);

    g.setFont (uiFont (11.0f, true));
    g.drawFittedText ("DRAG\nTO DAW", r.toNearestInt(), juce::Justification::centredTop, 2);
}

void DragHandle::mouseDrag (const juce::MouseEvent& e)
{
    if (! dragStarted && e.getDistanceFromDragStart() > 4)
    {
        dragStarted = true;
        startLoopFileDrag (processor, this, onStatus);
    }
}

} // namespace jgl
