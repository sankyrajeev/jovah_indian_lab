#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

namespace jgl
{

/** Warm parchment palette (see docs/ui for the reference mock-up). */
namespace Colours
{
    inline const juce::Colour paper       { 0xffede3d1 };
    inline const juce::Colour paperLight  { 0xfff6efe3 };
    inline const juce::Colour panel       { 0xfff3ebdd };
    inline const juce::Colour line        { 0xffdccab0 };
    inline const juce::Colour lineSoft    { 0xffe7dac6 };
    inline const juce::Colour ink         { 0xff33251a };
    inline const juce::Colour inkSoft     { 0xff6e5a47 };
    inline const juce::Colour inkFaint    { 0xffa38c74 };
    inline const juce::Colour copper      { 0xffd4842a };
    inline const juce::Colour copperDeep  { 0xffa9601b };
    inline const juce::Colour copperLight { 0xfff3b866 };
    inline const juce::Colour dark        { 0xff231c17 };
    inline const juce::Colour darkRaised  { 0xff2f2620 };
    inline const juce::Colour wave        { 0xffe8962e };
}

namespace Fonts
{
    /** Cormorant Garamond (SIL OFL), bundled. */
    juce::Font serif (float height, bool bold = false);
    /** Cinzel small capitals (SIL OFL), bundled, letter-spaced, for labels. */
    juce::Font caps (float height, float tracking = 0.18f);
}

/** Kept for older call sites: maps to the serif face. */
inline juce::Font uiFont (float height, bool bold = false) { return Fonts::serif (height + 2.0f, bold); }

/** Light, ornamental look shared by every control. */
class GrooveLookAndFeel : public juce::LookAndFeel_V4
{
public:
    GrooveLookAndFeel();

    void drawRotarySlider (juce::Graphics&, int x, int y, int w, int h, float pos,
                           float startAngle, float endAngle, juce::Slider&) override;
    void drawLinearSlider (juce::Graphics&, int x, int y, int w, int h, float pos, float minPos, float maxPos,
                           juce::Slider::SliderStyle, juce::Slider&) override;

    void drawButtonBackground (juce::Graphics&, juce::Button&, const juce::Colour&, bool highlighted, bool down) override;
    void drawButtonText (juce::Graphics&, juce::TextButton&, bool highlighted, bool down) override;
    juce::Font getTextButtonFont (juce::TextButton&, int buttonHeight) override;

    void drawComboBox (juce::Graphics&, int w, int h, bool down, int bx, int by, int bw, int bh, juce::ComboBox&) override;
    juce::Font getComboBoxFont (juce::ComboBox&) override;
    void positionComboBoxText (juce::ComboBox&, juce::Label&) override;

    juce::Font getPopupMenuFont() override;
    void drawPopupMenuBackground (juce::Graphics&, int w, int h) override;
    juce::Font getLabelFont (juce::Label&) override;
    juce::Label* createSliderTextBox (juce::Slider&) override;
    juce::Font getAlertWindowMessageFont() override { return Fonts::serif (17.0f); }
    juce::Font getAlertWindowTitleFont() override { return Fonts::serif (22.0f, true); }
};

/** Rotary knob: dial on top, caption in small capitals, value in serif below. */
class Knob : public juce::Component
{
public:
    explicit Knob (const juce::String& caption);
    juce::Slider slider;
    void resized() override;
    void paint (juce::Graphics&) override;

private:
    juce::String caption;
};

} // namespace jgl
