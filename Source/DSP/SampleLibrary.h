#pragma once

#include <algorithm>
#include <array>
#include <map>
#include <memory>

#include <juce_audio_formats/juce_audio_formats.h>
#include "../Core/Instruments.h"
#include "PitchCurve.h"

namespace jgl
{

/** Amplitude envelope for recorded melodic notes (Attack / Decay / Sustain / Release). */
struct NoteEnvelope
{
    double attackSec = 0.03;    // fade-in, softens the recording's own (tongued, plucked) attack
    double decaySec = 0.8;      // time to settle from full level to the sustain level
    double sustain = 0.9;       // held level, 0..1
    double releaseSec = 0.15;   // fade-out after the note ends

    /** Gain at time t of a note held for durationSec (attack and release are raised-cosine). */
    double gainAt (double t, double durationSec) const;
    double attack() const noexcept  { return std::max (0.002, attackSec); }    // never a hard edge
    double release() const noexcept { return std::max (0.005, releaseSec); }
};

/**
    Optional recorded multi-samples that replace the synthesized voices.

    Layout (inside AppPaths::samplesFolder()):

        <instrument key>/<Articulation>/<anything>[_v1.._v4].wav|aif|aiff|flac
        <instrument key>/notes/<anything>_<NoteName or MIDI number>.wav     (melodic)

    e.g.  tabla/Na/na_close_v2_rr1.wav,  dhol/Dagga/dagga_v3.wav,  sitar/notes/sitar_C#3.wav

    - _v1.._v4 mark velocity layers (v1 softest). Files without a layer tag share one layer.
    - Several files in the same layer become round-robins (never the same take twice in a row).
    - Melodic notes are pitch-shifted from the nearest sampled root (with glides).
    - Files are downmixed to mono, trimmed to 12 s and kept in memory.

    Thread-safety: a library is immutable after load(); the GenerationService swaps whole
    libraries via shared_ptr.
*/
class SampleLibrary
{
public:
    struct Sample
    {
        juce::AudioBuffer<float> data;   // mono
        double sampleRate = 48000.0;
        int layer = 0;
        double rootMidi = -1.0;     // fractional: detected tuning of the recording
        int loopStart = -1, loopEnd = -1;   // sustain loop (melodic), -1 = none
        juce::String name;
    };

    /** Scans `root` and loads all recognised files. Returns number of files loaded. */
    int load (const juce::File& root);

    bool hasArticulation (InstrumentId id, int articulation) const;
    bool hasMelodicNotes (InstrumentId id) const;

    /** Picks a take for a hit (velocity layer + round-robin by seed). */
    const Sample* pick (InstrumentId id, int articulation, float velocity, uint32_t seed) const;

    /** Picks the multi-sample whose root is closest to `midi`. */
    const Sample* pickNote (InstrumentId id, double midi, uint32_t seed) const;

    int numSamples() const noexcept { return total; }
    juce::StringArray describe() const;

    /** Renders a sample-based hit / note into `out` (mono), resampled to `sampleRate`. */
    static void renderHit (const Sample& s, float velocity, int numLayers, double sampleRate, std::vector<float>& out);
    /** Pitch-shifted playback following a pitch curve (glides, gamaka, falls, vibrato), looping
        the sustain region for notes longer than the recording, shaped by `envelope`. The output
        is durationSec + envelope release long and always starts and ends at silence. */
    static void renderNote (const Sample& s, double targetMidi, float velocity, double durationSec,
                            const PitchCurve& curve, const NoteEnvelope& envelope, double sampleRate,
                            std::vector<float>& out);

    /** Detects pitch and a sustain loop for a one-note recording (fills rootMidi if < 0). */
    static void analyseNote (Sample& s);

    /** Loads any audio file as a melodic voice: mono, trimmed, normalised and analysed. */
    static std::shared_ptr<Sample> loadVoice (const juce::File& file, juce::String& error);

    int numLayers (InstrumentId id, int articulation) const;

    /** Extracts a MIDI note from names like "sitar_C#3", "bansuri-60", "A2". -1 if none. */
    static int parseRootNote (const juce::String& fileNameWithoutExtension);

private:
    using Key = std::pair<int, int>;   // instrument, articulation (-1 = melodic notes)
    std::map<Key, std::vector<std::unique_ptr<Sample>>> samples;
    int total = 0;
};

} // namespace jgl
