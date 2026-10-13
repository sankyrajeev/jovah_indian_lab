#pragma once

#include <juce_data_structures/juce_data_structures.h>

namespace jgl
{

/** Small persistent per-user preferences (favourite instruments, export defaults ...). */
struct UserSettings
{
    static juce::PropertiesFile& get();

    static bool isFavourite (const juce::String& instrumentKey);
    static void setFavourite (const juce::String& instrumentKey, bool favourite);
};

} // namespace jgl
