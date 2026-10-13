#include "WaveformView.h"

#include "../Plugin/PluginProcessor.h"
#include "Decor.h"
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

    if (status) status ("Dragging " + file.getFileName());

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
    setTooltip ("Drag the waveform into your DAW to drop the loop as a WAV file. Click to play or stop.");
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

juce::Rectangle<float> WaveformView::gripArea() const
{
    auto r = getLocalBounds().toFloat();
    return r.removeFromRight (56.0f);
}

void WaveformView::paint (juce::Graphics& g)
{
    auto r = getLocalBounds().toFloat();
    g.setColour (juce::Colours::black.withAlpha (0.12f));
    g.fillRoundedRectangle (r.translated (0.0f, 2.0f), 14.0f);
    g.setGradientFill (juce::ColourGradient (Colours::darkRaised, r.getX(), r.getY(), Colours::dark, r.getX(), r.getBottom(), false));
    g.fillRoundedRectangle (r, 14.0f);

    const auto grip = gripArea();
    decor::drawIcon (g, decor::Icon::Grip, grip.withSizeKeepingCentre (18.0f, 22.0f), Colours::paper.withAlpha (isMouseOver() ? 0.9f : 0.55f));

    auto area = r.withTrimmedLeft (34.0f).withTrimmedRight (grip.getWidth() + 6.0f).reduced (0.0f, 12.0f);
    area.removeFromTop (12.0f);

    if (result == nullptr || result->peakMax.empty())
    {
        g.setColour (Colours::paper.withAlpha (0.6f));
        g.setFont (Fonts::serif (17.0f));
        g.drawText ("Rendering...", area, juce::Justification::centred);
        return;
    }

    // Bar grid
    const int bars = result->arrangement.bars;
    for (int b = 0; b < bars; ++b)
    {
        const float x = area.getX() + area.getWidth() * (float) b / (float) bars;
        g.setColour (Colours::paper.withAlpha (0.5f));
        g.setFont (Fonts::serif (13.0f));
        g.drawText (juce::String (b + 1), juce::Rectangle<float> (x - 10.0f, r.getY() + 4.0f, 20.0f, 14.0f), juce::Justification::centred);
        g.setColour (Colours::paper.withAlpha (0.08f));
        g.drawVerticalLine ((int) x, area.getY(), area.getBottom());
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
        wave.addRectangle (area.getX() + (float) px, mid - hi * half, 1.0f, juce::jmax (1.0f, (hi - lo) * half));
    }
    g.setGradientFill (juce::ColourGradient (Colours::copperLight, area.getX(), area.getY(), Colours::wave, area.getX(), mid, false));
    if (busy) g.setOpacity (0.5f);
    g.fillPath (wave);

    if (playhead >= 0.0f)
    {
        const float x = area.getX() + area.getWidth() * playhead;
        g.setColour (Colours::copperLight);
        g.drawLine (x, area.getY() - 2.0f, x, area.getBottom() + 4.0f, 1.6f);
        juce::Path tri;
        tri.addTriangle (x - 5.0f, r.getY() + 5.0f, x + 5.0f, r.getY() + 5.0f, x, r.getY() + 12.0f);
        g.fillPath (tri);
    }

    if (busy)
    {
        g.setColour (Colours::paper.withAlpha (0.7f));
        g.setFont (Fonts::caps (10.0f));
        g.drawText ("RENDERING", r.reduced (16.0f, 6.0f).withTrimmedRight (grip.getWidth()).removeFromTop (14.0f), juce::Justification::centredRight);
    }
}

void WaveformView::mouseMove (const juce::MouseEvent&) { repaint (gripArea().toNearestInt()); }

void WaveformView::mouseDown (const juce::MouseEvent&) { dragStarted = false; }

void WaveformView::mouseDrag (const juce::MouseEvent& e)
{
    if (! dragStarted && e.getDistanceFromDragStart() > 5)
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

} // namespace jgl
