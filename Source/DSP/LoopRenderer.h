#pragma once

#include <atomic>
#include <juce_audio_basics/juce_audio_basics.h>

#include "../Core/MusicTypes.h"
#include "../Core/Settings.h"

namespace jgl
{

class SampleLibrary;

/**
    Offline renderer: Arrangement + RenderSettings -> exact-length stereo loop.

    Seamless looping is guaranteed by construction:
      - every hit/note is rendered into a circular buffer of exactly the loop length,
        so tails (and notes humanised before bar 1) wrap around to the start, just as
        they would when the loop repeats in the DAW;
      - stateful effects (reverb, delay, sympathetic strings, filter, saturation) are run
        over the loop several times to reach steady state, and only the final pass is kept.
    The result therefore contains no leading silence, no cut-off tails and no click at the seam.
*/
class LoopRenderer
{
public:
    struct Options
    {
        const SampleLibrary* samples = nullptr;
        const std::atomic<bool>* cancel = nullptr;
        bool normalise = true;
    };

    /** Returns false if cancelled or the settings are invalid. */
    static bool render (const Arrangement& arrangement, const RenderSettings& settings,
                        juce::AudioBuffer<float>& out, const Options& options);

    /** MIDI note of Sa for a melodic instrument in the given key. */
    static double tonicMidi (InstrumentId id, int keyRoot);

    /** Dayan (treble tabla) tuning: Sa of the key, kept within the drum's natural range. */
    static double tablaSaHz (int keyRoot);

    static constexpr float normaliseCeilingDb = -1.0f;
};

} // namespace jgl
