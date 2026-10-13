#pragma once

#include <juce_audio_formats/juce_audio_formats.h>
#include "../Core/Instruments.h"

namespace jgl
{

/**
    Turns raw recordings from a sample pack into the layout SampleLibrary expects.

    Percussion: a file may contain one hit or a whole take of repeated hits. Hits are found
    with an energy-flux onset detector, cut from just before each onset, trimmed at -60 dB and
    faded. Optionally all hits of an articulation are sorted by loudness and split into
    velocity layers (_v1 soft .. _vN hard). Relative levels are preserved (one gain for the
    whole set), so dynamics survive.

    Melodic: each file is one sustained note. The root is taken from the file name if present,
    otherwise estimated (YIN) and written into the name, e.g. nadaswaram/notes/nadaswaram_D4.wav.
*/
class SampleImporter
{
public:
    struct Options
    {
        InstrumentId instrument = InstrumentId::Tabla;
        juce::String articulation;       // percussion only, e.g. "Na"
        bool slice = true;               // percussion: split takes into hits
        int velocityLayers = 1;          // percussion: 1..4
        juce::File destinationRoot;      // e.g. AppPaths::samplesFolder()
    };

    struct Report
    {
        int filesRead = 0, samplesWritten = 0;
        juce::StringArray messages;
    };

    static Report run (const juce::Array<juce::File>& inputs, const Options& options);

    // Analysis building blocks (public for tests) -----------------------------------
    static std::vector<int> detectOnsets (const juce::AudioBuffer<float>& mono, double sampleRate);
    static int trimEnd (const juce::AudioBuffer<float>& mono, int start, int end, float thresholdDb = -60.0f);
    static double estimatePitchHz (const juce::AudioBuffer<float>& mono, double sampleRate);
    static juce::String noteName (int midi);
};

} // namespace jgl
