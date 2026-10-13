#pragma once

#include <array>
#include "MusicTypes.h"

namespace jgl
{

/** Everything that affects *what* is played (the composition). */
struct ComposeSettings
{
    int bars = 4;
    int scaleIndex = 1;
    int styleIndex = 0;
    int frameworkIndex = 0;   // 0 = style default
    float complexity = 0.5f;
    float density = 0.5f;
    float energy = 0.6f;
    float variation = 0.5f;
    std::array<bool, (size_t) numInstruments> enabled {};

    // Expression of the sample-driven voice ("Your Sample")
    float voiceBend = 0.5f;      // meend glides between notes and scoops into notes
    float voiceShake = 0.35f;    // gamaka: oscillating ornaments on held notes
    float voiceFalls = 0.3f;     // pitch dives at phrase ends
    float voiceStutter = 0.15f;  // rhythmic re-triggers of a note
    float voiceOctave = 0.2f;    // octave leaps on phrase peaks

    bool operator== (const ComposeSettings& o) const noexcept
    {
        return bars == o.bars && scaleIndex == o.scaleIndex && styleIndex == o.styleIndex
            && frameworkIndex == o.frameworkIndex && complexity == o.complexity && density == o.density
            && energy == o.energy && variation == o.variation && enabled == o.enabled
            && voiceBend == o.voiceBend && voiceShake == o.voiceShake && voiceFalls == o.voiceFalls
            && voiceStutter == o.voiceStutter && voiceOctave == o.voiceOctave;
    }
    bool operator!= (const ComposeSettings& o) const noexcept { return ! (*this == o); }

    bool isEnabled (InstrumentId id) const noexcept { return enabled[(size_t) toIndex (id)]; }
    int numEnabled() const noexcept { int n = 0; for (auto e : enabled) n += e ? 1 : 0; return n; }
};

/** Everything that affects *how* it sounds (groove feel, mix, effects, format). */
struct RenderSettings
{
    double bpm = 124.0;
    int keyRoot = 2;          // 0 = C
    float swing = 0.3f;       // 0..1 -> 50%..75% 16th swing
    float groove = 0.5f;      // microtiming / velocity template depth
    float humanize = 0.4f;
    float ghost = 0.5f;       // ghost-note level
    float accent = 0.6f;      // accent strength
    float saturation = 0.1f;
    float reverb = 0.25f;
    float delay = 0.2f;
    int delayTime = 1;        // index into delayTimeNames()
    float filter = 0.0f;      // -1 = low-pass .. 0 = off .. +1 = high-pass
    float levelDb = 0.0f;     // master output level applied after normalisation
    float voiceVibrato = 0.3f; // vibrato depth of the sample-driven voice
    double sampleRate = 48000.0;

    bool operator== (const RenderSettings& o) const noexcept
    {
        return bpm == o.bpm && keyRoot == o.keyRoot && swing == o.swing && groove == o.groove
            && humanize == o.humanize && ghost == o.ghost && accent == o.accent
            && saturation == o.saturation && reverb == o.reverb && delay == o.delay
            && delayTime == o.delayTime && filter == o.filter && levelDb == o.levelDb && voiceVibrato == o.voiceVibrato
            && sampleRate == o.sampleRate;
    }
    bool operator!= (const RenderSettings& o) const noexcept { return ! (*this == o); }
};

inline const juce::StringArray& delayTimeNames()
{
    static const juce::StringArray n { "1/16", "1/8 dotted", "1/8", "1/4 triplet", "1/4", "1/4 dotted" };
    return n;
}

inline double delayTimeBeats (int index)
{
    static const double b[] = { 0.25, 0.75, 0.5, 2.0 / 3.0, 1.0, 1.5 };
    return b[juce::jlimit (0, 5, index)];
}

/** Exact loop length. 4/4, so bars * 4 beats. Rounded to the nearest sample. */
inline juce::int64 loopLengthSamples (int bars, double bpm, double sampleRate)
{
    const double seconds = bars * 4.0 * 60.0 / bpm;
    return (juce::int64) std::llround (seconds * sampleRate);
}

} // namespace jgl
