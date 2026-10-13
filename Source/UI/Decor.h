#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "../Core/MusicTypes.h"

namespace jgl::decor
{

/** Parchment background with fibre speckle and vignette (cached). */
void drawPaper (juce::Graphics&, juce::Rectangle<int> area);

/** Faint line-art floral sprig for page corners. */
void drawFloralCorner (juce::Graphics&, juce::Rectangle<float> area, bool flipX, bool flipY, juce::Colour colour);

/** Outline of a lotus flower fitting `area`. */
juce::Path lotus (juce::Rectangle<float> area);

/** Small four-petal ornament (used above/below the style dial). */
void drawFlowerOrnament (juce::Graphics&, juce::Point<float> centre, float size, juce::Colour colour);

/** Horizontal rule with a diamond in the middle / at the ends. */
void drawDiamond (juce::Graphics&, juce::Point<float> centre, float size, juce::Colour colour, bool filled);

/** Panel whose top edge is a scalloped Indian arch. */
juce::Path archPanel (juce::Rectangle<float> area, float archHeight);

/** Watercolour-style sunset with a temple gopuram, palms and water reflections. */
void drawTempleSunset (juce::Graphics&, juce::Rectangle<float> area);

/** Vector illustration of an instrument. */
void drawInstrument (juce::Graphics&, InstrumentId id, juce::Rectangle<float> area);

enum class Icon { Wave, Bars, Copy, Upload, Folder, Search, Heart, HeartFilled, LockOpen, LockClosed, Regenerate,
                  Play, Pause, Dots, Grip, ChevronLeft, ChevronRight, ChevronDown, ChevronUp, Sliders };

void drawIcon (juce::Graphics&, Icon icon, juce::Rectangle<float> area, juce::Colour colour);

} // namespace jgl::decor
