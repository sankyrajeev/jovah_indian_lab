#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "../Core/ParamIDs.h"

namespace jgl
{

juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();

const juce::StringArray& tempoPresets();

/** Sample rates offered for export; 0 = use the session / device rate. */
const std::vector<double>& exportSampleRates();
juce::String exportSampleRateName (double rate);

} // namespace jgl
