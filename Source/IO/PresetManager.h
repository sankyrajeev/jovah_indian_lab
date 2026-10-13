#pragma once

#include <juce_data_structures/juce_data_structures.h>
#include "../Core/MusicTypes.h"

namespace jgl
{

/**
    Presets are plain ValueTrees:

        <JGLPreset name="..." factory="0" seeds="<SeedSet::toString()>">
          <P id="bpm" v="124"/>   (real-world parameter values, not normalised)
          ...
        </JGLPreset>

    Factory presets carry fixed seeds, so they always produce the same loop.
    User presets (favourites) store the current seeds too, so a saved loop is
    reproduced exactly. Stored as XML in AppPaths::presetsFolder() / *.jglpreset.
*/
class PresetManager
{
public:
    static const juce::Identifier presetType;

    static const std::vector<juce::ValueTree>& factoryPresets();

    static juce::StringArray userPresetNames();
    static juce::Result saveUserPreset (const juce::String& name, const juce::ValueTree& preset);
    static juce::ValueTree loadUserPreset (const juce::String& name);
    static bool deleteUserPreset (const juce::String& name);

    static juce::File fileFor (const juce::String& name);

    // Helpers for building/reading presets
    static void setValue (juce::ValueTree& preset, const juce::String& paramId, float value);
    static bool getValue (const juce::ValueTree& preset, const juce::String& paramId, float& value);
};

} // namespace jgl
