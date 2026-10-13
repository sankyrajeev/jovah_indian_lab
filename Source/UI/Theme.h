#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

namespace jgl
{

namespace Colours
{
    inline const juce::Colour background   { 0xff0e0f12 };
    inline const juce::Colour panel        { 0xff16181d };
    inline const juce::Colour panelRaised  { 0xff1d2026 };
    inline const juce::Colour outline      { 0xff2a2e36 };
    inline const juce::Colour text         { 0xffe9e6e1 };
    inline const juce::Colour textDim      { 0xff8d929b };
    inline const juce::Colour textFaint    { 0xff5d626b };
    inline const juce::Colour accent       { 0xfff2a33a };   // saffron
    inline const juce::Colour accentDeep   { 0xffc9761c };
    inline const juce::Colour play         { 0xff3fc1b0 };   // teal
    inline const juce::Colour warning      { 0xffe5484d };
}

juce::Font uiFont (float height, bool bold = false);

/** Dark, minimal look shared by every control. */
class GrooveLookAndFeel : public juce::LookAndFeel_V4
{
public:
    GrooveLookAndFeel();

    void drawRotarySlider (juce::Graphics&, int x, int y, int w, int h, float pos,
                           float startAngle, float endAngle, juce::Slider&) override;

    void drawLinearSlider (juce::Graphics&, int x, int y, int w, int h, float pos, float minPos, float maxPos,
                           juce::Slider::SliderStyle, juce::Slider&) override;

    void drawButtonBackground (juce::Graphics&, juce::Button&, const juce::Colour&,
                               bool highlighted, bool down) override;
    juce::Font getTextButtonFont (juce::TextButton&, int buttonHeight) override;

    void drawToggleButton (juce::Graphics&, juce::ToggleButton&, bool highlighted, bool down) override;

    void drawComboBox (juce::Graphics&, int w, int h, bool down, int bx, int by, int bw, int bh, juce::ComboBox&) override;
    juce::Font getComboBoxFont (juce::ComboBox&) override;
    void positionComboBoxText (juce::ComboBox&, juce::Label&) override;

    juce::Font getPopupMenuFont() override;
    juce::Font getLabelFont (juce::Label&) override;
    void drawPopupMenuBackground (juce::Graphics&, int w, int h) override;

    juce::Label* createSliderTextBox (juce::Slider&) override;
};

/** Rotary knob with a caption and value read-out. */
class Knob : public juce::Component
{
public:
    Knob (const juce::String& caption);
    juce::Slider slider;
    void resized() override;
    void paint (juce::Graphics&) override;

private:
    juce::String caption;
};

} // namespace jgl
