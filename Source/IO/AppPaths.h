#pragma once

#include <juce_core/juce_core.h>

namespace jgl
{

/**
    Predictable on-disk locations (created on demand):

      ~/Music/Jovah Groove Lab/Exports   exported & dragged WAV files
      ~/Music/Jovah Groove Lab/Samples   optional recorded multi-samples (see SampleLibrary)
      ~/Music/Jovah Groove Lab/Presets   user presets / favourites (*.jglpreset)
      <user app data>/Jovah Groove Lab/  settings and log file
*/
struct AppPaths
{
    static juce::File root();
    static juce::File exportsFolder();
    static juce::File samplesFolder();
    static juce::File presetsFolder();
    static juce::File voicesFolder();     // copies of samples loaded into "Your Sample"
    static juce::File settingsFolder();
    static juce::File logFile();

    /** Lets tests and the CLI redirect everything into a temporary folder. */
    static void overrideRoot (const juce::File& newRoot);
};

} // namespace jgl
