#include "Theme.h"
#include "BinaryData.h"

namespace jgl
{

namespace
{
    juce::Typeface::Ptr loadFace (const char* data, int size)
    {
        return juce::Typeface::createSystemTypefaceFor (data, (size_t) size);
    }

    juce::Typeface::Ptr serifFace (bool bold)
    {
        static juce::Typeface::Ptr medium = loadFace (BinaryData::CormorantGaramondMedium_ttf, BinaryData::CormorantGaramondMedium_ttfSize);
        static juce::Typeface::Ptr heavy  = loadFace (BinaryData::CormorantGaramondBold_ttf, BinaryData::CormorantGaramondBold_ttfSize);
        return bold ? heavy : medium;
    }

    juce::Typeface::Ptr capsFace()
    {
        static juce::Typeface::Ptr f = loadFace (BinaryData::CinzelMedium_ttf, BinaryData::CinzelMedium_ttfSize);
        return f;
    }

    juce::String style (const juce::Component& c) { return c.getProperties()["jglStyle"].toString(); }
}

juce::Font Fonts::serif (float height, bool bold)
{
    if (auto f = serifFace (bold))
        return juce::Font (juce::FontOptions (f).withHeight (height));
    return juce::Font (juce::FontOptions (height, bold ? juce::Font::bold : juce::Font::plain));
}

juce::Font Fonts::caps (float height, float tracking)
{
    if (auto f = capsFace())
        return juce::Font (juce::FontOptions (f).withHeight (height)).withExtraKerningFactor (tracking);
    return juce::Font (juce::FontOptions (height, juce::Font::bold)).withExtraKerningFactor (tracking);
}

//==============================================================================
GrooveLookAndFeel::GrooveLookAndFeel()
{
    using namespace juce;
    setColour (ResizableWindow::backgroundColourId, Colours::paper);
    setColour (Label::textColourId, Colours::ink);
    setColour (TextButton::buttonColourId, Colours::paperLight);
    setColour (TextButton::buttonOnColourId, Colours::copper);
    setColour (TextButton::textColourOffId, Colours::ink);
    setColour (TextButton::textColourOnId, juce::Colours::white);
    setColour (ComboBox::backgroundColourId, Colours::paperLight);
    setColour (ComboBox::textColourId, Colours::ink);
    setColour (ComboBox::outlineColourId, Colours::line);
    setColour (ComboBox::arrowColourId, Colours::ink);
    setColour (PopupMenu::backgroundColourId, Colours::paperLight);
    setColour (PopupMenu::textColourId, Colours::ink);
    setColour (PopupMenu::headerTextColourId, Colours::copperDeep);
    setColour (PopupMenu::highlightedBackgroundColourId, Colours::copper.withAlpha (0.9f));
    setColour (PopupMenu::highlightedTextColourId, juce::Colours::white);
    setColour (Slider::textBoxTextColourId, Colours::ink);
    setColour (Slider::textBoxBackgroundColourId, juce::Colours::transparentBlack);
    setColour (Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
    setColour (Slider::textBoxHighlightColourId, Colours::copper.withAlpha (0.3f));
    setColour (TextEditor::backgroundColourId, Colours::paperLight);
    setColour (TextEditor::textColourId, Colours::ink);
    setColour (TextEditor::outlineColourId, Colours::line);
    setColour (TextEditor::focusedOutlineColourId, Colours::copper);
    setColour (TextEditor::highlightColourId, Colours::copper.withAlpha (0.3f));
    setColour (CaretComponent::caretColourId, Colours::copperDeep);
    setColour (AlertWindow::backgroundColourId, Colours::paperLight);
    setColour (AlertWindow::textColourId, Colours::ink);
    setColour (AlertWindow::outlineColourId, Colours::line);
    setColour (TooltipWindow::backgroundColourId, Colours::dark);
    setColour (TooltipWindow::textColourId, Colours::paperLight);
    setColour (TooltipWindow::outlineColourId, Colours::dark);
    setColour (BubbleComponent::backgroundColourId, Colours::dark);
    setColour (ScrollBar::thumbColourId, Colours::copper.withAlpha (0.55f));
    setColour (ScrollBar::trackColourId, juce::Colours::transparentBlack);
    setColour (BubbleComponent::outlineColourId, Colours::dark);
}

void GrooveLookAndFeel::drawRotarySlider (juce::Graphics& g, int x, int y, int w, int h, float pos,
                                          float startAngle, float endAngle, juce::Slider& s)
{
    const auto bounds = juce::Rectangle<int> (x, y, w, h).toFloat().reduced (3.0f);
    const float radius = juce::jmin (bounds.getWidth(), bounds.getHeight()) * 0.5f;
    const auto c = bounds.getCentre();
    const float lineW = juce::jmax (2.5f, radius * 0.1f);
    const float arcR = radius - lineW * 0.5f;
    const bool bipolar = s.getMinimum() < 0.0 && s.getMaximum() > 0.0;

    juce::Path track;
    track.addCentredArc (c.x, c.y, arcR, arcR, 0.0f, startAngle, endAngle, true);
    g.setColour (Colours::ink.withAlpha (0.8f));
    g.strokePath (track, juce::PathStrokeType (lineW, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

    const float angle = startAngle + pos * (endAngle - startAngle);
    const float from = bipolar ? (startAngle + endAngle) * 0.5f : startAngle;
    if (std::abs (angle - from) > 0.001f)
    {
        juce::Path value;
        value.addCentredArc (c.x, c.y, arcR, arcR, 0.0f, juce::jmin (from, angle), juce::jmax (from, angle), true);
        g.setGradientFill (juce::ColourGradient (Colours::copperLight, c.x - radius, c.y + radius,
                                                 Colours::copper, c.x + radius, c.y - radius, false));
        g.strokePath (value, juce::PathStrokeType (lineW, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    }

    // Cream face with a soft shadow and a dark pointer dot.
    const float faceR = arcR - lineW * 1.3f;
    g.setColour (juce::Colours::black.withAlpha (0.12f));
    g.fillEllipse (c.x - faceR, c.y - faceR + 2.0f, faceR * 2.0f, faceR * 2.0f);
    g.setGradientFill (juce::ColourGradient (Colours::paperLight.brighter (0.05f), c.x, c.y - faceR,
                                             Colours::paper.darker (0.04f), c.x, c.y + faceR, false));
    g.fillEllipse (c.x - faceR, c.y - faceR, faceR * 2.0f, faceR * 2.0f);

    const float dotR = juce::jmax (2.5f, faceR * 0.16f);
    const float d = faceR - dotR * 2.2f;
    g.setColour (Colours::dark);
    g.fillEllipse (c.x + d * std::sin (angle) - dotR, c.y - d * std::cos (angle) - dotR, dotR * 2.0f, dotR * 2.0f);
}

void GrooveLookAndFeel::drawLinearSlider (juce::Graphics& g, int x, int y, int w, int h, float pos, float, float,
                                          juce::Slider::SliderStyle st, juce::Slider& s)
{
    auto r = juce::Rectangle<int> (x, y, w, h).toFloat();

    if (st == juce::Slider::LinearBarVertical || st == juce::Slider::LinearBar)
    {
        g.setColour (Colours::paperLight);
        g.fillRoundedRectangle (r.reduced (0.5f), 7.0f);
        g.setColour (s.isMouseOverOrDragging() ? Colours::copper : Colours::line);
        g.drawRoundedRectangle (r.reduced (0.5f), 7.0f, 1.0f);
        return;
    }

    const float cy = r.getCentreY();
    const auto track = juce::Rectangle<float> (r.getX() + 6.0f, cy - 1.5f, r.getWidth() - 12.0f, 3.0f);
    g.setColour (Colours::line);
    g.fillRoundedRectangle (track, 1.5f);
    g.setColour (Colours::copper);
    g.fillRoundedRectangle (track.withRight (pos), 1.5f);
    g.setColour (juce::Colours::black.withAlpha (0.2f));
    g.fillEllipse (pos - 8.0f, cy - 7.0f, 16.0f, 16.0f);
    g.setGradientFill (juce::ColourGradient (Colours::darkRaised.brighter (0.3f), pos, cy - 8.0f, Colours::dark, pos, cy + 8.0f, false));
    g.fillEllipse (pos - 8.0f, cy - 8.0f, 16.0f, 16.0f);
}

void GrooveLookAndFeel::drawButtonBackground (juce::Graphics& g, juce::Button& b, const juce::Colour&, bool highlighted, bool down)
{
    auto r = b.getLocalBounds().toFloat().reduced (1.0f);
    const auto kind = style (b);
    const bool on = b.getToggleState();

    if (kind == "segment")
    {
        if (on)
        {
            const float d = juce::jmin (r.getWidth(), r.getHeight()) - 2.0f;
            auto circle = juce::Rectangle<float> (d, d).withCentre (r.getCentre());
            g.setColour (Colours::copperDeep.withAlpha (0.35f));
            g.fillEllipse (circle.translated (0.0f, 1.5f));
            g.setGradientFill (juce::ColourGradient (Colours::copperLight, circle.getX(), circle.getY(),
                                                     Colours::copper, circle.getRight(), circle.getBottom(), false));
            g.fillEllipse (circle);
        }
        else if (highlighted)
        {
            g.setColour (Colours::copper.withAlpha (0.12f));
            g.fillEllipse (juce::Rectangle<float> (r.getHeight() - 4.0f, r.getHeight() - 4.0f).withCentre (r.getCentre()));
        }
        return;
    }

    if (kind == "flat")
    {
        if (highlighted || down)
        {
            g.setColour (Colours::copper.withAlpha (down ? 0.2f : 0.1f));
            g.fillRoundedRectangle (r, 6.0f);
        }
        return;
    }

    const float radius = kind == "round" ? r.getHeight() * 0.5f : 8.0f;
    auto fill = on ? Colours::copper : Colours::paperLight;
    if (down)             fill = fill.darker (0.05f);
    else if (highlighted) fill = fill.brighter (0.03f);

    g.setColour (juce::Colours::black.withAlpha (0.06f));
    g.fillRoundedRectangle (r.translated (0.0f, 1.5f), radius);
    g.setColour (fill);
    g.fillRoundedRectangle (r, radius);
    g.setColour (highlighted ? Colours::copper.withAlpha (0.7f) : Colours::line);
    g.drawRoundedRectangle (r, radius, 1.0f);
}

void GrooveLookAndFeel::drawButtonText (juce::Graphics& g, juce::TextButton& b, bool, bool)
{
    const auto kind = style (b);
    auto colour = b.getToggleState() ? juce::Colours::white : Colours::ink;
    if (! b.isEnabled()) colour = colour.withAlpha (0.35f);
    g.setColour (colour);

    if (kind == "segment")
        g.setFont (Fonts::serif (19.0f, b.getToggleState()));
    else if (kind == "symbol")
        g.setFont (Fonts::serif (24.0f));
    else
        g.setFont (Fonts::caps (12.0f));

    auto r = b.getLocalBounds();
    if (b.getProperties().contains ("iconSpace"))
        r.removeFromLeft ((int) b.getProperties()["iconSpace"]);
    g.drawFittedText (b.getButtonText(), r.reduced (4, 0), juce::Justification::centred, 1);
}

juce::Font GrooveLookAndFeel::getTextButtonFont (juce::TextButton&, int) { return Fonts::caps (12.0f); }

void GrooveLookAndFeel::drawComboBox (juce::Graphics& g, int w, int h, bool, int, int, int, int, juce::ComboBox& box)
{
    auto r = juce::Rectangle<int> (0, 0, w, h).toFloat().reduced (0.5f);
    g.setColour (Colours::paperLight);
    g.fillRoundedRectangle (r, 7.0f);
    g.setColour (box.isMouseOver (true) ? Colours::copper.withAlpha (0.7f) : Colours::line);
    g.drawRoundedRectangle (r, 7.0f, 1.0f);

    juce::Path arrow;
    const float ax = (float) w - 17.0f, ay = (float) h * 0.5f;
    arrow.startNewSubPath (ax - 4.5f, ay - 2.0f);
    arrow.lineTo (ax, ay + 2.5f);
    arrow.lineTo (ax + 4.5f, ay - 2.0f);
    g.setColour (Colours::ink);
    g.strokePath (arrow, juce::PathStrokeType (1.4f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
}

juce::Font GrooveLookAndFeel::getComboBoxFont (juce::ComboBox&) { return Fonts::serif (18.0f); }

void GrooveLookAndFeel::positionComboBoxText (juce::ComboBox& box, juce::Label& label)
{
    label.setBounds (10, 1, box.getWidth() - 32, box.getHeight() - 2);
    label.setFont (getComboBoxFont (box));
}

juce::Font GrooveLookAndFeel::getPopupMenuFont() { return Fonts::serif (18.0f); }

void GrooveLookAndFeel::drawPopupMenuBackground (juce::Graphics& g, int w, int h)
{
    g.fillAll (Colours::paperLight);
    g.setColour (Colours::line);
    g.drawRect (0, 0, w, h);
}

juce::Font GrooveLookAndFeel::getLabelFont (juce::Label& l) { return l.getFont(); }

juce::Label* GrooveLookAndFeel::createSliderTextBox (juce::Slider& s)
{
    auto* l = LookAndFeel_V4::createSliderTextBox (s);
    const bool tempo = s.getSliderStyle() == juce::Slider::LinearBarVertical;
    l->setFont (Fonts::serif (tempo ? 21.0f : 18.0f, tempo));
    l->setJustificationType (juce::Justification::centred);
    l->setColour (juce::Label::textColourId, Colours::ink);
    l->setColour (juce::Label::outlineColourId, juce::Colours::transparentBlack);
    l->setColour (juce::Label::backgroundColourId, juce::Colours::transparentBlack);
    l->setColour (juce::Label::outlineWhenEditingColourId, Colours::copper);
    return l;
}

//==============================================================================
Knob::Knob (const juce::String& c) : caption (c)
{
    slider.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
    slider.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
    slider.setRotaryParameters (juce::MathConstants<float>::pi * 1.25f, juce::MathConstants<float>::pi * 2.75f, true);
    slider.setMouseDragSensitivity (220);
    slider.setDoubleClickReturnValue (false, 0.0);
    slider.onValueChange = [this] { repaint(); };
    addAndMakeVisible (slider);
}

void Knob::resized()
{
    auto r = getLocalBounds();
    const int dial = juce::jmin (r.getWidth(), r.getHeight() - 38);
    slider.setBounds (r.removeFromTop (dial).withSizeKeepingCentre (dial, dial));
}

void Knob::paint (juce::Graphics& g)
{
    auto r = getLocalBounds();
    r.removeFromTop (slider.getBottom() + 2);
    g.setColour (Colours::ink);
    g.setFont (Fonts::caps (10.0f, 0.06f));
    g.drawText (caption.toUpperCase(), r.removeFromTop (15), juce::Justification::centred);
    g.setFont (Fonts::serif (19.0f, true));
    g.drawText (slider.getTextFromValue (slider.getValue()), r.removeFromTop (20), juce::Justification::centred);
}

} // namespace jgl
