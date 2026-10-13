#pragma once

#include <juce_audio_basics/juce_audio_basics.h>

namespace jgl
{

/** Analysis of single-note recordings: pitch and a seamless sustain loop. */
struct SampleAnalysis
{
    /** Median YIN pitch (Hz) of the steady part of the note, 0 if none. */
    static double estimatePitchHz (const juce::AudioBuffer<float>& mono, double sampleRate, int startSample = -1, int endSample = -1);

    struct Loop
    {
        int start = -1, end = -1;   // samples; -1 = no loop
        double medianMidi = 0.0;    // fractional MIDI note of the steady region
        bool valid() const noexcept { return start >= 0 && end > start; }
    };

    /** Finds the longest steady region (stable level and pitch) and a loop inside it whose
        length is a whole number of pitch periods, starting and ending on rising zero crossings. */
    static Loop findSustainLoop (const juce::AudioBuffer<float>& mono, double sampleRate);

    static double hzToMidi (double hz) { return 69.0 + 12.0 * std::log2 (hz / 440.0); }
};

} // namespace jgl
