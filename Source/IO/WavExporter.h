#pragma once

#include <juce_audio_formats/juce_audio_formats.h>

namespace jgl
{

enum class WavBitDepth
{
    Pcm24 = 0,   // default
    Pcm16,       // TPDF-dithered
    Float32
};

inline const juce::StringArray& bitDepthNames()
{
    static const juce::StringArray n { "24-bit", "16-bit", "32-bit float" };
    return n;
}

struct LoopFileInfo
{
    juce::String styleTag = "Custom";   // e.g. "AfroIndian"
    double bpm = 124.0;
    juce::String keyLabel;              // e.g. "Dmin"; empty for percussion-only loops
    int bars = 4;
};

/**
    Writes loops as stereo WAV files (24-bit PCM default, 16-bit dithered, 32-bit float).

    Files carry an ACID chunk (tempo, beat count, 4/4) so hosts that read it can
    recognise the loop's tempo; the duration itself is exactly bars * 4 beats.
*/
class WavExporter
{
public:
    static juce::Result write (const juce::AudioBuffer<float>& audio, double sampleRate, WavBitDepth depth,
                               const juce::File& file, double bpm = 0.0, int beats = 0, int rootMidi = -1);

    /** e.g. IndianGroove_AfroIndian_124BPM_Dmin_4Bars.wav */
    static juce::String makeFileName (const LoopFileInfo& info);

    /** `name` in `folder`, adding _2, _3 ... if a file already exists. */
    static juce::File uniqueFile (const juce::File& folder, const juce::String& fileName);
};

} // namespace jgl
