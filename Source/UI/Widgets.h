#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "Decor.h"

namespace jgl
{

/** Icon-only button (heart, lock, regenerate, chevrons, dots ...). */
class IconButton : public juce::Button
{
public:
    IconButton (decor::Icon off, decor::Icon on, const juce::String& tooltip);
    void paintButton (juce::Graphics&, bool highlighted, bool down) override;
    juce::Colour colour { 0xff6e5a47 }, onColour { 0xffd4842a };
    bool framed = false;
    bool roundDark = false;   // dark disc with a copper glyph (transport)

private:
    decor::Icon offIcon, onIcon;
};

/** Pill button with a small icon to the left of its caption. */
class IconTextButton : public juce::TextButton
{
public:
    IconTextButton (const juce::String& caption, decor::Icon icon);
    void paintButton (juce::Graphics&, bool highlighted, bool down) override;

private:
    decor::Icon icon;
};

/** The large glowing lotus GENERATE button. */
class GenerateButton : public juce::Button, private juce::Timer
{
public:
    GenerateButton();
    void paintButton (juce::Graphics&, bool highlighted, bool down) override;
    void clicked() override;
    using juce::Button::clicked;

private:
    void timerCallback() override;
    float pulse = 0.0f;
};

/** Big engraved disc that selects the generation personality (drag, scroll or click). */
class StyleDial : public juce::Component, public juce::SettableTooltipClient
{
public:
    StyleDial();
    void setNumItems (int n) { numItems = juce::jmax (1, n); repaint(); }
    void setIndex (int i) { index = juce::jlimit (0, numItems - 1, i); repaint(); }
    int getIndex() const noexcept { return index; }

    std::function<void (int)> onUserChange;
    std::function<void()> onClick;

    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;
    void mouseWheelMove (const juce::MouseEvent&, const juce::MouseWheelDetails&) override;

private:
    void step (int delta);
    int numItems = 9, index = 0, dragStartIndex = 0;
    bool dragged = false;
};

} // namespace jgl
