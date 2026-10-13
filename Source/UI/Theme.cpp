#include "Theme.h"

namespace jgl
{

juce::Font uiFont (float height, bool bold)
{
    return juce::Font (juce::FontOptions (height, bold ? juce::Font::bold : juce::Font::plain));
}

GrooveLookAndFeel::GrooveLookAndFeel()
{
    setColour (juce::ResizableWindow::backgroundColourId, Colours::background);
    setColour (juce::Label::textColourId, Colours::text);
    setColour (juce::TextButton::buttonColourId, Colours::panelRaised);
    setColour (juce::TextButton::buttonOnColourId, Colours::accent);
    setColour (juce::TextButton::textColourOffId, Colours::text);
    setColour (juce::TextButton::textColourOnId, juce::Colours::black);
    setColour (juce::ComboBox::backgroundColourId, Colours::panelRaised);
    setColour (juce::ComboBox::textColourId, Colours::text);
    setColour (juce::ComboBox::outlineColourId, Colours::outline);
    setColour (juce::ComboBox::arrowColourId, Colours::textDim);
    setColour (juce::PopupMenu::backgroundColourId, Colours::panelRaised);
    setColour (juce::PopupMenu::textColourId, Colours::text);
    setColour (juce::PopupMenu::headerTextColourId, Colours::accent);
    setColour (juce::PopupMenu::highlightedBackgroundColourId, Colours::accent.withAlpha (0.85f));
    setColour (juce::PopupMenu::highlightedTextColourId, juce::Colours::black);
    setColour (juce::Slider::textBoxTextColourId, Colours::text);
    setColour (juce::Slider::textBoxBackgroundColourId, juce::Colours::transparentBlack);
    setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
    setColour (juce::Slider::textBoxHighlightColourId, Colours::accent.withAlpha (0.4f));
    setColour (juce::TextEditor::backgroundColourId, Colours::panelRaised);
    setColour (juce::TextEditor::textColourId, Colours::text);
    setColour (juce::TextEditor::outlineColourId, Colours::outline);
    setColour (juce::TextEditor::focusedOutlineColourId, Colours::accent);
    setColour (juce::TextEditor::highlightColourId, Colours::accent.withAlpha (0.35f));
    setColour (juce::CaretComponent::caretColourId, Colours::accent);
    setColour (juce::AlertWindow::backgroundColourId, Colours::panelRaised);
    setColour (juce::AlertWindow::textColourId, Colours::text);
    setColour (juce::AlertWindow::outlineColourId, Colours::outline);
    setColour (juce::TooltipWindow::backgroundColourId, Colours::panelRaised);
    setColour (juce::TooltipWindow::textColourId, Colours::text);
    setColour (juce::TooltipWindow::outlineColourId, Colours::outline);
}

void GrooveLookAndFeel::drawRotarySlider (juce::Graphics& g, int x, int y, int w, int h, float pos,
                                          float startAngle, float endAngle, juce::Slider& s)
{
    const auto bounds = juce::Rectangle<int> (x, y, w, h).toFloat().reduced (4.0f);
    const float radius = juce::jmin (bounds.getWidth(), bounds.getHeight()) * 0.5f;
    const auto centre = bounds.getCentre();
    const float lineW = juce::jmax (2.5f, radius * 0.13f);
    const float arcR = radius - lineW * 0.5f;
    const bool bipolar = s.getMinimum() < 0.0 && s.getMaximum() > 0.0;

    juce::Path track;
    track.addCentredArc (centre.x, centre.y, arcR, arcR, 0.0f, startAngle, endAngle, true);
    g.setColour (Colours::outline);
    g.strokePath (track, juce::PathStrokeType (lineW, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

    const float angle = startAngle + pos * (endAngle - startAngle);
    const float from = bipolar ? (startAngle + endAngle) * 0.5f : startAngle;
    if (std::abs (angle - from) > 0.001f)
    {
        juce::Path value;
        value.addCentredArc (centre.x, centre.y, arcR, arcR, 0.0f, juce::jmin (from, angle), juce::jmax (from, angle), true);
        g.setColour (s.isEnabled() ? Colours::accent : Colours::textFaint);
        g.strokePath (value, juce::PathStrokeType (lineW, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    }

    const float knobR = arcR - lineW * 1.4f;
    g.setColour (Colours::panelRaised);
    g.fillEllipse (centre.x - knobR, centre.y - knobR, knobR * 2.0f, knobR * 2.0f);
    g.setColour (Colours::outline.brighter (0.1f));
    g.drawEllipse (centre.x - knobR, centre.y - knobR, knobR * 2.0f, knobR * 2.0f, 1.0f);

    const juce::Point<float> tip (centre.x + (knobR - 3.0f) * std::sin (angle), centre.y - (knobR - 3.0f) * std::cos (angle));
    const juce::Point<float> inner (centre.x + knobR * 0.35f * std::sin (angle), centre.y - knobR * 0.35f * std::cos (angle));
    g.setColour (Colours::text);
    g.drawLine ({ inner, tip }, 2.0f);
}

void GrooveLookAndFeel::drawLinearSlider (juce::Graphics& g, int x, int y, int w, int h, float pos, float, float,
                                          juce::Slider::SliderStyle style, juce::Slider& s)
{
    auto r = juce::Rectangle<int> (x, y, w, h).toFloat();

    if (style == juce::Slider::LinearBarVertical || style == juce::Slider::LinearBar)
    {
        g.setColour (Colours::panelRaised);
        g.fillRoundedRectangle (r, 5.0f);
        const float frac = (float) s.valueToProportionOfLength (s.getValue());
        g.setColour (Colours::accent.withAlpha (0.18f));
        g.fillRoundedRectangle (r.withWidth (r.getWidth() * frac), 5.0f);
        g.setColour (s.isMouseOverOrDragging() ? Colours::accent : Colours::outline);
        g.drawRoundedRectangle (r.reduced (0.5f), 5.0f, 1.0f);
        return;
    }

    // Horizontal level slider
    const float cy = r.getCentreY();
    const auto track = juce::Rectangle<float> (r.getX() + 4.0f, cy - 2.0f, r.getWidth() - 8.0f, 4.0f);
    g.setColour (Colours::outline);
    g.fillRoundedRectangle (track, 2.0f);
    g.setColour (Colours::accent);
    g.fillRoundedRectangle (track.withRight (pos), 2.0f);
    g.setColour (Colours::text);
    g.fillEllipse (pos - 6.0f, cy - 6.0f, 12.0f, 12.0f);
}

void GrooveLookAndFeel::drawButtonBackground (juce::Graphics& g, juce::Button& b, const juce::Colour&,
                                              bool highlighted, bool down)
{
    auto r = b.getLocalBounds().toFloat().reduced (0.5f);
    const bool on = b.getToggleState();
    const auto base = b.findColour (on ? juce::TextButton::buttonOnColourId : juce::TextButton::buttonColourId);

    auto fill = base;
    if (down)             fill = fill.brighter (0.15f);
    else if (highlighted) fill = fill.brighter (0.07f);
    if (! b.isEnabled())  fill = fill.withMultipliedAlpha (0.4f);

    g.setColour (fill);
    g.fillRoundedRectangle (r, 6.0f);
    g.setColour (on ? fill.brighter (0.2f) : (highlighted ? Colours::textFaint : Colours::outline));
    g.drawRoundedRectangle (r, 6.0f, 1.0f);
}

juce::Font GrooveLookAndFeel::getTextButtonFont (juce::TextButton&, int buttonHeight)
{
    return uiFont (juce::jmin (15.0f, (float) buttonHeight * 0.48f), true);
}

void GrooveLookAndFeel::drawToggleButton (juce::Graphics& g, juce::ToggleButton& b, bool highlighted, bool)
{
    auto r = b.getLocalBounds().toFloat();
    const bool on = b.getToggleState();
    const auto pill = r.removeFromLeft (30.0f).withSizeKeepingCentre (28.0f, 16.0f);

    g.setColour (on ? Colours::play : Colours::outline);
    g.fillRoundedRectangle (pill, 8.0f);
    g.setColour (on ? juce::Colours::black : Colours::textDim);
    const float knob = 12.0f;
    g.fillEllipse (on ? pill.getRight() - knob - 2.0f : pill.getX() + 2.0f, pill.getCentreY() - knob * 0.5f, knob, knob);

    g.setColour (highlighted ? Colours::text : Colours::textDim);
    g.setFont (uiFont (13.0f));
    g.drawText (b.getButtonText(), r.withTrimmedLeft (6.0f), juce::Justification::centredLeft);
}

void GrooveLookAndFeel::drawComboBox (juce::Graphics& g, int w, int h, bool, int, int, int, int, juce::ComboBox& box)
{
    auto r = juce::Rectangle<int> (0, 0, w, h).toFloat().reduced (0.5f);
    g.setColour (Colours::panelRaised);
    g.fillRoundedRectangle (r, 6.0f);
    g.setColour (box.isMouseOver (true) ? Colours::textFaint : Colours::outline);
    g.drawRoundedRectangle (r, 6.0f, 1.0f);

    juce::Path arrow;
    const float ax = (float) w - 16.0f, ay = (float) h * 0.5f;
    arrow.startNewSubPath (ax - 4.0f, ay - 2.0f);
    arrow.lineTo (ax, ay + 2.0f);
    arrow.lineTo (ax + 4.0f, ay - 2.0f);
    g.setColour (Colours::textDim);
    g.strokePath (arrow, juce::PathStrokeType (1.5f));
}

juce::Font GrooveLookAndFeel::getComboBoxFont (juce::ComboBox&) { return uiFont (14.0f); }

void GrooveLookAndFeel::positionComboBoxText (juce::ComboBox& box, juce::Label& label)
{
    label.setBounds (8, 1, box.getWidth() - 30, box.getHeight() - 2);
    label.setFont (getComboBoxFont (box));
}

juce::Font GrooveLookAndFeel::getPopupMenuFont() { return uiFont (14.0f); }
juce::Font GrooveLookAndFeel::getLabelFont (juce::Label& l) { return l.getFont(); }

void GrooveLookAndFeel::drawPopupMenuBackground (juce::Graphics& g, int w, int h)
{
    g.fillAll (Colours::panelRaised);
    g.setColour (Colours::outline);
    g.drawRect (0, 0, w, h);
}

juce::Label* GrooveLookAndFeel::createSliderTextBox (juce::Slider& s)
{
    auto* l = LookAndFeel_V4::createSliderTextBox (s);
    l->setFont (uiFont (s.getSliderStyle() == juce::Slider::LinearBarVertical ? 20.0f : 12.0f,
                        s.getSliderStyle() == juce::Slider::LinearBarVertical));
    l->setJustificationType (juce::Justification::centred);
    l->setColour (juce::Label::textColourId, Colours::text);
    l->setColour (juce::Label::outlineWhenEditingColourId, Colours::accent);
    l->setColour (juce::Label::outlineColourId, juce::Colours::transparentBlack);
    l->setColour (juce::Label::backgroundColourId, juce::Colours::transparentBlack);
    return l;
}

//==============================================================================
Knob::Knob (const juce::String& c) : caption (c)
{
    slider.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
    slider.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 60, 14);
    slider.setRotaryParameters (juce::MathConstants<float>::pi * 1.25f, juce::MathConstants<float>::pi * 2.75f, true);
    slider.setVelocityBasedMode (false);
    slider.setMouseDragSensitivity (220);
    addAndMakeVisible (slider);
}

void Knob::resized()
{
    auto r = getLocalBounds();
    r.removeFromTop (15);
    slider.setBounds (r.withSizeKeepingCentre (juce::jmin (r.getWidth(), r.getHeight() + 6), r.getHeight()));
}

void Knob::paint (juce::Graphics& g)
{
    g.setColour (Colours::textDim);
    g.setFont (uiFont (11.5f, true));
    g.drawText (caption.toUpperCase(), getLocalBounds().removeFromTop (16), juce::Justification::centred);
}

} // namespace jgl
