#include "Widgets.h"
#include "Theme.h"

namespace jgl
{

//==============================================================================
IconButton::IconButton (decor::Icon off, decor::Icon on, const juce::String& tooltip)
    : juce::Button (tooltip), offIcon (off), onIcon (on)
{
    setTooltip (tooltip);
}

void IconButton::paintButton (juce::Graphics& g, bool highlighted, bool down)
{
    auto r = getLocalBounds().toFloat();
    if (roundDark)
    {
        const float d = juce::jmin (r.getWidth(), r.getHeight()) - 2.0f;
        auto c = juce::Rectangle<float> (d, d).withCentre (r.getCentre());
        g.setColour (juce::Colours::black.withAlpha (0.18f));
        g.fillEllipse (c.translated (0.0f, 2.0f));
        g.setGradientFill (juce::ColourGradient (Colours::darkRaised.brighter (highlighted ? 0.25f : 0.12f), c.getX(), c.getY(),
                                                 Colours::dark, c.getX(), c.getBottom(), false));
        g.fillEllipse (c);
        decor::drawIcon (g, getToggleState() ? onIcon : offIcon, c.reduced (d * 0.3f), Colours::copperLight);
        return;
    }
    if (framed)
    {
        getLookAndFeel().drawButtonBackground (g, *this, {}, highlighted, down);
        r = r.reduced (r.getHeight() * 0.3f);
    }
    else
    {
        r = r.reduced (r.getHeight() * 0.18f);
    }

    auto c = getToggleState() ? onColour : colour;
    if (highlighted && ! getToggleState()) c = c.darker (0.4f);
    if (! isEnabled()) c = c.withAlpha (0.3f);
    decor::drawIcon (g, getToggleState() ? onIcon : offIcon, r.withSizeKeepingCentre (juce::jmin (r.getWidth(), r.getHeight()), juce::jmin (r.getWidth(), r.getHeight())), c);
}

//==============================================================================
IconTextButton::IconTextButton (const juce::String& caption, decor::Icon i) : juce::TextButton (caption), icon (i) {}

void IconTextButton::paintButton (juce::Graphics& g, bool highlighted, bool down)
{
    auto& lf = getLookAndFeel();
    lf.drawButtonBackground (g, *this, {}, highlighted, down);

    auto r = getLocalBounds().toFloat().reduced (12.0f, 0.0f);
    g.setFont (Fonts::caps (11.5f));
    const float textW = juce::GlyphArrangement::getStringWidth (g.getCurrentFont(), getButtonText());
    const float iconS = 15.0f, gap = 9.0f;
    const float total = iconS + gap + textW;
    const float x0 = r.getCentreX() - total * 0.5f;

    decor::drawIcon (g, icon, { x0, r.getCentreY() - iconS * 0.5f, iconS, iconS }, Colours::ink.withAlpha (isEnabled() ? 1.0f : 0.35f));
    g.setColour (Colours::ink.withAlpha (isEnabled() ? 1.0f : 0.35f));
    g.drawText (getButtonText(), juce::Rectangle<float> (x0 + iconS + gap, r.getY(), textW + 4.0f, r.getHeight()), juce::Justification::centredLeft);
}

//==============================================================================
GenerateButton::GenerateButton() : juce::Button ("Generate")
{
    setTooltip ("Generate a new loop. Locked parts are kept.");
    setMouseCursor (juce::MouseCursor::PointingHandCursor);
}

void GenerateButton::clicked()
{
    pulse = 1.0f;
    startTimerHz (60);
}

void GenerateButton::timerCallback()
{
    pulse *= 0.9f;
    if (pulse < 0.01f) { pulse = 0.0f; stopTimer(); }
    repaint();
}

void GenerateButton::paintButton (juce::Graphics& g, bool highlighted, bool down)
{
    auto bounds = getLocalBounds().toFloat();
    const float d = juce::jmin (bounds.getWidth() * 0.78f, bounds.getHeight() * 0.72f);
    const auto c = juce::Point<float> (bounds.getCentreX(), bounds.getY() + d * 0.5f + bounds.getHeight() * 0.04f);
    const float r = d * 0.5f;

    // Glow
    const float glow = 0.5f + (highlighted ? 0.2f : 0.0f) + pulse * 0.5f;
    for (int i = 8; i >= 1; --i)
    {
        const float rr = r * (1.0f + (float) i * 0.045f);
        g.setColour (Colours::copperLight.withAlpha (0.06f * glow));
        g.fillEllipse (c.x - rr, c.y - rr, rr * 2.0f, rr * 2.0f);
    }

    // Drips of light below the disc
    juce::Random rnd (3);
    for (int i = 0; i < 9; ++i)
    {
        const float x = c.x - r * 0.6f + r * 1.2f * (float) i / 8.0f;
        const float yTop = c.y + std::sqrt (juce::jmax (0.0f, r * r - (x - c.x) * (x - c.x))) - 2.0f;
        const float len = r * (0.16f + 0.3f * rnd.nextFloat());
        g.setColour (Colours::copper.withAlpha (0.55f));
        g.drawLine (x, yTop, x, yTop + len, 1.0f);
        g.fillEllipse (x - 2.0f, yTop + len, 4.0f, 4.0f);
    }

    // Disc
    const float rd = down ? r * 0.98f : r;
    juce::ColourGradient body (juce::Colour (0xfffff8ec), c.x, c.y - rd * 0.2f, juce::Colour (0xfff2c98c), c.x, c.y + rd, true);
    body.addColour (0.6, juce::Colour (0xfffbe6c4));
    g.setGradientFill (body);
    g.fillEllipse (c.x - rd, c.y - rd, rd * 2.0f, rd * 2.0f);
    g.setGradientFill (juce::ColourGradient (Colours::copperLight, c.x - rd, c.y - rd, Colours::copperDeep, c.x + rd, c.y + rd, false));
    g.drawEllipse (c.x - rd, c.y - rd, rd * 2.0f, rd * 2.0f, 2.5f);
    g.setColour (Colours::copper.withAlpha (0.35f));
    g.drawEllipse (c.x - rd * 0.92f, c.y - rd * 0.92f, rd * 1.84f, rd * 1.84f, 1.0f);

    // Lotus and caption
    auto lotusArea = juce::Rectangle<float> (rd * 0.5f, rd * 0.36f).withCentre ({ c.x, c.y - rd * 0.3f });
    g.setColour (Colours::copperDeep);
    g.strokePath (decor::lotus (lotusArea), juce::PathStrokeType (1.4f));
    g.setColour (Colours::ink);
    g.setFont (Fonts::caps (juce::jmax (14.0f, rd * 0.19f), 0.2f));
    g.drawText ("GENERATE", juce::Rectangle<float> (rd * 2.0f, rd * 0.4f).withCentre ({ c.x, c.y + rd * 0.12f }), juce::Justification::centred);
}

//==============================================================================
StyleDial::StyleDial()
{
    setMouseCursor (juce::MouseCursor::UpDownResizeCursor);
}

void StyleDial::step (int delta)
{
    const int next = (index + delta % numItems + numItems) % numItems;
    if (next != index)
    {
        index = next;
        repaint();
        if (onUserChange) onUserChange (index);
    }
}

void StyleDial::mouseDown (const juce::MouseEvent&) { dragStartIndex = index; dragged = false; }

void StyleDial::mouseDrag (const juce::MouseEvent& e)
{
    const int steps = (int) std::round ((e.getDistanceFromDragStartX() - e.getDistanceFromDragStartY()) / 40.0f);
    if (std::abs (e.getDistanceFromDragStartX()) + std::abs (e.getDistanceFromDragStartY()) > 6)
        dragged = true;
    const int target = juce::jlimit (0, numItems - 1, dragStartIndex + steps);
    if (target != index)
        step (target - index);
}

void StyleDial::mouseUp (const juce::MouseEvent& e)
{
    if (! dragged && e.mouseWasClicked() && onClick)
        onClick();
}

void StyleDial::mouseWheelMove (const juce::MouseEvent&, const juce::MouseWheelDetails& w)
{
    const float d = w.deltaY != 0.0f ? w.deltaY : w.deltaX;
    if (std::abs (d) > 0.01f)
        step (d > 0 ? -1 : 1);
}

void StyleDial::paint (juce::Graphics& g)
{
    const auto b = getLocalBounds().toFloat();
    const float R = juce::jmin (b.getWidth(), b.getHeight()) * 0.5f - 6.0f;
    const auto c = b.getCentre();

    // Drop shadow
    for (int i = 0; i < 6; ++i)
    {
        const float rr = R + (float) i * 2.0f;
        g.setColour (juce::Colours::black.withAlpha (0.035f));
        g.fillEllipse (c.x - rr, c.y - rr + 6.0f, rr * 2.0f, rr * 2.0f);
    }

    // Outer ring
    g.setGradientFill (juce::ColourGradient (juce::Colour (0xff4a3f36), c.x - R, c.y - R, juce::Colour (0xff15110e), c.x + R, c.y + R, false));
    g.fillEllipse (c.x - R, c.y - R, R * 2.0f, R * 2.0f);
    g.setColour (juce::Colour (0xff6d5d50).withAlpha (0.6f));
    g.drawEllipse (c.x - R + 1.0f, c.y - R + 1.0f, R * 2.0f - 2.0f, R * 2.0f - 2.0f, 1.0f);

    // Inner disc with a stone-like texture
    const float Ri = R * 0.84f;
    g.setGradientFill (juce::ColourGradient (juce::Colour (0xff332a24), c.x, c.y - Ri, juce::Colour (0xff1c1713), c.x, c.y + Ri, false));
    g.fillEllipse (c.x - Ri, c.y - Ri, Ri * 2.0f, Ri * 2.0f);
    juce::Random rnd (42);
    for (int i = 0; i < 900; ++i)
    {
        const float a = rnd.nextFloat() * juce::MathConstants<float>::twoPi;
        const float rr = std::sqrt (rnd.nextFloat()) * Ri * 0.98f;
        g.setColour ((rnd.nextBool() ? juce::Colours::white : juce::Colours::black).withAlpha (0.04f + rnd.nextFloat() * 0.05f));
        g.fillRect (c.x + std::cos (a) * rr, c.y + std::sin (a) * rr, 1.2f, 1.2f);
    }

    // Copper ring
    const float Rc = R * 0.8f;
    g.setGradientFill (juce::ColourGradient (Colours::copperLight, c.x - Rc, c.y - Rc, Colours::copperDeep, c.x + Rc, c.y + Rc, false));
    g.drawEllipse (c.x - Rc, c.y - Rc, Rc * 2.0f, Rc * 2.0f, 3.0f);
    g.setColour (Colours::copper.withAlpha (0.35f));
    g.drawEllipse (c.x - Rc * 0.95f, c.y - Rc * 0.95f, Rc * 1.9f, Rc * 1.9f, 1.0f);

    // Notches: one per style
    for (int i = 0; i < numItems; ++i)
    {
        const float a = juce::MathConstants<float>::pi * (-0.75f + 1.5f * (float) i / (float) juce::jmax (1, numItems - 1));
        const float rr = R * 0.92f;
        g.setColour (i == index ? Colours::copperLight : juce::Colour (0xff8a7868).withAlpha (0.6f));
        g.fillEllipse (c.x + rr * std::sin (a) - 1.8f, c.y - rr * std::cos (a) - 1.8f, 3.6f, 3.6f);
    }

    // Engraving: dotted circle and lotus
    const float Re = R * 0.46f;
    g.setColour (Colours::copper.withAlpha (0.55f));
    for (int i = 0; i < 8; ++i)
    {
        const float a = juce::MathConstants<float>::twoPi * (float) i / 8.0f;
        g.fillEllipse (c.x + Re * std::sin (a) - 2.5f, c.y - Re * std::cos (a) - 2.5f, 5.0f, 5.0f);
    }
    juce::Path dotted;
    dotted.addEllipse (c.x - Re, c.y - Re, Re * 2.0f, Re * 2.0f);
    juce::Path dashed;
    const float dashes[] = { 1.5f, 4.0f };
    juce::PathStrokeType (0.8f).createDashedStroke (dashed, dotted, dashes, 2);
    g.setColour (Colours::copper.withAlpha (0.35f));
    g.fillPath (dashed);
    g.setColour (Colours::copper.withAlpha (0.6f));
    g.strokePath (decor::lotus (juce::Rectangle<float> (R * 0.6f, R * 0.46f).withCentre (c)), juce::PathStrokeType (1.2f));

    // Indicator
    const float a = juce::MathConstants<float>::pi * (-0.75f + 1.5f * (float) index / (float) juce::jmax (1, numItems - 1));
    const juce::Point<float> dot (c.x + Rc * std::sin (a), c.y - Rc * std::cos (a));
    for (int i = 4; i >= 1; --i)
    {
        const float gr = 3.0f + (float) i * 2.5f;
        g.setColour (Colours::copperLight.withAlpha (0.12f));
        g.fillEllipse (dot.x - gr, dot.y - gr, gr * 2.0f, gr * 2.0f);
    }
    g.setColour (juce::Colours::white);
    g.fillEllipse (dot.x - 5.0f, dot.y - 5.0f, 10.0f, 10.0f);
}

} // namespace jgl
