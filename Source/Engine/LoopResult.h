#pragma once

#include <juce_audio_basics/juce_audio_basics.h>

#include "../Core/MusicTypes.h"
#include "../Core/Settings.h"
#include "../DSP/SampleLibrary.h"

namespace jgl
{

/** A finished loop: composition, settings that produced it and the rendered audio. Immutable once published. */
struct LoopResult
{
    uint64_t id = 0;
    Arrangement arrangement;
    ComposeSettings compose;
    RenderSettings render;
    SeedSet seeds;
    std::shared_ptr<const SampleLibrary::Sample> userVoice;   // recording used by "Your Sample"

    juce::AudioBuffer<float> audio;          // exact loop length, stereo
    std::vector<float> peakMin, peakMax;     // waveform overview (per bucket, mono)
    double renderMilliseconds = 0.0;

    double sampleRate() const noexcept { return render.sampleRate; }
    double lengthBeats() const noexcept { return arrangement.lengthBeats(); }
    bool hasMelodic() const noexcept
    {
        for (auto& p : arrangement.parts)
            if (isMelodicPart (p.instrument))
                return true;
        return false;
    }

    static bool isMelodicPart (InstrumentId id) noexcept
    {
        return id == InstrumentId::Sitar || id == InstrumentId::Bansuri
            || id == InstrumentId::Tumbi || id == InstrumentId::Tanpura || id == InstrumentId::Nadaswaram || id == InstrumentId::CustomVoice;
    }

    void computePeaks (int buckets)
    {
        peakMin.assign ((size_t) buckets, 0.0f);
        peakMax.assign ((size_t) buckets, 0.0f);
        const int n = audio.getNumSamples();
        if (n == 0 || buckets <= 0) return;

        for (int b = 0; b < buckets; ++b)
        {
            const int s0 = (int) ((juce::int64) b * n / buckets);
            const int s1 = std::max (s0 + 1, (int) ((juce::int64) (b + 1) * n / buckets));
            float lo = 0.0f, hi = 0.0f;
            for (int c = 0; c < audio.getNumChannels(); ++c)
            {
                auto r = audio.findMinMax (c, s0, std::min (s1, n) - s0);
                lo = std::min (lo, r.getStart());
                hi = std::max (hi, r.getEnd());
            }
            peakMin[(size_t) b] = lo;
            peakMax[(size_t) b] = hi;
        }
    }
};

} // namespace jgl
